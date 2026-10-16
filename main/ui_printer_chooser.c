#include "ui_printer_chooser.h"
#include "ui_text.h"
#include "ui_value_update.h"
#include "ui_text_fit.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_log.h"

#include "moonraker.h"
#include "moonraker_config_controller.h"
#include "moonraker_probe.h"
#include "printer_profile_health.h"
#include "printer_connection_policy.h"
#include "moonraker_live_websocket.h"
#include "printer_preview_cache.h"
#include "ui_button.h"
#include "ui_theme.h"
#include "ui_page_geometry.h"

typedef struct {
    lv_obj_t *root;
    lv_obj_t *name;
    lv_obj_t *endpoint;
    lv_obj_t *status;
    lv_subject_t name_subject;
    lv_subject_t endpoint_subject;
    char name_value[MOONRAKER_CONFIG_NAME_LENGTH];
    char name_previous[MOONRAKER_CONFIG_NAME_LENGTH];
    char endpoint_value[MOONRAKER_CONFIG_HOST_LENGTH + 16];
    char endpoint_previous[MOONRAKER_CONFIG_HOST_LENGTH + 16];
    /* LVGL DOT mode edits its visible label buffer. Remember that representation
     * separately from the full subject so unchanged polls do not repair dots. */
    char name_rendered[MOONRAKER_CONFIG_NAME_LENGTH];
    char endpoint_rendered[MOONRAKER_CONFIG_HOST_LENGTH + 16];
    bool name_bound;
    bool endpoint_bound;
    lv_subject_t status_subject;
    char status_value[64];
    char status_previous[64];
    bool status_bound;
    bool subjects_ready;
    lv_obj_t *preview_box;
    lv_obj_t *preview_icon;
    lv_obj_t *preview_image;
    uint32_t preview_revision;
    lv_obj_t *active;
    lv_obj_t *hint;
} chooser_card_t;

static lv_obj_t *s_root = NULL;
static lv_timer_t *s_timer = NULL;
static chooser_card_t s_cards[MOONRAKER_CONFIG_MAX_PROFILES];

static ui_printer_chooser_select_cb_t s_select_cb = NULL;
static ui_printer_chooser_manage_cb_t s_manage_cb = NULL;
static lv_obj_t *s_manage_action;
static uint32_t s_chooser_epoch;
static bool s_chooser_dispatching,s_refreshing,s_flushing,s_show_pending,s_flush_queued;
static unsigned s_subject_depth,s_retiring;
static void chooser_deferred_show(void *unused);
static void chooser_cancel_deferred(void)
{
    if(s_flush_queued){lv_async_call_cancel(chooser_deferred_show,NULL);s_flush_queued=false;}
}
static ui_printer_chooser_select_cb_t s_pending_select;
static ui_printer_chooser_manage_cb_t s_pending_manage;
static void chooser_flush_show(void);
typedef struct {lv_obj_t *root;uint32_t epoch,generation;int active;} chooser_context_t;
static bool chooser_current(const chooser_context_t *context)
{
    return context->root && context->root==s_root && context->epoch==s_chooser_epoch &&
        context->generation==moonraker_config_generation() && context->active==moonraker_config_active_profile_index();
}
static void chooser_hidden(lv_obj_t *object,bool hidden)
{
    if(!object)return;
    if(hidden)lv_obj_add_flag(object,LV_OBJ_FLAG_HIDDEN);else lv_obj_clear_flag(object,LV_OBJ_FLAG_HIDDEN);
}
static void card_retire(chooser_card_t *card)
{
    /* Keep subject storage intact until an in-flight notification unwinds. */
    bool ready=card->subjects_ready;
    card->subjects_ready=card->name_bound=card->endpoint_bound=card->status_bound=false;
    card->root=card->name=card->endpoint=card->status=card->preview_box=NULL;
    card->preview_icon=card->preview_image=card->active=card->hint=NULL;card->preview_revision=0;
    if(ready){lv_subject_deinit(&card->name_subject);lv_subject_deinit(&card->endpoint_subject);lv_subject_deinit(&card->status_subject);}
}
static void chooser_retire(void)
{
    lv_timer_t *timer=s_timer;s_timer=NULL;
    ++s_chooser_epoch;s_root=s_manage_action=NULL;s_select_cb=NULL;s_manage_cb=NULL;
    if(timer)lv_timer_delete(timer);
    ++s_retiring;
    for(int i=0;i<MOONRAKER_CONFIG_MAX_PROFILES;++i)card_retire(&s_cards[i]);
    --s_retiring;
}
static void chooser_root_deleted(lv_event_t *event)
{
    if(lv_event_get_target_obj(event)!=s_root)return;
    chooser_retire();chooser_flush_show();
}
static void chooser_field_deleted(lv_event_t *event)
{
    lv_obj_t *object=lv_event_get_target_obj(event);bool retired=false;
    if(object==s_manage_action){s_manage_action=NULL;retired=true;}
    for(int i=0;i<MOONRAKER_CONFIG_MAX_PROFILES;++i){
        chooser_card_t *card=&s_cards[i];
        lv_obj_t **fields[]={&card->name,&card->endpoint,&card->status,&card->preview_box,
            &card->preview_icon,&card->preview_image,&card->active,&card->hint};
        if(object==card->name)card->name_bound=false;
        if(object==card->endpoint)card->endpoint_bound=false;
        if(object==card->status)card->status_bound=false;
        if(object==card->preview_image)card->preview_revision=0;
        for(unsigned j=0;j<sizeof(fields)/sizeof(fields[0]);++j)if(*fields[j]==object){*fields[j]=NULL;retired=true;}
    }
    if(retired)++s_chooser_epoch;
}
static void chooser_watch(lv_obj_t *object)
{
    if(object)lv_obj_add_event_cb(object,chooser_field_deleted,LV_EVENT_DELETE,NULL);
}


static void card_subjects_delete_cb(lv_event_t *event)
{
    chooser_card_t *card=lv_event_get_user_data(event);
    if(!card || lv_event_get_target_obj(event)!=card->root ||
        lv_event_get_current_target_obj(event)!=card->root)return;
    ++s_chooser_epoch;++s_retiring;card_retire(card);--s_retiring;chooser_flush_show();
}

static void set_card_text(lv_obj_t *label, lv_subject_t *subject,
                          size_t capacity, bool bound, char *rendered, const char *text)
{
    if (!label) return;
    if (!text) text = "";
    if (!bound || strlen(text) >= capacity) {
        ui_value_set_text(label, text);
        return;
    }
    bool unchanged = strcmp(lv_subject_get_string(subject), text) == 0;
    uint32_t epoch=s_chooser_epoch;
    ++s_subject_depth;lv_subject_copy_string(subject,text);--s_subject_depth;
    if(unchanged && epoch==s_chooser_epoch &&
        (!rendered || strcmp(rendered,lv_label_get_text(label))!=0))
        ui_value_set_text(label,text);
    if(rendered && epoch==s_chooser_epoch) {
        lv_obj_update_layout(label);
        if(epoch==s_chooser_epoch)
            snprintf(rendered,capacity,"%s",lv_label_get_text(label));
    }
    chooser_flush_show();
}

static void set_status_text_font(chooser_card_t *card, const char *text, const lv_font_t *font)
{
    if (!card) return;
    uint32_t epoch=s_chooser_epoch;
    bool changed = card->status && strcmp(lv_label_get_text(card->status), text ? text : "") != 0;
    set_card_text(card->status, &card->status_subject,
        sizeof(card->status_value), card->status_bound, NULL, text);
    if(changed && epoch==s_chooser_epoch)ui_text_fit_single_line(card->status,font);
}

static void set_status_text(chooser_card_t *card, const char *text)
{
    set_status_text_font(card,text,UI_FONT_BODY_LARGE);
}

/* Display wording only: health, freshness and command policy use the raw state. */
static const char *chooser_display_status(const char *raw)
{
    if(!raw)return "";
    static const struct {const char *raw,*display;} names[] = {
        {"printing","Printing"}, {"paused","Paused"}, {"ready","Ready"},
        {"standby","Standby"}, {"complete","Complete"}, {"cancelled","Cancelled"},
        {"error","Error"}, {"shutdown","Shutdown"},
        {"CHECKING...","Checking..."}, {"CONNECTING...","Connecting..."},
        {"VERIFYING...","Verifying..."}, {"OFFLINE / RETRYING","Offline / retrying"},
        {"KLIPPER SHUTDOWN","Klipper shutdown"}, {"WAITING FOR KLIPPER","Waiting for Klipper"},
        {"ONLINE","Online"}, {"OFFLINE","Offline"}
    };
    for(unsigned i=0;i<sizeof(names)/sizeof(names[0]);++i)
        if(!strcmp(raw,names[i].raw))return ui_text(names[i].display);
    return raw;
}

static void set_name_text(chooser_card_t *card, const char *text)
{
    if (!card) return;
    set_card_text(card->name, &card->name_subject,
        sizeof(card->name_value), card->name_bound, card->name_rendered, text);
}

static void set_endpoint_text(chooser_card_t *card, const char *text)
{
    if (!card) return;
    set_card_text(card->endpoint, &card->endpoint_subject,
        sizeof(card->endpoint_value), card->endpoint_bound, card->endpoint_rendered, text);
}

static void apply_status_style(lv_obj_t *label, bool configured, bool online)
{
    ui_value_set_color(label, !configured ? UI_TEXT_DIM :
        online ? UI_OK_BRIGHT :
        ui_theme_get_active() == UI_THEME_CLASSIC ? UI_TEXT_ERROR : UI_DANGER_BRIGHT, 0);
}


static bool chooser_action(lv_event_t *event,lv_obj_t *button)
{
    return s_root && !s_chooser_dispatching && !s_retiring && button && !lv_obj_has_state(button,LV_STATE_DISABLED) &&
        (!event || (lv_event_get_code(event)==LV_EVENT_CLICKED && lv_event_get_target_obj(event)==button &&
            lv_event_get_current_target_obj(event)==button));
}
static void manage_clicked_cb(lv_event_t *event)
{
    if(!chooser_action(event,s_manage_action) || !s_manage_cb)return;
    ui_printer_chooser_manage_cb_t manage=s_manage_cb;
    s_chooser_dispatching=true;manage(-1);s_chooser_dispatching=false;
}
static void card_clicked_cb(lv_event_t *event)
{
    if(!event)return;
    int index=(int)(intptr_t)lv_event_get_user_data(event);
    if(index<0 || index>=MOONRAKER_CONFIG_MAX_PROFILES || !chooser_action(event,s_cards[index].root))return;
    chooser_context_t context={s_root,s_chooser_epoch,moonraker_config_generation(),moonraker_config_active_profile_index()};
    ui_printer_chooser_select_cb_t select=s_select_cb;ui_printer_chooser_manage_cb_t manage=s_manage_cb;
    s_chooser_dispatching=true;
    const moonraker_profile_t *profile=moonraker_config_profile(index);
    if(chooser_current(&context)){
        if(!profile || !profile->configured){if(manage)manage(index);}
        else if(select)select(index);
    }
    s_chooser_dispatching=false;
}

static lv_obj_t *make_label(
    lv_obj_t *parent,
    const char *text,
    int x,
    int y,
    int width)
{
    lv_obj_t *label = lv_label_create(parent);
    chooser_watch(label);
    lv_label_set_text(label, text ? text : ui_text(""));
    lv_obj_set_width(label, width);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(label, x, y);
    return label;
}


static void create_card(int index, int x, int y, int width, int height)
{
    chooser_card_t *card = &s_cards[index];

    card->root = lv_obj_create(s_root);
    lv_obj_remove_style_all(card->root);
    lv_obj_add_event_cb(card->root,card_subjects_delete_cb,LV_EVENT_DELETE,card);
    lv_obj_set_size(card->root, width, height);
    lv_obj_set_pos(card->root, x, y);
    lv_obj_clear_flag(card->root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(card->root, LV_OBJ_FLAG_CLICKABLE);
    /* Open two-column sections retain the familiar chooser geometry. */
    lv_obj_set_style_bg_color(card->root, UI_PANEL, 0);
    lv_obj_set_style_bg_opa(card->root, LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(card->root, 0, 0);
    lv_obj_set_style_pad_all(card->root, 0, 0);
    lv_obj_set_style_border_width(card->root, 3, 0);
    lv_obj_set_style_border_side(card->root, LV_BORDER_SIDE_LEFT, 0);
    lv_obj_set_style_border_color(card->root, UI_ACCENT_BRIGHT, 0);
    lv_obj_set_style_border_opa(card->root, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(card->root, LV_OPA_COVER, LV_STATE_USER_1);
    lv_obj_set_style_bg_color(card->root, UI_CONTROL, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(card->root, LV_OPA_COVER, LV_STATE_PRESSED);

    lv_obj_t *divider = lv_obj_create(card->root);
    lv_obj_remove_style_all(divider);
    lv_obj_set_size(divider, width - 24, 1);
    lv_obj_set_pos(divider, 9, height - 1);
    lv_obj_set_style_bg_color(divider, UI_BORDER, 0);
    lv_obj_set_style_bg_opa(divider, LV_OPA_COVER, 0);
    lv_obj_clear_flag(divider, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_add_event_cb(
        card->root,
        card_clicked_cb,
        LV_EVENT_CLICKED,
        (void *)(intptr_t)index);

    card->preview_box = lv_obj_create(card->root);chooser_watch(card->preview_box);
    lv_obj_set_size(card->preview_box, 116, 116);
    lv_obj_set_pos(card->preview_box, 12, 34);
    /* Preview content is decorative; native taps belong to the card. */
    lv_obj_clear_flag(card->preview_box, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    ui_apply_preview_style(card->preview_box);
    lv_obj_set_style_pad_all(card->preview_box, 0, 0);
    lv_obj_set_style_radius(card->preview_box, 8, 0);

    card->preview_icon = lv_label_create(card->preview_box);chooser_watch(card->preview_icon);
    lv_label_set_text(card->preview_icon, LV_SYMBOL_FILE);
    /* Symbols use the same glyph face/size in every theme, including Studio. */
    ui_apply_custom_label_style(card->preview_icon, &lv_font_montserrat_32, UI_TEXT_DIM);
    lv_obj_center(card->preview_icon);

    card->name = make_label(card->root, "PRINTER", 146, 14, width - 170);
    ui_apply_text_title(card->name);
    ui_apply_label_bright(card->name);
    lv_label_set_long_mode(card->name, LV_LABEL_LONG_DOT);
    lv_obj_set_height(card->name, lv_obj_get_style_text_font(card->name, 0)->line_height);
    lv_obj_set_style_text_letter_space(card->name, 0, 0);

    card->endpoint = make_label(card->root, "--", 146, 58, width - 170);
    ui_apply_text_caption(card->endpoint);
    ui_apply_label_dim(card->endpoint);
    lv_label_set_long_mode(card->endpoint, LV_LABEL_LONG_DOT);
    lv_obj_set_height(card->endpoint, lv_obj_get_style_text_font(card->endpoint, 0)->line_height);
    lv_obj_set_style_text_letter_space(card->endpoint, 0, 0);

    card->status = make_label(card->root, "Checking...", 146, 88, width - 170);
    ui_apply_text_body_large(card->status);
    ui_apply_label_dim(card->status);
    ui_text_fit_single_line(card->status, UI_FONT_BODY_LARGE);
    lv_subject_init_string(&card->name_subject, card->name_value,
        card->name_previous, sizeof(card->name_value), "PRINTER");
    lv_subject_init_string(&card->endpoint_subject, card->endpoint_value,
        card->endpoint_previous, sizeof(card->endpoint_value), "--");
    card->name_bound = lv_label_bind_text(card->name, &card->name_subject, NULL) != NULL;
    card->endpoint_bound = lv_label_bind_text(card->endpoint, &card->endpoint_subject, NULL) != NULL;
    lv_subject_init_string(&card->status_subject, card->status_value,
        card->status_previous, sizeof(card->status_value), "Checking...");
    card->status_bound = lv_label_bind_text(card->status, &card->status_subject, NULL) != NULL;
    card->subjects_ready=true;

    lv_obj_t *hint = make_label(card->root, "Open " LV_SYMBOL_RIGHT, 146, height - 36, 130);
    ui_apply_text_caption(hint);
    ui_apply_label_dim(hint);
    lv_label_set_long_mode(hint, LV_LABEL_LONG_CLIP);
    card->hint = hint;

    card->active = make_label(card->root, "Active", width - 106, height - 36, 82);
    ui_apply_text_caption(card->active);
    ui_apply_label_success(card->active);
    lv_label_set_long_mode(card->active, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(card->active, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_align(card->hint, LV_TEXT_ALIGN_LEFT, 0);
}


static void refresh_cards(void)
{
    if(!s_root || s_refreshing || s_retiring)return;
    s_refreshing=true;
    chooser_context_t context={s_root,s_chooser_epoch,moonraker_config_generation(),moonraker_config_active_profile_index()};
    int active = moonraker_config_active_profile_index();
    moonraker_state_t state_snapshot;
    moonraker_state_snapshot(&state_snapshot);
    if(!chooser_current(&context))goto finished;
    const moonraker_state_t *state = &state_snapshot;

    for (int index = 0; index < MOONRAKER_CONFIG_MAX_PROFILES; ++index) {
        chooser_card_t *card = &s_cards[index];
        if(!chooser_current(&context))goto finished;
        if(!card->root)continue;
        const moonraker_profile_t *profile = moonraker_config_profile(index);
        if(!chooser_current(&context))goto finished;
        bool configured=profile && profile->configured;
        char profile_name[MOONRAKER_CONFIG_NAME_LENGTH],profile_host[MOONRAKER_CONFIG_HOST_LENGTH];
        snprintf(profile_name,sizeof(profile_name),"%.*s",(int)sizeof(profile_name)-1,profile?profile->name:"");
        snprintf(profile_host,sizeof(profile_host),"%.*s",(int)sizeof(profile_host)-1,profile?profile->host:"");
        int profile_port=profile?profile->port:0;
        bool known = false;
        bool online =
            configured &&
            printer_profile_health_get(index, &known);
        if(!chooser_current(&context))goto finished;

        bool health_online = online;
        if (configured && index == active) {
            online = !printer_connection_reason(state)[0];
            /* Transport readiness does not confirm an initial failed probe. */
        }

        if (!card->root) continue;

        /* Keep the normal 500ms refresh quiet when the selected row is unchanged. */
        bool selected = configured && index == active;
        if (lv_obj_has_state(card->root, LV_STATE_USER_1) != selected) {
            if (selected) lv_obj_add_state(card->root, LV_STATE_USER_1);
            else lv_obj_remove_state(card->root, LV_STATE_USER_1);
        }
        if(!chooser_current(&context))goto finished;
        ui_value_set_color(card->name, configured ? UI_TEXT_BRIGHT : UI_TEXT_DIM, 0);
        if(!chooser_current(&context))goto finished;
        ui_value_set_text(card->preview_icon, configured ? LV_SYMBOL_FILE : LV_SYMBOL_PLUS);
        if(!chooser_current(&context))goto finished;

        const char *cached_file = NULL;
        uint32_t cached_revision = 0;

        const lv_image_dsc_t *cached_image =
            configured
                ? printer_preview_cache_image(
                    index,
                    &cached_file,
                    &cached_revision)
                : NULL;
        if(!chooser_current(&context))goto finished;
        if(cached_image && (!cached_image->header.w || !cached_image->header.h || !card->preview_box))cached_image=NULL;

        if (cached_image) {
            bool created=false;
            if (!card->preview_image) {
                card->preview_image =
                    lv_image_create(card->preview_box);
                chooser_watch(card->preview_image);created=true;
            }

            if (card->preview_image &&
                (created || card->preview_revision != cached_revision)) {
                int width=cached_image->header.w,height=cached_image->header.h;
                lv_image_set_src(card->preview_image,cached_image);
                if(!chooser_current(&context))goto finished;

                int scale_x =
                    (108 * 256) / width;

                int scale_y =
                    (108 * 256) / height;

                int scale = scale_x < scale_y ? scale_x : scale_y;
                if (scale > 256) scale = 256;
                if (scale < 1) scale = 1;

                lv_image_set_scale(card->preview_image, scale);
                if(!chooser_current(&context))goto finished;
                lv_obj_center(card->preview_image);
                if(!chooser_current(&context))goto finished;
                card->preview_revision = cached_revision;
            }

            if (card->preview_image) {
                chooser_hidden(card->preview_image,false);
                /* Created last in this preview box; no repeated z-order write. */
            }

            chooser_hidden(card->preview_icon,true);
        } else {
            if (card->preview_image) {
                chooser_hidden(card->preview_image,true);
            }

            chooser_hidden(card->preview_icon,false);
            card->preview_revision = 0;
        }

        if(!chooser_current(&context))goto finished;
        if (!configured) {
            char empty_name[32];
            snprintf(empty_name, sizeof(empty_name), "Add printer %d", index + 1);
            set_name_text(card, empty_name);
            if(!chooser_current(&context))goto finished;
            set_endpoint_text(card, ui_text("Available profile slot"));
            if(!chooser_current(&context))goto finished;
            set_status_text_font(card, ui_text("Not configured"), UI_FONT_CAPTION);
            if(!chooser_current(&context))goto finished;
            ui_value_set_text(card->hint, ui_text("Set up " LV_SYMBOL_RIGHT));
            if(!chooser_current(&context))goto finished;
            chooser_hidden(card->active,true);
            apply_status_style(card->status, false, false);
            continue;
        }

        char endpoint[MOONRAKER_CONFIG_HOST_LENGTH + 16];
        snprintf(endpoint,sizeof(endpoint),"%s:%d",profile_host,profile_port);

        ui_value_set_text(card->hint, ui_text("Open " LV_SYMBOL_RIGHT));
        if(!chooser_current(&context))goto finished;
        set_name_text(card,profile_name);
        if(!chooser_current(&context))goto finished;
        set_endpoint_text(card, endpoint);
        if(!chooser_current(&context))goto finished;
        bool active_live =
            index == active &&
            state &&
            !printer_connection_reason(state)[0];
        bool status_online = active_live;
        const char *status_text = NULL;

        char inactive_state[PRINTER_PROFILE_HEALTH_STATE_LENGTH] = "";
        if (index == active) {
            /* Active transport readiness overrides cached background health. */
            if (active_live &&
                state->printer_state[0] &&
                strcmp(state->printer_state, "--") != 0) {
                status_text = state->printer_state;
            } else if (!state->moonraker_ok) {
                bool initial_state = !state->printer_state[0] ||
                    !strcmp(state->printer_state, "--");
                status_text = !known ? "CHECKING..."
                    : (health_online && initial_state ? "CONNECTING..."
                       : "OFFLINE / RETRYING");
            } else if (!strcmp(state->printer_state, "shutdown") ||
                       !strcmp(state->printer_state, "error")) {
                status_text = "KLIPPER SHUTDOWN";
            } else {
                status_text = "WAITING FOR KLIPPER";
            }
        } else {
            bool has_live_state =
                known &&
                online &&
                printer_profile_health_get_live_state(
                    index, inactive_state, sizeof(inactive_state));
            if(!chooser_current(&context))goto finished;
            bool inactive_online_fresh =
                known &&
                online &&
                printer_profile_health_live_state_fresh(
                    index, 5000000LL);
            if(!chooser_current(&context))goto finished;
            bool verifying = known && online && !inactive_online_fresh;

            status_online = inactive_online_fresh;
            /* Preserve a previously confirmed state during the normal freshness
             * window and while the next background probe is in flight. */
            status_text = !known
                ? "VERIFYING..."
                : (!online
                    ? "OFFLINE"
                    : (has_live_state
                        ? inactive_state
                        : (verifying
                            ? "VERIFYING..."
                            : "ONLINE")));
        }

        set_status_text(card, chooser_display_status(status_text));
        if(!chooser_current(&context))goto finished;
        if ((index == active && !state->moonraker_ok &&
             (!known || (health_online && (!state->printer_state[0] ||
              !strcmp(state->printer_state, "--"))))) ||
            (index != active &&
             known &&
             online &&
             !status_online)) {
            ui_value_set_color(card->status, UI_TEXT_DIM, 0);
        } else {
            apply_status_style(card->status, true, status_online);
        }

        chooser_hidden(card->active, index != active);
    }
finished:
    s_refreshing=false;chooser_flush_show();
}



static void chooser_timer_cb(lv_timer_t *timer)
{
    if(!timer || timer!=s_timer)return;
    refresh_cards();
}


void ui_printer_chooser_refresh(void)
{
    if (!s_root) return;
    refresh_cards();
}


void ui_printer_chooser_show(
    ui_printer_chooser_select_cb_t select_cb,
    ui_printer_chooser_manage_cb_t manage_cb)
{
    if(s_root){
        lv_obj_t *root=s_root;uint32_t epoch=s_chooser_epoch;
        refresh_cards();
        if(root==s_root && epoch==s_chooser_epoch)lv_obj_move_foreground(root);
        return;
    }
    /* Reusing static subjects inside their previous notification is unsafe. */
    if(s_subject_depth || s_retiring || s_refreshing){
        s_show_pending=true;s_pending_select=select_cb;s_pending_manage=manage_cb;return;
    }
    s_show_pending=false;s_pending_select=NULL;s_pending_manage=NULL;chooser_cancel_deferred();
    s_select_cb=select_cb;s_manage_cb=manage_cb;
    memset(s_cards,0,sizeof(s_cards));++s_chooser_epoch;
    s_root = lv_obj_create(lv_screen_active());
    lv_obj_add_event_cb(s_root,chooser_root_deleted,LV_EVENT_DELETE,NULL);
    lv_obj_set_size(s_root,
                    UI_PAGE_ROOT_WIDTH,
                    UI_PAGE_ROOT_HEIGHT);
    lv_obj_set_pos(s_root,
                   UI_PAGE_ROOT_X,
                   UI_PAGE_ROOT_Y);
    lv_obj_clear_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);
    ui_apply_root_style(s_root);
    lv_obj_set_style_pad_all(s_root, 0, 0);

    lv_obj_t *title = make_label(s_root, "Printers", 20, 12, 360);
    ui_apply_text_heading(title);
    ui_apply_label_bright(title);

    lv_obj_t *subtitle = make_label(
        s_root,
        "Select a printer to open its dashboard.",
        20,
        54,
        520);
    ui_apply_text_caption(subtitle);
    ui_apply_label_dim(subtitle);

    lv_obj_t *manage = ui_button_create_icon(
        s_root,
        UI_BUTTON_OUTLINED,
        LV_SYMBOL_SETTINGS,
        "Manage printers",
        UI_ACCENT_CYAN,
        UI_BUTTON_ICON_HORIZONTAL);

    s_manage_action=manage;chooser_watch(manage);
    if (manage) {
        lv_obj_set_size(manage, 280, 46);
        lv_obj_set_pos(manage, 546, 12);

        if (s_manage_cb) {
            lv_obj_add_event_cb(
                manage,
                manage_clicked_cb,
                LV_EVENT_CLICKED,
                NULL);
        }
    }

    int section_x = 20, section_y = 84, section_width = 390, section_height = 184;
    int column_step = 404, row_step = 198;
    if(ui_theme_is_studio()) {
        lv_obj_set_pos(title,0,0);lv_obj_set_width(title,640);ui_apply_custom_label_style(title,&ui_studio_font_32,UI_TEXT);
        lv_obj_add_flag(subtitle,LV_OBJ_FLAG_HIDDEN);
        if(manage){lv_obj_set_size(manage,280,44);lv_obj_set_pos(manage,696,0);}
        section_x = 0; section_y = 60; section_width = 480; section_height = 176;
        column_step = 496; row_step = 188;
    }
    for (int index = 0; index < MOONRAKER_CONFIG_MAX_PROFILES; ++index)
        create_card(index, section_x + (index % 2) * column_step,
            section_y + (index / 2) * row_step, section_width, section_height);

    lv_obj_t *center_separator = lv_obj_create(s_root);
    lv_obj_remove_style_all(center_separator);
    lv_obj_set_size(center_separator, 1, row_step + section_height - 20);
    lv_obj_set_pos(center_separator,
        section_x + section_width + (column_step - section_width) / 2, section_y + 8);
    lv_obj_set_style_bg_color(center_separator, UI_BORDER, 0);
    lv_obj_set_style_bg_opa(center_separator, LV_OPA_60, 0);
    lv_obj_clear_flag(center_separator, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *root=s_root;uint32_t epoch=s_chooser_epoch;
    refresh_cards();
    if(root!=s_root || epoch!=s_chooser_epoch)return;

    s_timer = lv_timer_create(chooser_timer_cb, 500, NULL);

    ESP_LOGI("printer_chooser", "Chooser visible");

    lv_obj_move_foreground(s_root);
}


static void chooser_flush_show(void)
{
    if(!s_show_pending || s_root || s_subject_depth || s_retiring || s_refreshing || s_flushing)return;
    ui_printer_chooser_select_cb_t select=s_pending_select;ui_printer_chooser_manage_cb_t manage=s_pending_manage;
    s_show_pending=false;s_pending_select=NULL;s_pending_manage=NULL;
    s_flushing=true;ui_printer_chooser_show(select,manage);s_flushing=false;
    if(s_show_pending && !s_root && !s_flush_queued)
        s_flush_queued=lv_async_call(chooser_deferred_show,NULL)==LV_RESULT_OK;
}
static void chooser_deferred_show(void *unused)
{
    (void)unused;s_flush_queued=false;chooser_flush_show();
}
void ui_printer_chooser_hide(void)
{
    lv_obj_t *root=s_root;
    s_show_pending=false;s_pending_select=NULL;s_pending_manage=NULL;chooser_cancel_deferred();
    chooser_retire();
    if(root)lv_obj_delete(root);
    chooser_flush_show();
}

bool ui_printer_chooser_is_visible(void)
{
    return s_root != NULL;
}
