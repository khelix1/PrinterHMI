#include "ui_appearance_popups.h"

#include "theme_manager.h"
#include "ui_popup.h"
#include "ui_theme.h"

#include "esp_log.h"

#include <stddef.h>
#include <stdint.h>

static const char *TAG = "ui_appearance";
static lv_obj_t *s_popup = NULL;
static ui_appearance_changed_cb_t s_changed_cb = NULL;
static ui_appearance_changed_cb_t s_pending_cb = NULL;
static bool s_change_pending = false;

static void appearance_close(void)
{
    if (s_popup) lv_obj_delete(s_popup);
    s_popup = NULL;
    s_changed_cb = NULL;
}

static void appearance_close_cb(lv_event_t *event)
{
    (void)event;
    appearance_close();
}

static void appearance_apply_async(void *user_data)
{
    (void)user_data;
    ui_appearance_changed_cb_t callback = s_pending_cb;
    s_pending_cb = NULL;
    s_change_pending = false;
    appearance_close();
    if (callback) callback();
}

static void appearance_schedule_apply(void)
{
    if (s_change_pending) return;
    s_pending_cb = s_changed_cb;
    s_change_pending = true;
    lv_async_call(appearance_apply_async, NULL);
}

static lv_obj_t *appearance_popup(const char *title,
                                  const char *message,
                                  int32_t width,
                                  int32_t height)
{
    s_popup = ui_popup_create(
        lv_layer_top(), width, height, UI_POPUP_STANDARD);
    if (!s_popup) return NULL;

    ui_popup_add_title(s_popup, title, false, 8);
    ui_popup_add_header_divider(s_popup, 44);
    ui_popup_add_status_label(s_popup, message, 24, 50, width - 48);
    ui_popup_add_standard_footer_divider(s_popup);
    ui_popup_add_footer_action(
        s_popup, UI_POPUP_ACTION_CLOSE, LV_SYMBOL_CLOSE " CLOSE",
        160, UI_POPUP_FOOTER_CENTER,
        appearance_close_cb, NULL, NULL);
    return s_popup;
}

static void accent_select_cb(lv_event_t *event)
{
    if (!event || lv_event_get_code(event) != LV_EVENT_CLICKED) return;
    ui_accent_id_t accent =
        (ui_accent_id_t)(uintptr_t)lv_event_get_user_data(event);
    if (!theme_manager_select_accent(accent)) {
        ESP_LOGE(TAG, "Could not select accent %d", (int)accent);
        return;
    }
    appearance_schedule_apply();
}

void ui_appearance_popups_show_accent(ui_appearance_changed_cb_t changed_cb)
{
    s_changed_cb = changed_cb;
    if (s_popup) { lv_obj_move_foreground(s_popup); return; }
    const int32_t row_height = ui_theme_density_metric(38, 44, 50);
    const int32_t row_step = ui_theme_density_metric(45, 51, 57);
    const int32_t list_height = 16 + (UI_ACCENT_COUNT - 1) * row_step + row_height;
    const int32_t popup_height = list_height + 170;
    if (!appearance_popup(
            "ACCENT COLOR",
            "Choose an accent. Status, warning, and danger colors remain semantic.",
            720, popup_height)) return;

    lv_obj_t *list = ui_popup_add_list(
        s_popup, 24, 84, 672, list_height);
    if (!list) { appearance_close(); return; }

    ui_accent_id_t active = theme_manager_accent();
    for (int accent = 0; accent < UI_ACCENT_COUNT; ++accent) {
        lv_obj_t *row = ui_popup_add_selectable_row(
            list,
            theme_manager_accent_name((ui_accent_id_t)accent),
            8, 8 + accent * row_step, 640, row_height,
            accent_select_cb, (void *)(uintptr_t)accent);
        ui_popup_set_selectable_row_selected(
            row, (ui_accent_id_t)accent == active);
    }
}

static void density_select_cb(lv_event_t *event)
{
    if (!event || lv_event_get_code(event) != LV_EVENT_CLICKED) return;
    ui_density_id_t density =
        (ui_density_id_t)(uintptr_t)lv_event_get_user_data(event);
    if (!theme_manager_select_density(density)) {
        ESP_LOGE(TAG, "Could not select density %d", (int)density);
        return;
    }
    appearance_schedule_apply();
}

void ui_appearance_popups_show_density(ui_appearance_changed_cb_t changed_cb)
{
    s_changed_cb = changed_cb;
    if (s_popup) { lv_obj_move_foreground(s_popup); return; }
    const int32_t row_height = ui_theme_density_metric(40, 48, 56);
    const int32_t row_step = ui_theme_density_metric(47, 56, 64);
    const int32_t list_height = 16 + 2 * row_step + row_height;
    const int32_t popup_height = list_height + 170;
    if (!appearance_popup(
            "DISPLAY DENSITY",
            "Choose typography and spacing independently of the theme.",
            640, popup_height)) return;

    lv_obj_t *list = ui_popup_add_list(
        s_popup, 24, 84, 592, list_height);
    if (!list) { appearance_close(); return; }

    ui_density_id_t active = theme_manager_density();
    for (int density = 0; density < UI_DENSITY_COUNT; ++density) {
        lv_obj_t *row = ui_popup_add_selectable_row(
            list,
            theme_manager_density_name((ui_density_id_t)density),
            8, 8 + density * row_step, 560, row_height,
            density_select_cb, (void *)(uintptr_t)density);
        ui_popup_set_selectable_row_selected(
            row, (ui_density_id_t)density == active);
    }
}

static const char *toggle_text(const char *label, bool enabled)
{
    static char buffers[4][64];
    static unsigned next = 0;
    char *buffer = buffers[next++ % 4];
    lv_snprintf(buffer, 64, "%s   |   %s", label, enabled ? "ON" : "OFF");
    return buffer;
}

static void accessibility_toggle_cb(lv_event_t *event)
{
    if (!event || lv_event_get_code(event) != LV_EVENT_CLICKED) return;
    int option = (int)(uintptr_t)lv_event_get_user_data(event);
    ui_accessibility_t value = theme_manager_accessibility();

    if (option == 0) value.large_text = !value.large_text;
    else if (option == 1) value.high_contrast = !value.high_contrast;
    else if (option == 2) value.reduced_transparency = !value.reduced_transparency;
    else if (option == 3) value.reduced_motion = !value.reduced_motion;
    else return;

    if (!theme_manager_set_accessibility(value)) {
        ESP_LOGE(TAG, "Could not save accessibility option %d", option);
        return;
    }
    appearance_schedule_apply();
}

void ui_appearance_popups_show_accessibility(
    ui_appearance_changed_cb_t changed_cb)
{
    s_changed_cb = changed_cb;
    if (s_popup) { lv_obj_move_foreground(s_popup); return; }
    const int32_t row_height = ui_theme_density_metric(40, 48, 56);
    const int32_t row_step = ui_theme_density_metric(47, 56, 64);
    const int32_t list_height = 16 + 3 * row_step + row_height;
    const int32_t popup_height = list_height + 170;
    if (!appearance_popup(
            "ACCESSIBILITY",
            "Tap an option to toggle it. The interface will refresh immediately.",
            720, popup_height)) return;

    lv_obj_t *list = ui_popup_add_list(
        s_popup, 24, 84, 672, list_height);
    if (!list) { appearance_close(); return; }

    ui_accessibility_t value = theme_manager_accessibility();
    const char *labels[] = {
        toggle_text("Large text", value.large_text),
        toggle_text("High contrast", value.high_contrast),
        toggle_text("Reduced transparency", value.reduced_transparency),
        toggle_text("Reduced motion", value.reduced_motion),
    };
    const bool enabled[] = {
        value.large_text, value.high_contrast,
        value.reduced_transparency, value.reduced_motion,
    };

    for (int option = 0; option < 4; ++option) {
        lv_obj_t *row = ui_popup_add_selectable_row(
            list, labels[option], 8, 8 + option * row_step,
            640, row_height,
            accessibility_toggle_cb, (void *)(uintptr_t)option);
        ui_popup_set_selectable_row_selected(row, enabled[option]);
    }
}

void ui_appearance_popups_close_all(void)
{
    appearance_close();
}
