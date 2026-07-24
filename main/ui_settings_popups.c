#include "ui_settings_popups.h"

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

static const char *TAG = "ui_settings_popups";

static lv_obj_t *s_reset_settings_popup = NULL;
static lv_obj_t *s_timezone_popup = NULL;
static ui_settings_timezone_changed_cb_t s_timezone_changed_cb = NULL;
static lv_obj_t *s_theme_popup = NULL;
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
        "RESET SETTINGS?",
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
        "TIME ZONE",
        false,
        8);

    ui_popup_add_header_divider(s_timezone_popup, 44);

    ui_popup_add_status_label(
        s_timezone_popup,
        "Select local time zone. Daylight-saving rules apply automatically.",
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

    if (theme == theme_manager_active()) {
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
        "INTERFACE THEME",
        false,
        8);

    ui_popup_add_header_divider(s_theme_popup, 44);

    ui_popup_add_status_label(
        s_theme_popup,
        "Tap a preview to apply it across the interface.",
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

    for (size_t index = 0;
         index < sizeof(choices) / sizeof(choices[0]);
         ++index) {
        ui_theme_preview_create(
            s_theme_popup,
            choices[index].id,
            choices[index].id == selected,
            24 + (int32_t)index * 290,
            84,
            276,
            250,
            theme_select_cb,
            (void *)(uintptr_t)choices[index].id);
    }

    ui_popup_add_standard_footer_divider(s_theme_popup);

    ui_popup_add_footer_action(
        s_theme_popup,
        UI_POPUP_ACTION_CLOSE,
        LV_SYMBOL_CLOSE " CLOSE",
        160,
        UI_POPUP_FOOTER_CENTER,
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
