#include "ui_toast.h"

#include <stdio.h>
#include <string.h>
#include "nvs.h"
#include "operator_event_log.h"
#include "moonraker_config_controller.h"
#include "ui_button.h"
#include "ui_popup.h"
#include "ui_text_fit.h"

#define NOTICE_CAPACITY 4
#define NOTICE_NVS "notifications"
#define NOTICE_KEY "confirm"

typedef struct {
    ui_status_kind_t kind;
    int destination;
    uint32_t owner;
    char title[80], detail[320], printer[80];
} notice_t;
static notice_t s_notices[NOTICE_CAPACITY];
static unsigned s_notice_count, s_overflow;
static bool s_confirmations; /* Quiet default; errors are never disabled. */
static lv_obj_t *s_toast, *s_preference_popup, *s_preference_value, *s_preference_status;
static lv_timer_t *s_timer;
static bool s_pending_confirmations;
static void (*s_preference_changed)(void);

static void surface_deleted(lv_event_t *event)
{
    (void)event;
    s_toast = NULL;
    if (s_timer) { lv_timer_delete(s_timer); s_timer = NULL; }
}
static void close_surface(void)
{
    if (s_timer) { lv_timer_delete(s_timer); s_timer = NULL; }
    if (s_toast) { lv_obj_t *object = s_toast; s_toast = NULL; lv_obj_delete(object); }
}
void ui_toast_close(void)
{
    close_surface();
    s_notice_count = s_overflow = 0;
}
void ui_toast_init(void)
{
    nvs_handle_t handle;
    s_confirmations = false;
    if (nvs_open(NOTICE_NVS, NVS_READONLY, &handle) == ESP_OK) {
        uint8_t value = 0;
        if (nvs_get_u8(handle, NOTICE_KEY, &value) == ESP_OK && value <= 1)
            s_confirmations = value != 0;
        nvs_close(handle);
    }
}
bool ui_toast_confirmations_enabled(void) { return s_confirmations; }
const char *ui_toast_preference_label(void) { return s_confirmations ? "ON" : "OFF"; }
bool ui_toast_set_confirmations(bool enabled)
{
    nvs_handle_t handle;
    if (nvs_open(NOTICE_NVS, NVS_READWRITE, &handle) != ESP_OK) return false;
    esp_err_t error = nvs_set_u8(handle, NOTICE_KEY, enabled);
    if (error == ESP_OK) error = nvs_commit(handle);
    nvs_close(handle);
    if (error != ESP_OK) return false;
    s_confirmations = enabled;
    if (!enabled && !s_notice_count) close_surface();
    return true;
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
static void render_notice(void);
static void dismiss_notice(lv_event_t *event);
static void open_notice(lv_event_t *event)
{
    (void)event;
    if(!s_notice_count)return;
    notice_t notice=s_notices[0];
    if(notice.owner!=moonraker_config_generation() || notice.destination<0 || notice.destination>=UI_SHELL_PAGE_COUNT) {
        lv_obj_t *body=lv_obj_get_child(s_toast,0);
        ui_popup_feedback(body,UI_STATUS_WARNING,"PRINTER CHANGED","Printer context changed. Open Console from the sidebar for the printer named in this notice.");
        return;
    }
    dismiss_notice(NULL);
    ui_shell_page_action((ui_shell_page_t)notice.destination);
    ui_shell_set_active_nav(notice.destination>=UI_SHELL_PAGE_BED_MESH ? UI_SHELL_PAGE_TOOLS : notice.destination);
}

static void dismiss_notice(lv_event_t *event)
{
    (void)event;
    close_surface();
    if (s_notice_count) {
        --s_notice_count;
        memmove(s_notices, s_notices + 1, s_notice_count * sizeof(s_notices[0]));
    }
    if (s_notice_count) render_notice();
    else s_overflow = 0;
}
static void render_notice(void)
{
    close_surface();
    if (!s_notice_count) return;
    const notice_t *notice = &s_notices[0];
    int width = lv_display_get_horizontal_resolution(NULL) - 32;
    int height = lv_display_get_vertical_resolution(NULL) - 32;
    if (width > 520) width = 520;
    if (height > 260) height = 260;
    s_toast = lv_obj_create(lv_layer_top());
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
    lv_obj_set_height(body, 0);
    lv_obj_set_flex_grow(body, 1);
    lv_obj_add_flag(body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(body, LV_DIR_VER);
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
    if(notice->destination>=0) {
        const char *name=notice->destination==UI_SHELL_PAGE_CONSOLE ? "OPEN CONSOLE" : "OPEN PAGE";
        lv_obj_t *open=ui_button_create(footer,UI_BUTTON_OUTLINED,name);
        lv_obj_set_size(open,144,48);
        ui_text_fit_single_line(lv_obj_get_child(open,0),UI_FONT_BODY);
        lv_obj_add_event_cb(open,open_notice,LV_EVENT_CLICKED,NULL);
    }
    lv_obj_t *close = ui_button_create(footer, UI_BUTTON_CLOSE, "DISMISS");
    lv_obj_set_size(close, 128, 48);
    lv_obj_add_event_cb(close, dismiss_notice, LV_EVENT_CLICKED, NULL);
    ui_text_fit_single_line(lv_obj_get_child(close, 0), UI_FONT_BODY_LARGE);
    lv_label_set_long_mode(lv_obj_get_child(close, 0), LV_LABEL_LONG_CLIP);
}
static void toast_timer_cb(lv_timer_t *timer)
{
    (void)timer; close_surface();
}
static bool optional(ui_status_kind_t kind)
{
    return kind == UI_STATUS_OK || kind == UI_STATUS_NEUTRAL;
}
static void toast_show(ui_status_kind_t kind, const char *title, const char *detail, int destination)
{
    const char *printer = moonraker_config_active_profile_name();
    if (!printer || !printer[0]) printer = "PrinterHMI";
    if (optional(kind)) {
        if (!s_confirmations || s_notice_count) return;
        close_surface();
        int width = lv_display_get_horizontal_resolution(NULL) - 32;
        if (width > 520) width = 520;
        s_toast = lv_obj_create(lv_layer_top());
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
    notice_t notice = {.kind = kind, .destination = destination, .owner = moonraker_config_generation()};
    snprintf(notice.title, sizeof(notice.title), "%s", title ? title : "");
    snprintf(notice.detail, sizeof(notice.detail), "%s", detail ? detail : "");
    snprintf(notice.printer, sizeof(notice.printer), "%s", printer);
    for (unsigned i = 0; i < s_notice_count; ++i) {
        if (s_notices[i].kind == kind && s_notices[i].destination==destination && s_notices[i].owner==notice.owner && !strcmp(s_notices[i].title, notice.title) &&
            !strcmp(s_notices[i].detail, notice.detail) && !strcmp(s_notices[i].printer, notice.printer)) {
            if (!s_toast) render_notice();
            return;
        }
    }
    operator_event_log_add(kind == UI_STATUS_DANGER ? OPERATOR_EVENT_ERROR :
        kind == UI_STATUS_WARNING ? OPERATOR_EVENT_WARNING : OPERATOR_EVENT_INFO,
        "%s: %s | %s", printer, notice.title, notice.detail);
    if (s_notice_count == NOTICE_CAPACITY) {
        ++s_overflow;
        if (kind != UI_STATUS_DANGER) { render_notice(); return; }
        --s_notice_count; /* Retire the last pending card, already recorded in the event ring. */
    }
    unsigned position = kind == UI_STATUS_DANGER ? 0 : s_notice_count;
    memmove(s_notices + position + 1, s_notices + position,
        (s_notice_count - position) * sizeof(s_notices[0]));
    s_notices[position] = notice; ++s_notice_count;
    render_notice();
}
void ui_toast_show(ui_status_kind_t kind,const char *title,const char *detail)
{
    toast_show(kind,title,detail,-1);
}
void ui_toast_show_link(ui_status_kind_t kind,const char *title,const char *detail,ui_shell_page_t page)
{
    toast_show(kind,title,detail,page>=0 && page<UI_SHELL_PAGE_COUNT ? (int)page : -1);
}
static void preferences_deleted(lv_event_t *event)
{
    (void)event;
    s_preference_popup = s_preference_value = s_preference_status = NULL;
    s_preference_changed = NULL;
}
void ui_toast_preferences_close(void)
{
    if (s_preference_popup) lv_obj_delete(s_preference_popup);
}
static void preference_close(lv_event_t *event) { (void)event; ui_toast_preferences_close(); }
static void preference_toggle(lv_event_t *event)
{
    (void)event; s_pending_confirmations = !s_pending_confirmations;
    lv_label_set_text(s_preference_value, s_pending_confirmations ? "CONFIRMATIONS ON" : "CONFIRMATIONS OFF");
}
static void preference_save(lv_event_t *event)
{
    (void)event;
    if (!ui_toast_set_confirmations(s_pending_confirmations)) {
        lv_label_set_text(s_preference_status, "Could not save. Your previous setting is unchanged.");
        return;
    }
    void (*changed)(void) = s_preference_changed;
    ui_toast_preferences_close();
    if (changed) changed();
}
void ui_toast_preferences_show(void (*changed)(void))
{
    if (s_preference_popup) { lv_obj_move_foreground(s_preference_popup); return; }
    int width = lv_display_get_horizontal_resolution(NULL) - 32;
    int height = lv_display_get_vertical_resolution(NULL) - 32;
    if (width > 640) width = 640;
    if (height > 360) height = 360;
    s_preference_popup = ui_popup_create(lv_layer_top(), width, height, UI_POPUP_STANDARD);
    if (!s_preference_popup) return;
    s_preference_changed = changed; s_pending_confirmations = s_confirmations;
    lv_obj_add_event_cb(s_preference_popup, preferences_deleted, LV_EVENT_DELETE, NULL);
    lv_obj_set_flex_flow(s_preference_popup, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(s_preference_popup, 12, 0);
    lv_obj_set_style_pad_row(s_preference_popup, 8, 0);
    lv_obj_remove_flag(s_preference_popup, LV_OBJ_FLAG_SCROLLABLE);
    text(s_preference_popup, "NOTIFICATIONS", UI_FONT_TITLE, UI_TEXT);
    lv_obj_t *body = column(s_preference_popup);
    lv_obj_set_height(body, 0); lv_obj_set_flex_grow(body, 1);
    lv_obj_add_flag(body, LV_OBJ_FLAG_SCROLLABLE); lv_obj_set_scroll_dir(body, LV_DIR_VER);
    text(body, "Optional confirmations apply to every printer, page and theme. OFF keeps routine actions quiet. Errors, warnings and recovery instructions stay visible until dismissed.", UI_FONT_BODY, UI_TEXT);
    lv_obj_t *toggle = ui_button_create(body, UI_BUTTON_OUTLINED, s_confirmations ? "CONFIRMATIONS ON" : "CONFIRMATIONS OFF");
    lv_obj_set_size(toggle, LV_PCT(100), 48);
    s_preference_value = lv_obj_get_child(toggle, 0);
    ui_text_fit_single_line(s_preference_value, UI_FONT_BODY_LARGE);
    lv_label_set_long_mode(s_preference_value, LV_LABEL_LONG_CLIP);
    lv_obj_add_event_cb(toggle, preference_toggle, LV_EVENT_CLICKED, NULL);
    s_preference_status = text(body, "Dismiss acknowledges a notice; it does not clear a printer fault.", UI_FONT_CAPTION, UI_TEXT_DIM);
    lv_obj_t *footer = column(s_preference_popup);
    lv_obj_set_flex_flow(footer, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(footer, 8, 0);
    lv_obj_t *cancel = ui_button_create(footer, UI_BUTTON_CANCEL, "CANCEL");
    lv_obj_set_size(cancel, 128, 48); lv_obj_add_event_cb(cancel, preference_close, LV_EVENT_CLICKED, NULL);
    lv_obj_t *save = ui_button_create(footer, UI_BUTTON_PRIMARY, "SAVE");
    lv_obj_set_size(save, 128, 48); lv_obj_add_event_cb(save, preference_save, LV_EVENT_CLICKED, NULL);
}
