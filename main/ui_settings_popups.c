#include "ui_settings_popups.h"
#include "ui_text.h"

#include "ui_popup.h"
#include "ui_theme.h"
#include "ui_theme_preview.h"
#include "timezone_config.h"
#include "theme_manager.h"

#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "ui_settings_popups";

static lv_obj_t *s_reset_settings_popup = NULL;
static lv_obj_t *s_timezone_popup = NULL;
static ui_settings_timezone_changed_cb_t s_timezone_changed_cb = NULL;
static lv_obj_t *s_theme_popup = NULL;
static lv_obj_t *s_custom_theme_popup = NULL;
static lv_obj_t *s_custom_remove_popup = NULL;
static ui_settings_theme_changed_cb_t s_theme_changed_cb = NULL;
static ui_settings_theme_changed_cb_t s_pending_theme_changed_cb = NULL;
static bool s_theme_change_pending = false;
static bool s_theme_open_pending = false;
static void theme_popup_create_async(void *user_data);
static int s_custom_selected_index = -1;
static char s_custom_selected_id[CUSTOM_THEME_ID_MAX + 1];
static char s_custom_remove_id[CUSTOM_THEME_ID_MAX + 1];
static lv_obj_t *s_custom_rows[CUSTOM_THEME_MAX_COUNT];
static lv_obj_t *s_custom_apply_action;
static lv_obj_t *s_custom_remove_action;
/* -------------------------------------------------------------------------
 * Shared close helper
 * ------------------------------------------------------------------------- */

static void settings_popup_delete(lv_obj_t **popup)
{
    if (!popup || !*popup) {
        return;
    }

    lv_obj_delete(*popup);
    *popup = NULL;
}

static lv_obj_t *settings_dialog_layout(lv_obj_t *popup, const char *title)
{
    lv_obj_set_flex_flow(popup, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(popup, UI_PAD_POPUP, 0);
    lv_obj_set_style_pad_row(popup, UI_GAP_CARD, 0);
    lv_obj_t *label = lv_label_create(popup);
    lv_label_set_text(label, title);
    ui_apply_custom_label_style(label, UI_FONT_TITLE, UI_TEXT_BRIGHT);
    lv_obj_set_width(label, LV_PCT(100));
    lv_obj_t *body = lv_obj_create(popup);
    lv_obj_remove_style_all(body);
    lv_obj_set_width(body, LV_PCT(100));
    lv_obj_set_height(body, 0);
    lv_obj_set_flex_grow(body, 1);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(body, UI_GAP_CARD, 0);
    lv_obj_add_flag(body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(body, LV_DIR_VER);
    return body;
}

static lv_obj_t *settings_dialog_footer(lv_obj_t *popup)
{
    lv_obj_t *footer = lv_obj_create(popup);
    lv_obj_remove_style_all(footer);
    lv_obj_set_size(footer, LV_PCT(100), 48);
    lv_obj_set_flex_flow(footer, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(footer, LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(footer, UI_GAP_CARD, 0);
    return footer;
}

static void theme_footer_fit(lv_obj_t *footer)
{
    lv_obj_set_height(footer, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(footer, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_row(footer, UI_GAP_ROW, 0);
    for (uint32_t i = 0; i < lv_obj_get_child_count(footer); ++i) {
        lv_obj_t *button = lv_obj_get_child(footer, i);
        lv_obj_t *label = lv_obj_get_child(button, 0);
        int32_t width = lv_obj_get_self_width(label) + 32;
        lv_obj_set_width(button, width);
        lv_obj_set_style_min_width(button, width, 0);
        lv_obj_set_flex_grow(button, 1);
    }
}

static int32_t settings_dialog_width(int32_t preferred)
{
    int32_t available = lv_display_get_horizontal_resolution(NULL) - 32;
    return available < preferred ? available : preferred;
}

static int32_t settings_dialog_height(int32_t preferred)
{
    int32_t available = lv_display_get_vertical_resolution(NULL) - 32;
    return available < preferred ? available : preferred;
}

static void settings_dialog_deleted(lv_event_t *event)
{
    lv_obj_t *popup = lv_event_get_target_obj(event);
    if (popup == s_custom_remove_popup) { s_custom_remove_popup = NULL; s_custom_remove_id[0] = 0; }
    if (popup == s_custom_theme_popup) {
        settings_popup_delete(&s_custom_remove_popup);
        s_custom_theme_popup = NULL; s_custom_selected_index = -1;
        s_custom_selected_id[0] = 0;
        s_custom_apply_action = s_custom_remove_action = NULL;
        memset(s_custom_rows, 0, sizeof(s_custom_rows));
    }
    if (popup == s_theme_popup) {
        settings_popup_delete(&s_custom_theme_popup);
        settings_popup_delete(&s_custom_remove_popup);
        s_theme_popup = NULL; s_theme_changed_cb = NULL;
    }
    if (popup == s_reset_settings_popup) s_reset_settings_popup = NULL;
    if (popup == s_timezone_popup) {
        s_timezone_popup = NULL;
        s_timezone_changed_cb = NULL;
    }
}

/* -------------------------------------------------------------------------
 * Factory reset confirmation
 * ------------------------------------------------------------------------- */

static void reset_settings_cancel_cb(lv_event_t *e)
{
    (void)e;

    settings_popup_delete(&s_reset_settings_popup);
}

static void reset_settings_confirm_cb(lv_event_t *e)
{
    (void)e;

    ESP_LOGW(
        TAG,
        "RESET SETTINGS: erasing NVS and rebooting");

    nvs_flash_erase();

    vTaskDelay(pdMS_TO_TICKS(300));

    esp_restart();
}

void reset_settings_cb(lv_event_t *e)
{
    (void)e;

    if (s_reset_settings_popup) {
        lv_obj_move_foreground(s_reset_settings_popup);
        return;
    }

    s_reset_settings_popup = ui_popup_create(lv_layer_top(), settings_dialog_width(680),
        settings_dialog_height(330), UI_POPUP_DANGER);
    if (!s_reset_settings_popup) {
        ESP_LOGE(TAG, "Failed to create reset popup"); return;
    }
    lv_obj_add_event_cb(s_reset_settings_popup, settings_dialog_deleted, LV_EVENT_DELETE, NULL);
    lv_obj_t *body = settings_dialog_layout(s_reset_settings_popup, ui_text("RESET SETTINGS?"));
    lv_obj_t *warning = lv_label_create(body);
    lv_label_set_text(warning, "This will erase saved WiFi, Moonraker, OTA URL, "
        "and preferences.\n\nFirmware, OTA slots, and rollback recovery "
        "will NOT be erased.");
    ui_apply_custom_label_style(warning, UI_FONT_BODY, UI_TEXT);
    lv_obj_set_width(warning, LV_PCT(100));
    lv_obj_t *footer = settings_dialog_footer(s_reset_settings_popup);
    lv_obj_t *cancel = ui_popup_add_action_at(footer, UI_POPUP_ACTION_CANCEL, LV_SYMBOL_CLOSE " CANCEL",
        0, 0, 160, 48, reset_settings_cancel_cb, NULL, NULL);
    lv_obj_t *erase = ui_popup_add_action_at(footer, UI_POPUP_ACTION_DANGER, LV_SYMBOL_TRASH " ERASE",
        0, 0, 160, 48, reset_settings_confirm_cb, NULL, NULL);
    lv_obj_set_flex_grow(cancel, 1); lv_obj_set_flex_grow(erase, 1);
}

/* -------------------------------------------------------------------------
 * Timezone selection
 * ------------------------------------------------------------------------- */

static void timezone_popup_close(void)
{
    settings_popup_delete(&s_timezone_popup);
    s_timezone_changed_cb = NULL;
}

static void timezone_close_cb(lv_event_t *event)
{
    (void)event;
    timezone_popup_close();
}

static void timezone_select_cb(lv_event_t *event)
{
    if (!event || lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }

    size_t index = (size_t)(uintptr_t)lv_event_get_user_data(event);

    if (!timezone_config_select(index)) {
        ESP_LOGE(TAG, "Could not select timezone index %u", (unsigned)index);
        return;
    }

    if (s_timezone_changed_cb) {
        s_timezone_changed_cb();
    }

    timezone_popup_close();
}

void ui_settings_popups_show_timezone(
    ui_settings_timezone_changed_cb_t changed_cb)
{
    s_timezone_changed_cb = changed_cb;

    if (s_timezone_popup) {
        lv_obj_move_foreground(s_timezone_popup);
        return;
    }

    s_timezone_popup = ui_popup_create(lv_layer_top(), settings_dialog_width(760),
        settings_dialog_height(500), UI_POPUP_STANDARD);
    if (!s_timezone_popup) { s_timezone_changed_cb = NULL; return; }
    lv_obj_add_event_cb(s_timezone_popup, settings_dialog_deleted, LV_EVENT_DELETE, NULL);
    lv_obj_t *body = settings_dialog_layout(s_timezone_popup, ui_text("TIME ZONE"));
    lv_obj_t *hint = lv_label_create(body);
    lv_label_set_text(hint, ui_text("Select local time zone. Daylight-saving rules apply automatically."));
    ui_apply_custom_label_style(hint, UI_FONT_BODY, UI_TEXT_MUTED);
    lv_obj_set_width(hint, LV_PCT(100));
    size_t selected = timezone_config_selected_index();
    for (size_t index = 0; index < timezone_config_count(); ++index) {
        const timezone_config_entry_t *entry = timezone_config_entry(index);
        if (!entry) continue;
        lv_obj_t *row = ui_popup_add_selectable_row(body, entry->label,
            0, 0, 1, 48, timezone_select_cb, (void *)(uintptr_t)index);
        if (!row) continue;
        ui_popup_set_selectable_row_selected(row, index == selected);
        lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_style_min_height(row, 48, 0);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_hor(row, UI_PAD_CARD, 0);
        lv_obj_set_style_pad_ver(row, 10, 0);
        lv_obj_t *label = lv_obj_get_child(row, 0);
        lv_label_set_text_fmt(label, "%s   |   %s", entry->label, entry->abbreviation);
        lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(label, LV_PCT(100));
    }
    lv_obj_t *footer = settings_dialog_footer(s_timezone_popup);
    ui_popup_add_action_at(footer, UI_POPUP_ACTION_CLOSE, LV_SYMBOL_CLOSE " CLOSE",
        0, 0, 160, 48, timezone_close_cb, NULL, NULL);
}

/* -------------------------------------------------------------------------
 * Interface theme selection
 * ------------------------------------------------------------------------- */

static void theme_popup_close(void)
{
    if (s_theme_open_pending) {
        lv_async_call_cancel(theme_popup_create_async, NULL);
        s_theme_open_pending = false;
    }
    settings_popup_delete(&s_theme_popup);
    settings_popup_delete(&s_custom_theme_popup);
    settings_popup_delete(&s_custom_remove_popup);
    s_theme_changed_cb = NULL;
}

static void theme_close_cb(lv_event_t *event)
{
    (void)event;
    theme_popup_close();
}

static void theme_changed_async_cb(void *user_data)
{
    (void)user_data;

    ui_settings_theme_changed_cb_t changed_cb =
        s_pending_theme_changed_cb;

    s_pending_theme_changed_cb = NULL;
    s_theme_change_pending = false;

    /* Event dispatch has finished; popup and page objects are now safe. */
    theme_popup_close();

    if (changed_cb) {
        changed_cb();
    }
}

static void theme_select_cb(lv_event_t *event)
{
    if (!event || lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }

    if (s_theme_change_pending) {
        return;
    }

    ui_theme_id_t theme =
        (ui_theme_id_t)(uintptr_t)lv_event_get_user_data(event);

    if (!theme_manager_custom_active() &&
        theme == theme_manager_active()) {
        theme_popup_close();
        return;
    }

    if (!theme_manager_select(theme)) {
        ESP_LOGE(TAG, "Could not select theme %d", (int)theme);
        return;
    }

    s_pending_theme_changed_cb =
        s_theme_changed_cb;
    s_theme_change_pending = true;

    /* Rebuilding here would delete objects still owned by this event stack. */
    lv_async_call(theme_changed_async_cb, NULL);
}

static void operator_shell_select_cb(lv_event_t *event)
{
    if (!event || s_theme_change_pending) {
        return;
    }

    lv_event_code_t code = lv_event_get_code(event);
    if (code != LV_EVENT_CLICKED && code != LV_EVENT_PRESSED) {
        return;
    }

    if (!theme_manager_select(UI_THEME_OPERATOR_SHELL)) {
        ESP_LOGE(TAG, "Could not select Operator Shell theme");
        return;
    }

    s_pending_theme_changed_cb = s_theme_changed_cb;
    s_theme_change_pending = true;
    lv_async_call(theme_changed_async_cb, NULL);
}

static void custom_theme_close_cb(lv_event_t *event)
{
    (void)event;
    settings_popup_delete(&s_custom_theme_popup);
}

static int custom_selected_index(void)
{
    if (s_custom_selected_index < 0) return -1;
    const custom_theme_summary_t *summary = theme_manager_custom_summary((size_t)s_custom_selected_index);
    return summary && !strcmp(summary->id, s_custom_selected_id) ? s_custom_selected_index : -1;
}
static void custom_theme_select_row_cb(lv_event_t *event)
{
    if (!event || lv_event_get_code(event) != LV_EVENT_CLICKED || s_theme_change_pending) return;
    size_t index = (size_t)(uintptr_t)lv_event_get_user_data(event);
    if (index >= CUSTOM_THEME_MAX_COUNT) return;
    const custom_theme_summary_t *summary = theme_manager_custom_summary(index);
    if (!summary) return;
    s_custom_selected_index = (int)index;
    snprintf(s_custom_selected_id, sizeof(s_custom_selected_id), "%s", summary->id);
    for (size_t i = 0; i < CUSTOM_THEME_MAX_COUNT; ++i)
        if (s_custom_rows[i]) ui_popup_set_selectable_row_selected(s_custom_rows[i], i == index);
    if (s_custom_apply_action) lv_obj_remove_state(s_custom_apply_action, LV_STATE_DISABLED);
    if (s_custom_remove_action) lv_obj_remove_state(s_custom_remove_action, LV_STATE_DISABLED);
}

static void custom_theme_apply_cb(lv_event_t *event)
{
    if (!event ||
        lv_event_get_code(event) != LV_EVENT_CLICKED ||
        s_theme_change_pending) {
        return;
    }

    int selected = custom_selected_index();
    if (selected < 0) return;
    size_t index = (size_t)selected;
    if (!theme_manager_select_custom(index)) {
        ESP_LOGE(TAG, "Could not select custom theme %u",
                 (unsigned)index);
        return;
    }

    s_pending_theme_changed_cb = s_theme_changed_cb;
    s_theme_change_pending = true;
    lv_async_call(theme_changed_async_cb, NULL);
}

static void custom_remove_cancel_cb(lv_event_t *event)
{
    (void)event;
    settings_popup_delete(&s_custom_remove_popup);
}

static void custom_remove_confirm_cb(lv_event_t *event)
{
    if (!event || lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }

    size_t index =
        (size_t)(uintptr_t)lv_event_get_user_data(event);
    const custom_theme_summary_t *summary =
        theme_manager_custom_summary(index);
    if (!summary || strcmp(summary->id, s_custom_remove_id)) return;
    bool removing_active =
        summary &&
        theme_manager_custom_active() &&
        strcmp(custom_theme_active_id(),
               summary->id) == 0;

    if (!theme_manager_remove_custom(index)) {
        ESP_LOGE(TAG, "Could not remove custom theme %u",
                 (unsigned)index);
        return;
    }

    settings_popup_delete(&s_custom_remove_popup);
    settings_popup_delete(&s_custom_theme_popup);

    if (removing_active) {
        s_pending_theme_changed_cb = s_theme_changed_cb;
        s_theme_change_pending = true;
        lv_async_call(theme_changed_async_cb, NULL);
    }
}

static void custom_theme_remove_cb(lv_event_t *event)
{
    if (!event || lv_event_get_code(event) != LV_EVENT_CLICKED || s_custom_remove_popup || s_theme_change_pending) return;
    int selected = custom_selected_index();
    if (selected < 0) return;
    size_t index = (size_t)selected;
    const custom_theme_summary_t *summary = theme_manager_custom_summary(index);
    if (!summary) return;
    snprintf(s_custom_remove_id, sizeof(s_custom_remove_id), "%s", summary->id);
    s_custom_remove_popup = ui_popup_create(lv_layer_top(), settings_dialog_width(620), settings_dialog_height(330), UI_POPUP_DANGER);
    if (!s_custom_remove_popup) return;
    lv_obj_add_event_cb(s_custom_remove_popup, settings_dialog_deleted, LV_EVENT_DELETE, NULL);
    lv_obj_t *body = settings_dialog_layout(s_custom_remove_popup, ui_text("REMOVE CUSTOM THEME?"));
    lv_obj_t *message = lv_label_create(body);
    lv_label_set_text_fmt(message, "Remove \"%s\" from the SD card?\n\nBuilt-in themes are protected and cannot be removed.", summary->name);
    ui_apply_custom_label_style(message, UI_FONT_BODY, UI_TEXT);
    lv_obj_set_width(message, LV_PCT(100));
    lv_obj_t *footer = settings_dialog_footer(s_custom_remove_popup);
    lv_obj_t *keep = ui_popup_add_action_at(footer, UI_POPUP_ACTION_CANCEL, LV_SYMBOL_CLOSE " KEEP",
        0, 0, 160, 48, custom_remove_cancel_cb, NULL, NULL);
    lv_obj_t *remove = ui_popup_add_action_at(footer, UI_POPUP_ACTION_DANGER, LV_SYMBOL_TRASH " REMOVE",
        0, 0, 180, 48, custom_remove_confirm_cb, (void *)(uintptr_t)index, NULL);
    lv_obj_set_flex_grow(keep, 1); lv_obj_set_flex_grow(remove, 1);
    theme_footer_fit(footer);
}

static const char *custom_base_label(ui_theme_id_t theme)
{
    switch (theme) {
        case UI_THEME_CLASSIC: return "Foundry base";
        case UI_THEME_GLASS: return "Dark Glass base";
        case UI_THEME_OPERATOR:
        default: return "Operator base";
    }
}

static lv_obj_t *custom_preview_rect(
    lv_obj_t *parent,
    int32_t x,
    int32_t y,
    int32_t width,
    int32_t height,
    uint32_t color,
    int32_t radius)
{
    lv_obj_t *object = lv_obj_create(parent);
    if (!object) return NULL;
    lv_obj_set_size(object, width, height);
    lv_obj_set_pos(object, x, y);
    lv_obj_clear_flag(object, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(object, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_pad_all(object, 0, 0);
    lv_obj_set_style_bg_color(
        object,
        lv_color_hex(color),
        0);
    lv_obj_set_style_bg_opa(object, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(object, 0, 0);
    lv_obj_set_style_radius(object, radius, 0);
    return object;
}

static void custom_theme_add_preview(
    lv_obj_t *row,
    const custom_theme_summary_t *summary)
{
    if (!row || !summary) return;

    lv_obj_t *frame = custom_preview_rect(
        row, 0, 0, 150, 46,
        summary->preview_background, 6);
    if (!frame) return;

    lv_obj_t *card = custom_preview_rect(
        frame, 8, 7, 92, 32,
        summary->preview_card, 5);
    if (card) {
        custom_preview_rect(
            card, 8, 8, 54, 4,
            summary->preview_text, 2);
        custom_preview_rect(
            card, 8, 20, 72, 5,
            summary->preview_accent, 2);
    }

    custom_preview_rect(
        frame, 108, 8, 34, 12,
        summary->preview_accent, 4);
    custom_preview_rect(
        frame, 108, 27, 34, 12,
        summary->preview_card, 4);
}

static void custom_theme_manager_show_cb(lv_event_t *event)
{
    if (event && lv_event_get_code(event) != LV_EVENT_CLICKED) return;
    if (s_theme_change_pending) return;
    if (s_custom_theme_popup) { lv_obj_move_foreground(s_custom_theme_popup); return; }
    theme_manager_scan_custom_themes();
    size_t count = theme_manager_custom_count();
    if (count > CUSTOM_THEME_MAX_COUNT) count = CUSTOM_THEME_MAX_COUNT;
    s_custom_selected_index = -1; s_custom_selected_id[0] = 0;
    s_custom_theme_popup = ui_popup_create(lv_layer_top(), settings_dialog_width(820), settings_dialog_height(520), UI_POPUP_STANDARD);
    if (!s_custom_theme_popup) return;
    lv_obj_add_event_cb(s_custom_theme_popup, settings_dialog_deleted, LV_EVENT_DELETE, NULL);
    lv_obj_t *body = settings_dialog_layout(s_custom_theme_popup, ui_text("CUSTOM THEMES"));
    lv_obj_t *hint = lv_label_create(body);
    lv_label_set_text(hint, count ? ui_text("Select a theme, then Apply. Remove deletes only its SD-card file.") :
        ui_text("No valid themes found in /sdcard/PrinterHMI/themes."));
    ui_apply_custom_label_style(hint, UI_FONT_BODY, UI_TEXT_MUTED);
    lv_obj_set_width(hint, LV_PCT(100));
    for (size_t index = 0; index < count; ++index) {
        const custom_theme_summary_t *summary = theme_manager_custom_summary(index);
        if (!summary) continue;
        lv_obj_t *row = ui_popup_add_selectable_row(body, summary->name, 0, 0, 1, 48,
            custom_theme_select_row_cb, (void *)(uintptr_t)index);
        if (!row) continue;
        s_custom_rows[index] = row;
        lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_all(row, UI_PAD_CARD, 0);
        lv_obj_set_style_pad_row(row, UI_GAP_ROW, 0);
        lv_obj_t *label = lv_obj_get_child(row, 0);
        lv_label_set_text_fmt(label, "%s\n%s%s%s%s", summary->name, custom_base_label(summary->base_theme),
            summary->author[0] ? "  |  " : "", summary->author,
            theme_manager_custom_active() && !strcmp(custom_theme_active_id(), summary->id) ? "  [ACTIVE]" : "");
        lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(label, LV_PCT(100));
        if (summary->description[0]) {
            lv_obj_t *description = lv_label_create(row);
            lv_label_set_text(description, summary->description);
            ui_apply_custom_label_style(description, UI_FONT_BODY, UI_TEXT_MUTED);
            lv_obj_set_width(description, LV_PCT(100));
        }
        custom_theme_add_preview(row, summary);
    }
    lv_obj_t *footer = settings_dialog_footer(s_custom_theme_popup);
    lv_obj_set_height(footer, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(footer, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_row(footer, UI_GAP_ROW, 0);
    int32_t width = ui_theme_density_metric(136, 144, 152);
    lv_obj_t *close = ui_popup_add_action_at(footer, UI_POPUP_ACTION_CLOSE, LV_SYMBOL_CLOSE " CLOSE",
        0, 0, width, 48, custom_theme_close_cb, NULL, NULL);
    s_custom_apply_action = ui_popup_add_action_at(footer, UI_POPUP_ACTION_CONFIRM, LV_SYMBOL_OK " APPLY",
        0, 0, width, 48, custom_theme_apply_cb, NULL, NULL);
    s_custom_remove_action = ui_popup_add_action_at(footer, UI_POPUP_ACTION_DANGER, LV_SYMBOL_TRASH " REMOVE",
        0, 0, width, 48, custom_theme_remove_cb, NULL, NULL);
    lv_obj_set_flex_grow(close, 1); lv_obj_set_flex_grow(s_custom_apply_action, 1); lv_obj_set_flex_grow(s_custom_remove_action, 1);
    theme_footer_fit(footer);
    lv_obj_add_state(s_custom_apply_action, LV_STATE_DISABLED); lv_obj_add_state(s_custom_remove_action, LV_STATE_DISABLED);
}

void ui_settings_popups_show_theme(ui_settings_theme_changed_cb_t changed_cb)
{
    if (s_theme_change_pending) return;
    s_theme_changed_cb = changed_cb;
    if (s_theme_popup) { lv_obj_move_foreground(s_theme_popup); return; }
    if (s_theme_open_pending) return;
    /* Building the preview/flex tree inside pointer event dispatch stacks
     * layout and label-measurement frames on top of the input event chain. */
    s_theme_open_pending = true;
    if (lv_async_call(theme_popup_create_async, NULL) != LV_RESULT_OK) {
        s_theme_open_pending = false;
        s_theme_changed_cb = NULL;
        ESP_LOGE(TAG, "Could not schedule theme chooser");
    }
}

static void theme_popup_create_async(void *user_data)
{
    (void)user_data;
    s_theme_open_pending = false;
    if (s_theme_popup || s_theme_change_pending) return;
    s_theme_popup = ui_popup_create(lv_layer_top(), settings_dialog_width(920), settings_dialog_height(520), UI_POPUP_STANDARD);
    if (!s_theme_popup) { s_theme_changed_cb = NULL; return; }
    lv_obj_add_event_cb(s_theme_popup, settings_dialog_deleted, LV_EVENT_DELETE, NULL);
    lv_obj_t *body = settings_dialog_layout(s_theme_popup, ui_text("INTERFACE THEME"));
    lv_obj_t *hint = lv_label_create(body);
    lv_label_set_text(hint, ui_text("Tap a preview to apply it across the interface."));
    ui_apply_custom_label_style(hint, UI_FONT_BODY, UI_TEXT_MUTED);
    lv_obj_set_width(hint, LV_PCT(100));
    lv_obj_t *grid = lv_obj_create(body);
    lv_obj_remove_style_all(grid);
    lv_obj_set_size(grid, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(grid, UI_GAP_CARD, 0);
    lv_obj_set_style_pad_row(grid, UI_GAP_CARD, 0);
    for (int theme = UI_THEME_CLASSIC; theme <= UI_THEME_STUDIO_DARK; ++theme) {
        lv_obj_t *preview = ui_theme_preview_create(grid, theme,
            !theme_manager_custom_active() && theme == (int)theme_manager_active(), 0, 0, 340, 250,
            theme == UI_THEME_OPERATOR_SHELL ? operator_shell_select_cb : theme_select_cb, (void *)(uintptr_t)theme);
        if (preview && theme == UI_THEME_OPERATOR_SHELL)
            lv_obj_add_event_cb(preview, operator_shell_select_cb, LV_EVENT_PRESSED, NULL);
    }
    lv_obj_t *footer = settings_dialog_footer(s_theme_popup);
    lv_obj_set_height(footer, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(footer, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_row(footer, UI_GAP_ROW, 0);
    lv_obj_t *custom = ui_popup_add_action_at(footer, UI_POPUP_ACTION_SECONDARY, LV_SYMBOL_SD_CARD " CUSTOM THEMES",
        0, 0, ui_theme_density_metric(236, 248, 260), 48, custom_theme_manager_show_cb, NULL, NULL);
    lv_obj_t *close = ui_popup_add_action_at(footer, UI_POPUP_ACTION_CLOSE, LV_SYMBOL_CLOSE " CLOSE",
        0, 0, 144, 48, theme_close_cb, NULL, NULL);
    lv_obj_set_flex_grow(custom, 1); lv_obj_set_flex_grow(close, 1);
    theme_footer_fit(footer);
}

/* -------------------------------------------------------------------------
 * Page cleanup
 * ------------------------------------------------------------------------- */

void ui_settings_popups_close_all(void)
{
    settings_popup_delete(&s_reset_settings_popup);
    timezone_popup_close();
    theme_popup_close();
}
