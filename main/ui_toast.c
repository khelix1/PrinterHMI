#include "ui_toast.h"

#include <stdio.h>
#include <string.h>
#include "nvs.h"
#include "operator_event_log.h"
#include "moonraker_config_controller.h"
#include "ui_button.h"
#include "ui_widgets.h"
#include "ui_popup.h"
#include "ui_text_fit.h"
#include "printer_profile_health.h"
#include "ui_value_update.h"
#include "ui_form_dialog.h"

#define NOTICE_CAPACITY 4
#define NOTICE_OPEN_PRINTERS (-2)
#define NOTICE_NVS "notifications"
#define NOTICE_KEY "level"
#define NOTICE_LEGACY_KEY "confirm"

typedef struct {
    ui_status_kind_t kind;
    int destination;
    int profile_index;
    uint32_t owner;
    bool essential;
    char title[80], detail[320], printer[80];
} notice_t;
static notice_t s_notices[NOTICE_CAPACITY];
static unsigned s_notice_count, s_overflow;
static ui_notification_level_t s_level; /* Quiet default; errors are never disabled. */
static lv_obj_t *s_toast, *s_preference_popup, *s_preference_value, *s_preference_status;
static lv_timer_t *s_timer;
static lv_obj_t *s_level_buttons[3], *s_preference_save, *s_notice_open, *s_notice_dismiss, *s_notice_body;
static uint32_t s_surface_epoch,s_notice_epoch,s_preference_epoch;
static bool s_notice_dispatching,s_preference_dispatching;
static void notification_field_deleted(lv_event_t *event)
{
    lv_obj_t *target=lv_event_get_target_obj(event);
    if(target==s_preference_value)s_preference_value=NULL;
    if(target==s_preference_status)s_preference_status=NULL;
    if(target==s_preference_save)s_preference_save=NULL;
    if(target==s_notice_body)s_notice_body=NULL;
    if(target==s_notice_open)s_notice_open=NULL;
    if(target==s_notice_dismiss)s_notice_dismiss=NULL;
    for(unsigned i=0;i<3;i++)if(target==s_level_buttons[i])s_level_buttons[i]=NULL;
}
static void notification_field_own(lv_obj_t *field)
{if(field)lv_obj_add_event_cb(field,notification_field_deleted,LV_EVENT_DELETE,NULL);}
static ui_notification_level_t s_pending_level;
static void (*s_preference_changed)(void);
static void render_notice(void);
static void (*s_printer_chooser_action)(void);
void ui_toast_set_printer_chooser_action(void (*action)(void)){s_printer_chooser_action=action;}

static void surface_deleted(lv_event_t *event)
{
    if(lv_event_get_target_obj(event)!=s_toast)return;
    ++s_surface_epoch;
    s_toast=s_notice_open=s_notice_dismiss=s_notice_body=NULL;
    lv_timer_t *timer=s_timer;s_timer=NULL;
    if(timer)lv_timer_delete(timer);
}
static void close_surface(void)
{
    ++s_surface_epoch;
    lv_timer_t *timer=s_timer;
    lv_obj_t *object=s_toast;
    s_timer=NULL;s_toast=s_notice_open=s_notice_dismiss=s_notice_body=NULL;
    if(timer)lv_timer_delete(timer);
    if(object)lv_obj_delete(object);
}
void ui_toast_close(void)
{
    ++s_notice_epoch;
    s_notice_count=s_overflow=0;
    close_surface();
}

void ui_toast_init(void)
{
    nvs_handle_t handle;
    s_level=UI_NOTIFY_QUIET;
    if(nvs_open(NOTICE_NVS,NVS_READONLY,&handle)==ESP_OK) {
        uint8_t value=0;
        esp_err_t error=nvs_get_u8(handle,NOTICE_KEY,&value);
        if(error==ESP_OK && value<=UI_NOTIFY_DETAILED) s_level=(ui_notification_level_t)value;
        else if(error==ESP_ERR_NVS_NOT_FOUND && nvs_get_u8(handle,NOTICE_LEGACY_KEY,&value)==ESP_OK && value<=1)
            s_level=value ? UI_NOTIFY_DETAILED : UI_NOTIFY_QUIET;
        nvs_close(handle);
    }
}
ui_notification_level_t ui_toast_level(void){return s_level;}
bool ui_toast_confirmations_enabled(void){return s_level==UI_NOTIFY_DETAILED;}
const char *ui_toast_preference_label(void){return (const char *[]){"Quiet","Standard","Detailed"}[s_level];}
bool ui_toast_set_level(ui_notification_level_t level)
{
    if(level<UI_NOTIFY_QUIET || level>UI_NOTIFY_DETAILED)return false;
    nvs_handle_t handle;
    if(nvs_open(NOTICE_NVS,NVS_READWRITE,&handle)!=ESP_OK)return false;
    esp_err_t error=nvs_set_u8(handle,NOTICE_KEY,(uint8_t)level);
    if(error==ESP_OK)error=nvs_commit(handle);
    nvs_close(handle);
    if(error!=ESP_OK)return false;
    s_level=level;
    if(level==UI_NOTIFY_QUIET) {
        unsigned kept=0;
        for(unsigned i=0;i<s_notice_count;i++) {
            if(s_notices[i].kind!=UI_STATUS_INFO || s_notices[i].essential)
                s_notices[kept++]=s_notices[i];
        }
        ++s_notice_epoch;s_notice_count=kept;
        if(!kept)s_overflow=0;
        render_notice();
    } else if(level!=UI_NOTIFY_DETAILED && !s_notice_count)close_surface();
    return true;
}
bool ui_toast_set_confirmations(bool enabled)
{
    return ui_toast_set_level(enabled ? UI_NOTIFY_DETAILED : UI_NOTIFY_QUIET);
}
static lv_obj_t *column(lv_obj_t *parent)
{
    lv_obj_t *object = lv_obj_create(parent);
    lv_obj_remove_style_all(object);
    lv_obj_set_width(object, LV_PCT(100));
    lv_obj_set_height(object, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(object, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(object, 8, 0);
    lv_obj_remove_flag(object, LV_OBJ_FLAG_SCROLLABLE);
    return object;
}
static lv_obj_t *text(lv_obj_t *parent, const char *value, const lv_font_t *font, lv_color_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_width(label, LV_PCT(100));
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_label_set_text(label, value);
    ui_apply_custom_label_style(label, font, color);
    return label;
}
static void dismiss_current_notice(void)
{
    if(s_notice_count){
        --s_notice_count;memmove(s_notices,s_notices+1,s_notice_count*sizeof(s_notices[0]));
        ++s_notice_epoch;
    }
    if(!s_notice_count)s_overflow=0;
    uint32_t expected=s_surface_epoch+1;
    close_surface();
    if(expected==s_surface_epoch && s_notice_count && !s_toast)render_notice();
}
static void open_notice(lv_event_t *event)
{
    if(!s_toast || !s_notice_body || !s_notice_open || !s_notice_count || s_notice_dispatching || lv_obj_has_state(s_notice_open,LV_STATE_DISABLED) ||
        (event && (lv_event_get_code(event)!=LV_EVENT_CLICKED || lv_event_get_target_obj(event)!=s_notice_open)))return;
    notice_t notice=s_notices[0];
    void(*chooser)(void)=s_printer_chooser_action;
    if(notice.destination==NOTICE_OPEN_PRINTERS){
        if(!chooser)return;
        s_notice_dispatching=true;dismiss_current_notice();chooser();s_notice_dispatching=false;return;
    }
    if(notice.owner!=moonraker_config_generation() || notice.destination<0 || notice.destination>=UI_SHELL_PAGE_COUNT){
        lv_obj_t *body=s_notice_body;
        if(body)ui_popup_feedback(body,UI_STATUS_WARNING,"PRINTER CHANGED","Printer context changed. Open Console from the sidebar for the printer named in this notice.");
        return;
    }
    s_notice_dispatching=true;
    dismiss_current_notice();
    if(notice.owner==moonraker_config_generation()){
        uint32_t epoch=s_surface_epoch;
        ui_shell_page_action((ui_shell_page_t)notice.destination);
        if(epoch==s_surface_epoch && notice.owner==moonraker_config_generation())
            ui_shell_set_active_nav(notice.destination>=UI_SHELL_PAGE_BED_MESH?UI_SHELL_PAGE_TOOLS:notice.destination);
    }
    s_notice_dispatching=false;
}
static void dismiss_notice(lv_event_t *event)
{
    if(!s_toast || !s_notice_dismiss || !s_notice_count || s_notice_dispatching || lv_obj_has_state(s_notice_dismiss,LV_STATE_DISABLED) ||
        (event && (lv_event_get_code(event)!=LV_EVENT_CLICKED || lv_event_get_target_obj(event)!=s_notice_dismiss)))return;
    s_notice_dispatching=true;dismiss_current_notice();s_notice_dispatching=false;
}

static void render_notice(void)
{
    uint32_t expected=s_surface_epoch+1;
    close_surface();
    if(expected!=s_surface_epoch || s_toast || !s_notice_count)return;
    notice_t displayed=s_notices[0];
    const notice_t *notice=&displayed;
    int width = lv_display_get_horizontal_resolution(NULL) - 32;
    int height = lv_display_get_vertical_resolution(NULL) - 32;
    if (width > 520) width = 520;
    if (height > 260) height = 260;
    s_toast = lv_obj_create(lv_layer_top());
    if(!s_toast)return;
    ++s_surface_epoch;
    ui_apply_popup_style(s_toast);
    lv_obj_set_size(s_toast, width, height);
    lv_obj_align(s_toast, LV_ALIGN_BOTTOM_RIGHT, -16, -16);
    lv_obj_set_flex_flow(s_toast, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(s_toast, 12, 0);
    lv_obj_set_style_pad_row(s_toast, 8, 0);
    lv_obj_set_style_border_color(s_toast, ui_status_color(notice->kind), 0);
    lv_obj_remove_flag(s_toast, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_toast, surface_deleted, LV_EVENT_DELETE, NULL);
    lv_obj_t *body = column(s_toast);
    s_notice_body=body;notification_field_own(body);
    lv_obj_set_height(body, 0);
    lv_obj_set_flex_grow(body, 1);
    lv_obj_add_flag(body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(body, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(body, LV_SCROLLBAR_MODE_OFF);
    text(body, notice->printer, UI_FONT_CAPTION, UI_TEXT_DIM);
    text(body, notice->title, UI_FONT_BODY_LARGE, ui_status_color(notice->kind));
    text(body, notice->detail, UI_FONT_BODY, UI_TEXT);
    if (s_overflow) text(body, "Additional notices are recorded in Event History.", UI_FONT_CAPTION, UI_WARN);
    lv_obj_t *footer = column(s_toast);
    lv_obj_set_flex_flow(footer, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(footer, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    char count[48];
    snprintf(count, sizeof(count), "%u notice%s", s_notice_count, s_notice_count == 1 ? "" : "s");
    lv_obj_t *label = text(footer, count, UI_FONT_CAPTION, UI_TEXT_DIM);
    lv_obj_set_width(label, 0); lv_obj_set_flex_grow(label, 1);
    if(notice->destination>=0 || notice->destination==NOTICE_OPEN_PRINTERS) {
        const char *name=notice->destination==NOTICE_OPEN_PRINTERS ? "OPEN PRINTERS" : notice->destination==UI_SHELL_PAGE_CONSOLE ? "OPEN CONSOLE" : notice->destination==UI_SHELL_PAGE_PRINTER ? "OPEN PRINTER" : "OPEN PAGE";
        lv_obj_t *open=ui_button_create(footer,UI_BUTTON_OUTLINED,name);
        lv_obj_set_size(open,144,48);
        ui_text_fit_single_line(lv_obj_get_child(open,0),UI_FONT_BODY);
        s_notice_open=open;notification_field_own(open);
        lv_obj_add_event_cb(open,open_notice,LV_EVENT_CLICKED,NULL);
    }
    lv_obj_t *close = ui_button_create(footer, UI_BUTTON_CLOSE, "DISMISS");
    lv_obj_set_size(close, 128, 48);
    s_notice_dismiss=close;notification_field_own(close);
    lv_obj_add_event_cb(close, dismiss_notice, LV_EVENT_CLICKED, NULL);
    ui_text_fit_single_line(lv_obj_get_child(close, 0), UI_FONT_BODY_LARGE);
    lv_label_set_long_mode(lv_obj_get_child(close, 0), LV_LABEL_LONG_CLIP);
}
void ui_toast_refresh_appearance(void)
{
    /* Page recreation does not own this top-layer surface. Keep the notice
     * queue and captured printer/action context, but rebuild visible faults
     * with the current theme and font metrics. A brief optional confirmation
     * can retire with the old appearance instead of restarting its timer. */
    if (!s_toast) return;
    if (s_notice_count) render_notice();
    else close_surface();
}

static void toast_timer_cb(lv_timer_t *timer)
{
    if(timer!=s_timer || !s_timer || !s_toast)return;
    close_surface();
}
static bool optional(ui_status_kind_t kind)
{
    return kind == UI_STATUS_OK || kind == UI_STATUS_NEUTRAL;
}
static void toast_show_named(ui_status_kind_t kind, const char *title, const char *detail, int destination, bool essential, const char *printer, int profile_index)
{
    if (!printer || !printer[0]) printer = "PrinterHMI";
    if (optional(kind)) {
        if (s_level!=UI_NOTIFY_DETAILED || s_notice_count) return;
        uint32_t expected=s_surface_epoch+1;
        close_surface();
        if(expected!=s_surface_epoch || s_toast || s_notice_count)return;
        int width = lv_display_get_horizontal_resolution(NULL) - 32;
        if (width > 520) width = 520;
        s_toast = lv_obj_create(lv_layer_top());
        if(!s_toast)return;
        ++s_surface_epoch;
        ui_apply_popup_style(s_toast);
        lv_obj_set_width(s_toast, width);
        lv_obj_set_height(s_toast, LV_SIZE_CONTENT);
        lv_obj_align(s_toast, LV_ALIGN_BOTTOM_RIGHT, -16, -16);
        lv_obj_set_flex_flow(s_toast, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_all(s_toast, 12, 0);
        lv_obj_set_style_pad_row(s_toast, 4, 0);
        lv_obj_remove_flag(s_toast, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(s_toast, surface_deleted, LV_EVENT_DELETE, NULL);
        text(s_toast, title ? title : "", UI_FONT_BODY_LARGE, ui_status_color(kind));
        s_timer = lv_timer_create(toast_timer_cb, 2600, NULL);
        if (s_timer) lv_timer_set_repeat_count(s_timer, 1);
        return;
    }
    notice_t notice = {.kind = kind, .profile_index = profile_index, .destination = destination, .owner = moonraker_config_generation(), .essential = essential};
    snprintf(notice.title, sizeof(notice.title), "%s", title ? title : "");
    snprintf(notice.detail, sizeof(notice.detail), "%s", detail ? detail : "");
    snprintf(notice.printer, sizeof(notice.printer), "%s", printer);
    for (unsigned i = 0; i < s_notice_count; ++i) {
        if (s_notices[i].kind == kind && s_notices[i].profile_index==profile_index && s_notices[i].destination==destination && s_notices[i].owner==notice.owner && !strcmp(s_notices[i].title, notice.title) &&
            !strcmp(s_notices[i].detail, notice.detail) && !strcmp(s_notices[i].printer, notice.printer)) {
            if (!s_toast) render_notice();
            return;
        }
    }
    uint32_t epoch=s_notice_epoch;
    operator_event_log_add(kind == UI_STATUS_DANGER ? OPERATOR_EVENT_ERROR :
        kind == UI_STATUS_WARNING ? OPERATOR_EVENT_WARNING : OPERATOR_EVENT_INFO,
        "%s: %s | %s", printer, notice.title, notice.detail);
    if(epoch!=s_notice_epoch || notice.owner!=moonraker_config_generation())return;
    /* Quiet suppresses routine event cards, not their Event History record. */
    if(kind==UI_STATUS_INFO && s_level==UI_NOTIFY_QUIET && !essential)return;
    unsigned priority=kind==UI_STATUS_DANGER?2:kind==UI_STATUS_WARNING?1:0;
    if (s_notice_count == NOTICE_CAPACITY) {
        ++s_overflow;
        int retire=-1;
        for(int i=(int)s_notice_count-1;i>=0;i--) {
            unsigned existing=s_notices[i].kind==UI_STATUS_DANGER?2:
                s_notices[i].kind==UI_STATUS_WARNING?1:0;
            if(existing<=priority){retire=i;break;}
        }
        if(retire<0){render_notice();return;}
        memmove(s_notices+retire,s_notices+retire+1,
            (s_notice_count-(unsigned)retire-1)*sizeof(s_notices[0]));
        --s_notice_count; /* Retired notice remains in Event History. */
    }
    unsigned position=0;
    if(kind!=UI_STATUS_DANGER) {
        while(position<s_notice_count) {
            unsigned existing=s_notices[position].kind==UI_STATUS_DANGER?2:
                s_notices[position].kind==UI_STATUS_WARNING?1:0;
            if(existing<priority)break;
            position++;
        }
    }
    memmove(s_notices + position + 1, s_notices + position,
        (s_notice_count - position) * sizeof(s_notices[0]));
    s_notices[position] = notice; ++s_notice_count;++s_notice_epoch;
    render_notice();
}
static void toast_show(ui_status_kind_t kind, const char *title, const char *detail, int destination, bool essential)
{
    toast_show_named(kind, title, detail, destination, essential,
                     moonraker_config_active_profile_name(), moonraker_config_active_profile_index());
}

void ui_toast_show(ui_status_kind_t kind,const char *title,const char *detail)
{
    toast_show(kind,title,detail,-1,false);
}
void ui_toast_show_link(ui_status_kind_t kind,const char *title,const char *detail,ui_shell_page_t page)
{
    toast_show(kind,title,detail,page>=0 && page<UI_SHELL_PAGE_COUNT ? (int)page : -1,false);
}
static void preferences_deleted(lv_event_t *event)
{
    if(lv_event_get_target_obj(event)!=s_preference_popup)return;
    ++s_preference_epoch;
    s_preference_popup=s_preference_value=s_preference_status=s_preference_save=NULL;
    s_preference_changed=NULL;memset(s_level_buttons,0,sizeof(s_level_buttons));
}
void ui_toast_preferences_close(void)
{
    lv_obj_t *popup=s_preference_popup;
    if(popup)++s_preference_epoch;
    s_preference_popup=s_preference_value=s_preference_status=s_preference_save=NULL;
    s_preference_changed=NULL;memset(s_level_buttons,0,sizeof(s_level_buttons));
    if(popup)lv_obj_delete(popup);
}
static void preference_close(lv_event_t *event){(void)event;ui_toast_preferences_close();}
static void preference_toggle(lv_event_t *event)
{
    if(!s_preference_popup || s_preference_dispatching)return;
    unsigned level=event?(unsigned)(uintptr_t)lv_event_get_user_data(event):((unsigned)s_pending_level+1)%3;
    if(level>=3 || !s_level_buttons[level] ||
        (event && (lv_event_get_code(event)!=LV_EVENT_CLICKED || lv_event_get_target_obj(event)!=s_level_buttons[level])))return;
    s_pending_level=(ui_notification_level_t)level;
    for(unsigned i=0;i<3;i++){
        if(!s_level_buttons[i])continue;
        ui_button_set_filter_active(s_level_buttons[i], i == level);
        char text[32];snprintf(text,sizeof(text),"%s%s",i==level?LV_SYMBOL_OK " ":"",(const char*[]){"Quiet","Standard","Detailed"}[i]);
        ui_value_set_text(lv_obj_get_child(s_level_buttons[i],0),text);
    }
}
static void preference_save(lv_event_t *event)
{
    if(!s_preference_popup || !s_preference_save || s_preference_dispatching || (unsigned)s_pending_level>=3 || !s_level_buttons[s_pending_level] ||
        (event && (lv_event_get_code(event)!=LV_EVENT_CLICKED || lv_event_get_target_obj(event)!=s_preference_save)))return;
    uint32_t epoch=s_preference_epoch;
    ui_notification_level_t level=s_pending_level;
    void(*changed)(void)=s_preference_changed;
    s_preference_dispatching=true;
    bool saved=ui_toast_set_level(level);
    if(epoch!=s_preference_epoch){s_preference_dispatching=false;return;}
    if(!saved){
        if(s_preference_status){ui_value_set_text(s_preference_status,"Could not save. Your previous setting is unchanged.");lv_obj_scroll_to_view(s_preference_status,LV_ANIM_OFF);}
        s_preference_dispatching=false;return;
    }
    ui_toast_preferences_close();
    if(s_preference_epoch==epoch+1 && changed)changed();
    s_preference_dispatching=false;
}

void ui_toast_preferences_show(void (*changed)(void))
{
    if (s_preference_popup) { lv_obj_move_foreground(s_preference_popup); return; }
    int width = lv_display_get_horizontal_resolution(NULL) - 32;
    int height = lv_display_get_vertical_resolution(NULL) - 32;
    if (width > 760) width = 760;
    if (height > 520) height = 520;
    s_preference_popup = ui_popup_create(lv_layer_top(), width, height, UI_POPUP_STANDARD);
    if (!s_preference_popup) return;
    ++s_preference_epoch;
    s_preference_changed = changed; s_pending_level = s_level;
    lv_obj_add_event_cb(s_preference_popup, preferences_deleted, LV_EVENT_DELETE, NULL);
    lv_obj_set_flex_flow(s_preference_popup, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(s_preference_popup, UI_PAD_POPUP, 0);
    lv_obj_set_style_pad_row(s_preference_popup, UI_GAP_ROW, 0);
    lv_obj_remove_flag(s_preference_popup, LV_OBJ_FLAG_SCROLLABLE);
    text(s_preference_popup, "Notifications", UI_FONT_TITLE, UI_TEXT);
    ui_form_divide_title(s_preference_popup);
    lv_obj_t *body = column(s_preference_popup);
    lv_obj_set_height(body, 0); lv_obj_set_flex_grow(body, 1);
    lv_obj_add_flag(body, LV_OBJ_FLAG_SCROLLABLE); lv_obj_set_scroll_dir(body, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(body, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_pad_row(body, UI_GAP_ROW, 0);
    text(body, "Applies to every printer, page and theme. Local validation and progress remain visible at every level.", UI_FONT_CAPTION, UI_TEXT_DIM);
    static const char *descriptions[] = {
        "Connection loss/recovery, paused prints, warnings and errors.",
        "Everything in Quiet, plus routine print events.",
        "Everything in Standard, plus routine confirmations.",
    };
    for(unsigned i=0;i<3;i++) {
        s_level_buttons[i]=ui_popup_add_selectable_row(body,
            (const char *[]){"Quiet","Standard","Detailed"}[i], 0, 0, 1, 48,
            preference_toggle, (void *)(uintptr_t)i);
        notification_field_own(s_level_buttons[i]);
        lv_obj_set_size(s_level_buttons[i],LV_PCT(100),LV_SIZE_CONTENT);
        lv_obj_set_style_min_height(s_level_buttons[i],48,0);
        lv_obj_set_flex_flow(s_level_buttons[i],LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_hor(s_level_buttons[i],UI_PAD_CARD,0);
        lv_obj_set_style_pad_ver(s_level_buttons[i],10,0);
        lv_obj_set_style_pad_row(s_level_buttons[i],4,0);
        lv_obj_t *label=lv_obj_get_child(s_level_buttons[i],0);
        lv_obj_set_width(label,LV_PCT(100));
        lv_label_set_long_mode(label,LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_align(label,LV_TEXT_ALIGN_LEFT,0);
        lv_obj_t *description=text(s_level_buttons[i],descriptions[i],UI_FONT_CAPTION,UI_TEXT_DIM);
        lv_obj_set_style_text_align(description,LV_TEXT_ALIGN_LEFT,0);
        ui_button_set_filter_active(s_level_buttons[i], i == (unsigned)s_pending_level);
        if(i==(unsigned)s_pending_level) {
            lv_label_set_text_fmt(lv_obj_get_child(s_level_buttons[i],0),LV_SYMBOL_OK " %s",(const char *[]){"Quiet","Standard","Detailed"}[i]);
        }
    }
    s_preference_value=lv_obj_get_child(s_level_buttons[s_pending_level],0);
    notification_field_own(s_preference_value);
    s_preference_status = text(body, "Dismiss acknowledges a notice; it does not clear a printer fault.", UI_FONT_CAPTION, UI_TEXT_DIM);
    notification_field_own(s_preference_status);
    lv_obj_t *footer = column(s_preference_popup);
    ui_apply_divider_border(footer, LV_BORDER_SIDE_TOP);
    lv_obj_set_style_pad_top(footer, 8, 0);
    lv_obj_set_flex_flow(footer, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(footer, 8, 0);
    lv_obj_t *cancel = ui_button_create(footer, UI_BUTTON_CANCEL, "Cancel");
    lv_obj_set_size(cancel, 128, 48); lv_obj_set_flex_grow(cancel, 1); lv_obj_add_event_cb(cancel, preference_close, LV_EVENT_CLICKED, NULL);
    lv_obj_t *save = ui_button_create(footer, UI_BUTTON_PRIMARY, "Save");
    s_preference_save=save;notification_field_own(save);
    lv_obj_set_size(save, 128, 48); lv_obj_set_flex_grow(save, 1); lv_obj_add_event_cb(save, preference_save, LV_EVENT_CLICKED, NULL);
}

/* Observe only this owner; boot/printer selection establishes a baseline,
 * not a fabricated disconnect or print event. No per-telemetry-poll notices. */
void ui_toast_observe_printer(const moonraker_state_t *state)
{
    static uint32_t owner,candidate_since;
    static bool initialized,armed,live,candidate,last_sample_live;
    static char previous[32],fault[32];
    if(!state)return;
    uint32_t current=moonraker_config_generation(),now=lv_tick_get();
    if(!initialized || owner!=current) {
        owner=current;initialized=true;armed=false;live=false;
        candidate=state->live_data_ok;candidate_since=now;
        last_sample_live=state->live_data_ok;
        fault[0]=0;
        snprintf(previous,sizeof(previous),"%s",state->printer_state);return;
    }
    if(candidate!=state->live_data_ok) {
        candidate=state->live_data_ok;candidate_since=now;
    }
    /* Selection/bootstrap is not a lost connection. Establish a confirmed
     * live baseline first, then debounce transport freshness changes. */
    bool established_now=false;
    if((uint32_t)(now-candidate_since)>=3000u) {
        if(!armed && candidate) {
            armed=true;live=true;established_now=true;
        } else if(armed && live!=candidate) {
            live=candidate;
            /* Replace this owner's previous connection notice on every
             * transition, including repeated loss/recovery without Dismiss. */
            unsigned kept=0;
            for(unsigned i=0;i<s_notice_count;i++) {
                bool connection=s_notices[i].owner==current &&
                    s_notices[i].profile_index==moonraker_config_active_profile_index() &&
                    (!strcmp(s_notices[i].title,"PRINTER OFFLINE") ||
                     !strcmp(s_notices[i].title,"PRINTER RECONNECTED"));
                if(!connection)s_notices[kept++]=s_notices[i];
            }
            ++s_notice_epoch;s_notice_count=kept;
            if(live) {
                toast_show(UI_STATUS_INFO,"PRINTER RECONNECTED",
                    "Live status is available again. Review the printer before resuming work.",UI_SHELL_PAGE_PRINTER,true);
            } else {
                ui_toast_show_link(UI_STATUS_WARNING,"PRINTER OFFLINE",
                    "Live status was lost. Printing may still be running; check the printer and its connection.",UI_SHELL_PAGE_PRINTER);
            }
            established_now=true;
        }
    }
    /* Klipper shutdown and print errors require attention even if live
     * printing telemetry is no longer available, while Moonraker is reachable. */
    bool reported_fault=!strcmp(state->printer_state,"shutdown") || !strcmp(state->printer_state,"error");
    if(armed && (state->live_data_ok || state->moonraker_ok)) {
        if(reported_fault && strcmp(fault,state->printer_state)) {
            snprintf(fault,sizeof(fault),"%s",state->printer_state);
            ui_toast_show_link(UI_STATUS_DANGER,
                !strcmp(fault,"shutdown")?"PRINTER SHUTDOWN":"PRINTER ERROR",
                "Review the reported error in Console. Resolve the cause before restarting or resuming.",UI_SHELL_PAGE_CONSOLE);
        } else if(!reported_fault && state->live_data_ok) fault[0]=0;
    }
    if(armed && !established_now && live && last_sample_live && state->live_data_ok && strcmp(previous,state->printer_state)) {
        const char *title=!strcmp(state->printer_state,"printing") ? (!strcmp(previous,"paused") ? "PRINT RESUMED" : "PRINT STARTED") :
            !strcmp(state->printer_state,"paused") ? "PRINT PAUSED" :
            !strcmp(state->printer_state,"complete") ? "PRINT COMPLETE" :
            !strcmp(state->printer_state,"cancelled") ? "PRINT CANCELLED" : NULL;
        if(title)ui_toast_show_link(!strcmp(state->printer_state,"paused")?UI_STATUS_WARNING:UI_STATUS_INFO,
            title,!strcmp(state->printer_state,"paused")?"The print needs attention. Open Printer for controls or filament recovery.":"Open Printer to review this job.",UI_SHELL_PAGE_PRINTER);
    }
    last_sample_live=state->live_data_ok;
    /* Preserve last reported job state during a disconnect. */
    if(state->live_data_ok)snprintf(previous,sizeof(previous),"%s",state->printer_state);
}

void ui_toast_observe_profiles(const moonraker_state_t *active_state)
{
    typedef struct {
        bool configured, armed, online;
        int port;
        char host[MOONRAKER_CONFIG_HOST_LENGTH];
    } profile_observer_t;
    static profile_observer_t observed[MOONRAKER_CONFIG_MAX_PROFILES];
    int active = moonraker_config_active_profile_index();
    for (int index = 0; index < MOONRAKER_CONFIG_MAX_PROFILES; ++index) {
        profile_observer_t *last = &observed[index];
        const moonraker_profile_t *profile = moonraker_config_profile(index);
        if (!profile || !profile->configured) {
            memset(last, 0, sizeof(*last));
            continue;
        }
        if (!last->configured || last->port != profile->port ||
            strcmp(last->host, profile->host)) {
            memset(last, 0, sizeof(*last));
            last->configured = true;
            last->port = profile->port;
            snprintf(last->host, sizeof(last->host), "%s", profile->host);
        }
        if (index == active) {
            /* Active notifications already have their own debounce. Keep the
             * confirmed baseline when that printer later becomes inactive. */
            if (active_state && active_state->moonraker_ok && active_state->live_data_ok) {
                last->armed = true;
                last->online = true;
            } else if (active_state && !active_state->live_data_ok) {
                last->online = false;
            }
            continue;
        }
        bool known = false;
        bool online = printer_profile_health_get(index, &known);
        if (!known) continue; /* Verification is not confirmed loss. */
        if (online && !printer_profile_health_live_state_fresh(index, 5000000LL)) continue;
        if (!last->armed) {
            if (online) last->armed = last->online = true;
            continue; /* Initial discovery is not a disconnect/recovery event. */
        }
        if (last->online == online) continue;
        last->online = online;
        /* Retire this printer's previous connection card, including one
         * captured before the operator selected another active profile. */
        unsigned kept = 0;
        for (unsigned i = 0; i < s_notice_count; ++i) {
            bool connection = s_notices[i].profile_index == index &&
                (!strcmp(s_notices[i].title, "PRINTER OFFLINE") ||
                 !strcmp(s_notices[i].title, "PRINTER RECONNECTED"));
            if (!connection) s_notices[kept++] = s_notices[i];
        }
        ++s_notice_epoch;s_notice_count = kept;
        toast_show_named(online ? UI_STATUS_INFO : UI_STATUS_WARNING,
            online ? "PRINTER RECONNECTED" : "PRINTER OFFLINE",
            online ? "Live status is available again. Open Printers to review this printer."
                   : "This printer is unavailable. A print may still be running; check the printer and its connection.",
            NOTICE_OPEN_PRINTERS, true, profile->name, index);
    }
}
