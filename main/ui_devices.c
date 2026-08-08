#include "ui_devices.h"

#include <stdbool.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "lvgl.h"

#include "ui_button.h"
#include "ui_page_geometry.h"
#include "ui_theme.h"
#include "ui_widgets.h"
#include "ui_devices_catalog_view.h"

typedef struct {
    lv_obj_t *root;
    lv_obj_t *banner_status;
    ui_devices_open_telemetry_cb_t open_telemetry;
} ui_devices_state_t;

static const char TAG[] = "ui_devices";
static ui_devices_state_t *s_devices;


static bool devices_state_init(void)
{
    if (s_devices) {
        return true;
    }

    s_devices = heap_caps_calloc(
        1,
        sizeof(*s_devices),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (s_devices) {
        ESP_LOGI(
            TAG,
            "Devices page state allocated permanently in PSRAM: %u bytes",
            (unsigned)sizeof(*s_devices));
        return true;
    }

    s_devices = heap_caps_calloc(
        1,
        sizeof(*s_devices),
        MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

    if (!s_devices) {
        ESP_LOGE(TAG, "Unable to allocate Devices page state");
        return false;
    }

    ESP_LOGW(TAG, "Devices page state using internal RAM fallback");
    return true;
}


static lv_obj_t *devices_label(
    lv_obj_t *parent,
    const char *text,
    const lv_font_t *font,
    lv_color_t color,
    int x,
    int y,
    int width)
{
    lv_obj_t *label = lv_label_create(parent);

    lv_label_set_text(label, text ? text : "--");
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(label, width);
    lv_obj_set_pos(label, x, y);
    ui_apply_custom_label_style(label, font, color);

    return label;
}


static void devices_open_telemetry_event_cb(
    lv_event_t *event)
{
    (void)event;

    if (s_devices && s_devices->open_telemetry) {
        s_devices->open_telemetry();
    }
}


void ui_devices_v32_show(
    ui_devices_open_telemetry_cb_t open_telemetry_cb)
{
    if (!devices_state_init()) {
        return;
    }

    s_devices->open_telemetry =
        open_telemetry_cb;

    if (s_devices->root) {
        lv_obj_move_foreground(s_devices->root);
        ui_devices_catalog_view_refresh();
        return;
    }

    s_devices->root = lv_obj_create(
        lv_screen_active());

    lv_obj_set_size(
        s_devices->root,
        UI_PAGE_ROOT_WIDTH,
        UI_PAGE_ROOT_HEIGHT);
    lv_obj_set_pos(
        s_devices->root,
        UI_PAGE_ROOT_X,
        UI_PAGE_ROOT_Y);
    lv_obj_clear_flag(
        s_devices->root,
        LV_OBJ_FLAG_SCROLLABLE);
    ui_apply_surface_role(
        s_devices->root,
        UI_SURFACE_PAGE_DEEP);

    lv_obj_t *banner = ui_create_operator_banner(
        s_devices->root,
        20,
        20,
        800,
        86,
        UI_STATUS_INFO);

    devices_label(
        banner,
        "DEVICES",
        UI_FONT_TITLE,
        UI_TEXT_BRIGHT,
        20,
        15,
        300);

    devices_label(
        banner,
        "Active-printer objects, grouped by capability",
        UI_FONT_CAPTION,
        UI_TEXT_DIM,
        20,
        50,
        480);

    s_devices->banner_status = devices_label(
        banner,
        "WAITING FOR PRINTER",
        UI_FONT_CAPTION,
        UI_ACCENT_BRIGHT,
        560,
        22,
        220);

    lv_obj_set_style_text_align(
        s_devices->banner_status,
        LV_TEXT_ALIGN_RIGHT,
        0);

    lv_obj_t *telemetry = ui_button_create(
        banner,
        UI_BUTTON_OUTLINED,
        "TELEMETRY");

    if (telemetry) {
        lv_obj_set_size(telemetry, 132, 34);
        lv_obj_align(
            telemetry,
            LV_ALIGN_BOTTOM_RIGHT,
            -18,
            -8);
        lv_obj_add_event_cb(
            telemetry,
            devices_open_telemetry_event_cb,
            LV_EVENT_CLICKED,
            NULL);
    }

    ui_devices_catalog_view_create(
        s_devices->root,
        s_devices->banner_status);

}


void ui_devices_v32_hide(void)
{
    if (!s_devices) {
        return;
    }

    ui_devices_catalog_view_close();

    if (s_devices->root) {
        lv_obj_delete(s_devices->root);
    }

    s_devices->root = NULL;
    s_devices->banner_status = NULL;
}
