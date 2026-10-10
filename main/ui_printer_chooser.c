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
    bool name_bound;
    bool endpoint_bound;
    lv_subject_t status_subject;
    char status_value[64];
    char status_previous[64];
    bool status_bound;
    lv_obj_t *preview_box;
    lv_obj_t *preview;
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


static void card_subjects_delete_cb(lv_event_t *event)
{
    chooser_card_t *card = lv_event_get_user_data(event);
    if (!card) return;
    /* Card DELETE precedes label deletion. Remove all widget observers before
     * the static card storage is cleared or reused on the next chooser open.
     */
    lv_subject_deinit(&card->name_subject);
    lv_subject_deinit(&card->endpoint_subject);
    lv_subject_deinit(&card->status_subject);
    card->name_bound = false;
    card->endpoint_bound = false;
    card->name = NULL;
    card->endpoint = NULL;
    card->status_bound = false;
    card->status = NULL;
    card->root = NULL;
}

static void set_card_text(lv_obj_t *label, lv_subject_t *subject,
                          size_t capacity, bool bound, const char *text)
{
    if (!label) return;
    if (!text) text = "";
    if (!bound || strlen(text) >= capacity) {
        ui_value_set_text(label, text);
        return;
    }
    bool unchanged = strcmp(lv_subject_get_string(subject), text) == 0;
    lv_subject_copy_string(subject, text);
    if (unchanged) ui_value_set_text(label, text);
}

static void set_status_text(chooser_card_t *card, const char *text)
{
    if (!card) return;
    bool changed = card->status && strcmp(lv_label_get_text(card->status), text ? text : "") != 0;
    set_card_text(card->status, &card->status_subject,
        sizeof(card->status_value), card->status_bound, text);
    if (changed) ui_text_fit_single_line(card->status, UI_FONT_BODY_LARGE);
}

static void set_name_text(chooser_card_t *card, const char *text)
{
    if (!card) return;
    set_card_text(card->name, &card->name_subject,
        sizeof(card->name_value), card->name_bound, text);
}

static void set_endpoint_text(chooser_card_t *card, const char *text)
{
    if (!card) return;
    set_card_text(card->endpoint, &card->endpoint_subject,
        sizeof(card->endpoint_value), card->endpoint_bound, text);
}

static void apply_status_style(lv_obj_t *label, bool configured, bool online)
{
    ui_value_set_color(label, !configured ? UI_TEXT_DIM :
        online ? UI_OK_BRIGHT :
        ui_theme_get_active() == UI_THEME_CLASSIC ? UI_TEXT_ERROR : UI_DANGER_BRIGHT, 0);
}


static void manage_clicked_cb(lv_event_t *event)
{
    (void)event;

    if (s_manage_cb) s_manage_cb(-1);
}


static void card_clicked_cb(lv_event_t *event)
{
    int index = (int)(intptr_t)lv_event_get_user_data(event);

    if (index < 0 || index >= MOONRAKER_CONFIG_MAX_PROFILES) return;

    const moonraker_profile_t *profile = moonraker_config_profile(index);

    if (!profile || !profile->configured) {
        /* An empty card is an explicit add-printer entry point. */
        if (s_manage_cb) s_manage_cb(index);
        return;
    }

    if (s_select_cb) s_select_cb(index);
}


static lv_obj_t *make_label(
    lv_obj_t *parent,
    const char *text,
    int x,
    int y,
    int width)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text ? text : ui_text(""));
    lv_obj_set_width(label, width);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(label, x, y);
    return label;
}


static void create_card(int index, int x, int y)
{
    chooser_card_t *card = &s_cards[index];

    card->root = lv_obj_create(s_root);
    lv_obj_set_size(card->root, ui_theme_is_studio()?480:390, ui_theme_is_studio()?176:184);
    lv_obj_set_pos(card->root, x, y);
    lv_obj_clear_flag(card->root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(card->root, LV_OBJ_FLAG_CLICKABLE);
    ui_apply_card_style(card->root);
    lv_obj_set_style_pad_all(card->root, 0, 0);

    lv_obj_add_event_cb(
        card->root,
        card_clicked_cb,
        LV_EVENT_CLICKED,
        (void *)(intptr_t)index);

    card->preview_box = lv_obj_create(card->root);
    lv_obj_set_size(card->preview_box, 116, 116);
    lv_obj_set_pos(card->preview_box, 12, 34);
    lv_obj_clear_flag(card->preview_box, LV_OBJ_FLAG_SCROLLABLE);
    ui_apply_preview_style(card->preview_box);
    lv_obj_set_style_pad_all(card->preview_box, 0, 0);

    card->preview_icon = lv_label_create(card->preview_box);
    lv_label_set_text(card->preview_icon, LV_SYMBOL_FILE);
    ui_apply_text_popup_title(card->preview_icon);
    ui_apply_label_dim(card->preview_icon);
    lv_obj_align(card->preview_icon, LV_ALIGN_TOP_MID, 0, 16);

    card->preview = lv_label_create(card->preview_box);
    lv_label_set_text(card->preview, "NO LIVE\nPREVIEW");
    lv_obj_set_width(card->preview, 108);
    lv_label_set_long_mode(card->preview, LV_LABEL_LONG_CLIP);
    ui_apply_text_caption(card->preview);
    lv_obj_set_height(card->preview, UI_FONT_CAPTION->line_height * 2);
    lv_obj_set_style_text_line_space(card->preview, 0, 0);
    ui_apply_label_dim(card->preview);
    lv_obj_set_style_text_align(card->preview, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(card->preview, LV_ALIGN_BOTTOM_MID, 0, -14);

    card->name = make_label(card->root, "PRINTER", 146, 14, ui_theme_is_studio()?310:220);
    ui_apply_text_title(card->name);
    ui_apply_label_bright(card->name);
    lv_label_set_long_mode(card->name, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_letter_space(card->name, 0, 0);

    card->endpoint = make_label(card->root, "--", 146, 58, ui_theme_is_studio()?310:220);
    ui_apply_text_caption(card->endpoint);
    ui_apply_label_dim(card->endpoint);
    lv_label_set_long_mode(card->endpoint, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_letter_space(card->endpoint, 0, 0);

    card->status = make_label(card->root, "CHECKING...", 146, 88, ui_theme_is_studio()?310:220);
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
        card->status_previous, sizeof(card->status_value), "CHECKING...");
    card->status_bound = lv_label_bind_text(card->status, &card->status_subject, NULL) != NULL;
    lv_obj_add_event_cb(card->root, card_subjects_delete_cb, LV_EVENT_DELETE, card);

    lv_obj_t *hint = make_label(card->root, "TAP TO OPEN", 146, 148, 130);
    ui_apply_text_caption(hint);
    ui_apply_label_dim(hint);
    lv_label_set_long_mode(hint, LV_LABEL_LONG_CLIP);
    card->hint = hint;

    card->active = make_label(card->root, "ACTIVE", ui_theme_is_studio()?374:284, 148, 82);
    ui_apply_text_caption(card->active);
    ui_apply_label_success(card->active);
    lv_label_set_long_mode(card->active, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(card->active, LV_TEXT_ALIGN_RIGHT, 0);
}


static void refresh_cards(void)
{
    int active = moonraker_config_active_profile_index();
    moonraker_state_t state_snapshot;
    moonraker_state_snapshot(&state_snapshot);
    const moonraker_state_t *state = &state_snapshot;

    for (int index = 0; index < MOONRAKER_CONFIG_MAX_PROFILES; ++index) {
        chooser_card_t *card = &s_cards[index];
        const moonraker_profile_t *profile = moonraker_config_profile(index);
        bool configured = profile && profile->configured;
        bool known = false;
        bool online =
            configured &&
            printer_profile_health_get(index, &known);

        if (configured && index == active && state && state->moonraker_ok) {
            online = true;
            known = true;
        }

        if (!card->root) continue;

        const char *cached_file = NULL;
        uint32_t cached_revision = 0;

        const lv_image_dsc_t *cached_image =
            configured
                ? printer_preview_cache_image(
                    index,
                    &cached_file,
                    &cached_revision)
                : NULL;

        if (cached_image) {
            if (!card->preview_image) {
                card->preview_image =
                    lv_image_create(card->preview_box);
            }

            if (card->preview_image &&
                card->preview_revision != cached_revision) {
                lv_image_set_src(card->preview_image, cached_image);

                int scale_x =
                    (108 * 256) / (int)cached_image->header.w;

                int scale_y =
                    (108 * 256) / (int)cached_image->header.h;

                int scale = scale_x < scale_y ? scale_x : scale_y;
                if (scale > 256) scale = 256;
                if (scale < 1) scale = 1;

                lv_image_set_scale(card->preview_image, scale);
                lv_obj_center(card->preview_image);
                card->preview_revision = cached_revision;
            }

            if (card->preview_image) {
                lv_obj_clear_flag(
                    card->preview_image,
                    LV_OBJ_FLAG_HIDDEN);
                /* Created last in this preview box; no repeated z-order write. */
            }

            lv_obj_add_flag(card->preview, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(card->preview_icon, LV_OBJ_FLAG_HIDDEN);
        } else {
            if (card->preview_image) {
                lv_obj_add_flag(
                    card->preview_image,
                    LV_OBJ_FLAG_HIDDEN);
            }

            lv_obj_clear_flag(card->preview, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(card->preview_icon, LV_OBJ_FLAG_HIDDEN);
            card->preview_revision = 0;
        }

        if (!configured) {
            char empty_name[32];
            snprintf(empty_name, sizeof(empty_name), "ADD PRINTER %d", index + 1);
            set_name_text(card, empty_name);
            set_endpoint_text(card, ui_text("EMPTY PROFILE SLOT"));
            set_status_text(card, ui_text("NOT CONFIGURED"));
            ui_value_set_text(card->hint, ui_text("TAP TO ADD"));
            if (!cached_image)
                ui_value_set_text(card->preview, "ADD A\nPRINTER");
            lv_obj_add_flag(card->active, LV_OBJ_FLAG_HIDDEN);
            apply_status_style(card->status, false, false);
            continue;
        }

        char endpoint[MOONRAKER_CONFIG_HOST_LENGTH + 16];
        snprintf(endpoint, sizeof(endpoint), "%s:%d", profile->host, profile->port);

        ui_value_set_text(card->hint, ui_text("TAP TO OPEN"));
        set_name_text(card, profile->name);
        set_endpoint_text(card, endpoint);
        bool active_live =
            index == active &&
            state &&
            state->moonraker_ok;
        bool status_online = active_live;
        const char *status_text = NULL;

        /* Both fallback buffers live through the final label write. */
        char inactive_state[PRINTER_PROFILE_HEALTH_STATE_LENGTH] = "";
        bool active_state_fallback = false;
        char active_health_state[PRINTER_PROFILE_HEALTH_STATE_LENGTH] = "";
        if (index == active) {
            /* Keep the last confirmed state visible while the active transport
             * reconnects; replace it only when fresh synchronized data arrives. */
            if (active_live &&
                state->printer_state[0] &&
                strcmp(state->printer_state, "--") != 0) {
                status_text = state->printer_state;
            } else if (printer_profile_health_get_live_state(
                           index,
                           active_health_state,
                           sizeof(active_health_state))) {
                status_text = active_health_state;
                active_state_fallback = true;
            } else {
                status_text = "OFFLINE / RETRYING";
            }
        } else {
            bool has_live_state =
                known &&
                online &&
                printer_profile_health_get_live_state(
                    index, inactive_state, sizeof(inactive_state));
            bool inactive_online_fresh =
                known &&
                online &&
                printer_profile_health_live_state_fresh(
                    index, 5000000LL);
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

        set_status_text(card, status_text);
        if ((index != active &&
             known &&
             online &&
             !status_online) ||
            (index == active && active_state_fallback)) {
            ui_value_set_color(card->status, UI_TEXT_DIM, 0);
        } else {
            apply_status_style(card->status, true, status_online);
        }

        if (index == active) {
            lv_obj_clear_flag(card->active, LV_OBJ_FLAG_HIDDEN);

            if (!cached_image) {
                if (state && state->live_data_ok && state->printer_file[0]) {
                    ui_value_set_text(card->preview, state->printer_file);
                    ui_value_set_color(card->preview, UI_TEXT_BRIGHT, 0);
                } else {
                    ui_value_set_text(card->preview, online ? "READY FOR\nLIVE DATA" : "NO LIVE\nPREVIEW");
                    ui_value_set_color(card->preview, UI_TEXT_DIM, 0);
                }
            }
        } else {
            lv_obj_add_flag(card->active, LV_OBJ_FLAG_HIDDEN);
            if (!cached_image) {
                ui_value_set_text(card->preview, online ? "AVAILABLE\nTO OPEN" : "NO LIVE\nPREVIEW");
                ui_value_set_color(card->preview, UI_TEXT_DIM, 0);
            }
        }
    }
}



static void chooser_timer_cb(lv_timer_t *timer)
{
    (void)timer;
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
    s_select_cb = select_cb;
    s_manage_cb = manage_cb;

    if (s_root) {
        refresh_cards();
        lv_obj_move_foreground(s_root);
        return;
    }

    s_root = lv_obj_create(lv_screen_active());
    lv_obj_set_size(s_root,
                    UI_PAGE_ROOT_WIDTH,
                    UI_PAGE_ROOT_HEIGHT);
    lv_obj_set_pos(s_root,
                   UI_PAGE_ROOT_X,
                   UI_PAGE_ROOT_Y);
    lv_obj_clear_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);
    ui_apply_root_style(s_root);
    lv_obj_set_style_pad_all(s_root, 0, 0);

    lv_obj_t *title = make_label(s_root, "PRINTERS", 20, 12, 360);
    ui_apply_text_heading(title);
    ui_apply_label_bright(title);

    lv_obj_t *subtitle = make_label(
        s_root,
        "Choose a printer to open its live operator dashboard.",
        20,
        54,
        520);
    ui_apply_text_caption(subtitle);
    ui_apply_label_dim(subtitle);

    lv_obj_t *manage = ui_button_create_icon(
        s_root,
        UI_BUTTON_OUTLINED,
        LV_SYMBOL_SETTINGS,
        "MANAGE PRINTERS",
        UI_ACCENT_CYAN,
        UI_BUTTON_ICON_HORIZONTAL);

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

    if(ui_theme_is_studio()) {
        lv_obj_set_pos(title,0,0);lv_obj_set_width(title,640);ui_apply_custom_label_style(title,&ui_studio_font_32,UI_TEXT);
        lv_obj_add_flag(subtitle,LV_OBJ_FLAG_HIDDEN);
        if(manage){lv_obj_set_size(manage,280,44);lv_obj_set_pos(manage,696,0);}
        create_card(0,0,60);create_card(1,496,60);
        create_card(2,0,248);create_card(3,496,248);
    } else {
        create_card(0,20,84);create_card(1,424,84);
        create_card(2,20,282);create_card(3,424,282);
    }

    refresh_cards();

    s_timer = lv_timer_create(chooser_timer_cb, 500, NULL);

    ESP_LOGI("printer_chooser", "Chooser visible");

    lv_obj_move_foreground(s_root);
}


void ui_printer_chooser_hide(void)
{
    if (s_timer) {
        lv_timer_delete(s_timer);
        s_timer = NULL;
    }

    if (s_root) {
        lv_obj_delete(s_root);
        s_root = NULL;
    }

    memset(s_cards, 0, sizeof(s_cards));
    s_select_cb = NULL;
    s_manage_cb = NULL;
}


bool ui_printer_chooser_is_visible(void)
{
    return s_root != NULL;
}
