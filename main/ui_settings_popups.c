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

    s_reset_settings_popup =
        ui_popup_create(
            lv_screen_active(),
            680,
            330,
            UI_POPUP_DANGER);

    if (!s_reset_settings_popup) {
        ESP_LOGE(TAG, "Failed to create reset popup");
        return;
    }

    ui_popup_add_title(
        s_reset_settings_popup,
        ui_text("RESET SETTINGS?"),
        true,
        0);

    ui_popup_add_header_divider(
        s_reset_settings_popup,
        44);

    ui_popup_add_body(
        s_reset_settings_popup,
        "This will erase saved WiFi, Moonraker, OTA URL, "
        "and preferences.\n\n"
        "Firmware, OTA slots, and rollback recovery "
        "will NOT be erased.",
        20,
        62,
        600);

    ui_popup_add_standard_footer_divider(s_reset_settings_popup);

    ui_popup_add_footer_action(
        s_reset_settings_popup,
        UI_POPUP_ACTION_CANCEL,
        LV_SYMBOL_CLOSE " CANCEL",
        160,
        UI_POPUP_FOOTER_LEFT,
        reset_settings_cancel_cb,
        NULL,
        NULL);

    ui_popup_add_footer_action(
        s_reset_settings_popup,
        UI_POPUP_ACTION_DANGER,
        LV_SYMBOL_TRASH " ERASE",
        160,
        UI_POPUP_FOOTER_RIGHT,
        reset_settings_confirm_cb,
        NULL,
        NULL);
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

    s_timezone_popup = ui_popup_create(
        lv_layer_top(),
        760,
        500,
        UI_POPUP_STANDARD);

    if (!s_timezone_popup) {
        s_timezone_changed_cb = NULL;
        return;
    }

    ui_popup_add_title(
        s_timezone_popup,
        ui_text("TIME ZONE"),
        false,
        8);

    ui_popup_add_header_divider(s_timezone_popup, 44);

    ui_popup_add_status_label(
        s_timezone_popup,
        ui_text("Select local time zone. Daylight-saving rules apply automatically."),
        24,
        50,
        712);

    lv_obj_t *list = ui_popup_add_list(
        s_timezone_popup,
        24,
        82,
        712,
        336);

    if (!list) {
        timezone_popup_close();
        return;
    }

    size_t selected = timezone_config_selected_index();

    for (size_t index = 0;
         index < timezone_config_count();
         ++index) {
        const timezone_config_entry_t *entry =
            timezone_config_entry(index);

        if (!entry) {
            continue;
        }

        char text[96];
        snprintf(
            text,
            sizeof(text),
            "%s   |   %s",
            entry->label,
            entry->abbreviation);

        lv_obj_t *row = ui_popup_add_selectable_row(
            list,
            text,
            8,
            8 + (int32_t)index * 56,
            680,
            48,
            timezone_select_cb,
            (void *)(uintptr_t)index);

        ui_popup_set_selectable_row_selected(
            row,
            index == selected);
    }

    ui_popup_add_standard_footer_divider(s_timezone_popup);

    ui_popup_add_footer_action(
        s_timezone_popup,
        UI_POPUP_ACTION_CLOSE,
        LV_SYMBOL_CLOSE " CLOSE",
        160,
        UI_POPUP_FOOTER_CENTER,
        timezone_close_cb,
        NULL,
        NULL);
}

/* -------------------------------------------------------------------------
 * Interface theme selection
 * ------------------------------------------------------------------------- */

static void theme_popup_close(void)
{
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

static void custom_theme_apply_cb(lv_event_t *event)
{
    if (!event ||
        lv_event_get_code(event) != LV_EVENT_CLICKED ||
        s_theme_change_pending) {
        return;
    }

    size_t index =
        (size_t)(uintptr_t)lv_event_get_user_data(event);
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
    if (!event ||
        lv_event_get_code(event) != LV_EVENT_CLICKED ||
        s_custom_remove_popup) {
        return;
    }

    size_t index =
        (size_t)(uintptr_t)lv_event_get_user_data(event);
    const custom_theme_summary_t *summary =
        theme_manager_custom_summary(index);
    if (!summary) return;

    s_custom_remove_popup = ui_popup_create(
        lv_layer_top(), 620, 300, UI_POPUP_DANGER);
    if (!s_custom_remove_popup) return;

    ui_popup_add_title(
        s_custom_remove_popup,
        ui_text("REMOVE CUSTOM THEME?"),
        true,
        8);
    ui_popup_add_header_divider(
        s_custom_remove_popup,
        44);

    char message[220];
    lv_snprintf(
        message,
        sizeof(message),
        "Remove \"%s\" from the SD card?\n\n"
        "Built-in themes are protected and cannot be removed.",
        summary->name);
    ui_popup_add_body(
        s_custom_remove_popup,
        message,
        24, 68, 572);

    ui_popup_add_standard_footer_divider(
        s_custom_remove_popup);
    ui_popup_add_footer_action(
        s_custom_remove_popup,
        UI_POPUP_ACTION_CANCEL,
        LV_SYMBOL_CLOSE " KEEP",
        160,
        UI_POPUP_FOOTER_LEFT,
        custom_remove_cancel_cb,
        NULL,
        NULL);
    ui_popup_add_footer_action(
        s_custom_remove_popup,
        UI_POPUP_ACTION_DANGER,
        LV_SYMBOL_TRASH " REMOVE",
        180,
        UI_POPUP_FOOTER_RIGHT,
        custom_remove_confirm_cb,
        (void *)(uintptr_t)index,
        NULL);
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
        row, 392, 8, 150, 46,
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
    if (event && lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }

    if (s_custom_theme_popup) {
        lv_obj_move_foreground(s_custom_theme_popup);
        return;
    }

    theme_manager_scan_custom_themes();
    size_t count = theme_manager_custom_count();

    s_custom_theme_popup = ui_popup_create(
        lv_layer_top(), 820, 500, UI_POPUP_STANDARD);
    if (!s_custom_theme_popup) return;

    ui_popup_add_title(
        s_custom_theme_popup,
        ui_text("CUSTOM THEMES"),
        false,
        8);
    ui_popup_add_header_divider(
        s_custom_theme_popup,
        44);
    ui_popup_add_status_label(
        s_custom_theme_popup,
        count
            ? ui_text("Tap a theme to apply it. Remove deletes only the SD-card file.")
            : ui_text("No valid themes found in /sdcard/PrinterHMI/themes."),
        24, 50, 772);

    lv_obj_t *list = ui_popup_add_list(
        s_custom_theme_popup,
        24, 84, 772, 330);
    if (!list) {
        settings_popup_delete(&s_custom_theme_popup);
        return;
    }

    for (size_t index = 0; index < count; ++index) {
        const custom_theme_summary_t *summary =
            theme_manager_custom_summary(index);
        if (!summary) continue;

        int32_t y = 8 + (int32_t)index * 72;
        char label[180];
        lv_snprintf(
            label,
            sizeof(label),
            "%s\n%s%s%s",
            summary->name,
            custom_base_label(summary->base_theme),
            summary->author[0] ? "  |  " : "",
            summary->author);

        lv_obj_t *row = ui_popup_add_selectable_row(
            list,
            label,
            8, y, 560, 62,
            custom_theme_apply_cb,
            (void *)(uintptr_t)index);

        ui_popup_set_selectable_row_selected(
            row,
            theme_manager_custom_active() &&
            strcmp(custom_theme_active_id(),
                   summary->id) == 0);
        custom_theme_add_preview(
            row,
            summary);

        ui_popup_add_action_at(
            list,
            UI_POPUP_ACTION_DANGER,
            ui_text(LV_SYMBOL_TRASH " REMOVE"),
            584, y + 7, 156, 48,
            custom_theme_remove_cb,
            (void *)(uintptr_t)index,
            NULL);
    }

    ui_popup_add_standard_footer_divider(
        s_custom_theme_popup);
    ui_popup_add_footer_action(
        s_custom_theme_popup,
        UI_POPUP_ACTION_CLOSE,
        LV_SYMBOL_CLOSE " CLOSE",
        160,
        UI_POPUP_FOOTER_CENTER,
        custom_theme_close_cb,
        NULL,
        NULL);
}

void ui_settings_popups_show_theme(
    ui_settings_theme_changed_cb_t changed_cb)
{
    s_theme_changed_cb = changed_cb;

    if (s_theme_popup) {
        lv_obj_move_foreground(s_theme_popup);
        return;
    }

    s_theme_popup = ui_popup_create(
        lv_layer_top(),
        920,
        470,
        UI_POPUP_STANDARD);

    if (!s_theme_popup) {
        s_theme_changed_cb = NULL;
        return;
    }

    ui_popup_add_title(
        s_theme_popup,
        ui_text("INTERFACE THEME"),
        false,
        8);

    ui_popup_add_header_divider(s_theme_popup, 44);

    ui_popup_add_status_label(
        s_theme_popup,
        ui_text("Tap a preview to apply it across the interface."),
        24,
        50,
        872);

    static const struct {
        ui_theme_id_t id;
    } choices[] = {
        {UI_THEME_CLASSIC},
        {UI_THEME_OPERATOR},
        {UI_THEME_GLASS},
    };

    ui_theme_id_t selected = theme_manager_active();
    lv_obj_t *preview_grid = ui_popup_add_list(
        s_theme_popup, 24, 84, 872, 286);
    if (!preview_grid) {
        settings_popup_delete(&s_theme_popup);
        s_theme_changed_cb = NULL;
        return;
    }
    lv_obj_set_style_pad_all(preview_grid, 0, 0);
    lv_obj_set_style_bg_opa(preview_grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(preview_grid, 0, 0);

    /* First row stays at the original comfortable three-card width. */
    for (size_t index = 0;
         index < sizeof(choices) / sizeof(choices[0]);
         ++index) {
        ui_theme_preview_create(
            preview_grid,
            choices[index].id,
            !theme_manager_custom_active() &&
                choices[index].id == selected,
            8 + (int32_t)index * 288,
            6,
            276,
            250,
            theme_select_cb,
            (void *)(uintptr_t)choices[index].id);
    }

    ui_theme_preview_create(
        preview_grid,
        UI_THEME_FUTURE,
        !theme_manager_custom_active() &&
            UI_THEME_FUTURE == selected,
        296,
        268,
        276,
        250,
        theme_select_cb,
        (void *)(uintptr_t)UI_THEME_FUTURE);

    /* Swipe the preview area upward to reach the additional layout theme. */
    lv_obj_t *operator_shell_preview = ui_theme_preview_create(
        preview_grid,
        UI_THEME_OPERATOR_SHELL,
        !theme_manager_custom_active() &&
            UI_THEME_OPERATOR_SHELL == selected,
        8,
        268,
        276,
        250,
        operator_shell_select_cb,
        NULL);
    if (operator_shell_preview) {
        lv_obj_add_event_cb(operator_shell_preview,
                            operator_shell_select_cb,
                            LV_EVENT_PRESSED, NULL);
    }

    ui_popup_add_standard_footer_divider(s_theme_popup);

    ui_popup_add_footer_action(
        s_theme_popup,
        UI_POPUP_ACTION_SECONDARY,
        LV_SYMBOL_SD_CARD " CUSTOM THEMES",
        220,
        UI_POPUP_FOOTER_LEFT,
        custom_theme_manager_show_cb,
        NULL,
        NULL);

    ui_popup_add_footer_action(
        s_theme_popup,
        UI_POPUP_ACTION_CLOSE,
        LV_SYMBOL_CLOSE " CLOSE",
        160,
        UI_POPUP_FOOTER_RIGHT,
        theme_close_cb,
        NULL,
        NULL);
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
