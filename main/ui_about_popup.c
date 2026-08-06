#include "ui_about_popup.h"

#include <stdio.h>

#include "esp_app_desc.h"
#include "lvgl.h"
#include "ui_popup.h"


static lv_obj_t *s_popup = NULL;


void ui_about_popup_close(void)
{
    if (s_popup) {
        lv_obj_delete(s_popup);
        s_popup = NULL;
    }
}


static void close_cb(lv_event_t *event)
{
    (void)event;
    ui_about_popup_close();
}


void ui_about_popup_show(void)
{
    if (s_popup) {
        lv_obj_move_foreground(s_popup);
        return;
    }

    const esp_app_desc_t *app = esp_app_get_description();
    const char *version = app && app->version[0] ? app->version : "--";
    const char *idf_version = app && app->idf_ver[0] ? app->idf_ver : "--";
    char build[48];

    if (app && app->date[0] && app->time[0]) {
        snprintf(build, sizeof(build), "%s %s", app->date, app->time);
    } else {
        snprintf(build, sizeof(build), "--");
    }

    s_popup = ui_popup_create(
        lv_layer_top(),
        720,
        430,
        UI_POPUP_STANDARD);

    if (!s_popup) {
        return;
    }

    ui_popup_add_title(s_popup, "ABOUT PRINTERHMI", false, 0);
    ui_popup_add_header_divider(s_popup, 44);
    ui_popup_add_status_label(
        s_popup,
        "OPEN-SOURCE PRINT-CELL OPERATOR INTERFACE",
        28,
        64,
        664);
    ui_popup_add_body(
        s_popup,
        "PrinterHMI is a local-first touchscreen interface for Klipper and "
        "Moonraker print cells. It combines real-time printer status, "
        "multi-printer management and operator tools in a dedicated "
        "ESP32-P4 appliance.",
        28,
        104,
        664);

    char runtime[160];
    snprintf(
        runtime,
        sizeof(runtime),
        "Firmware  %s\nBuild  %s\nPlatform  %s",
        version,
        build,
        idf_version);
    ui_popup_add_caption(s_popup, runtime, 28, 205, 664);
    ui_popup_add_divider(s_popup, 28, 286, 664);
    ui_popup_add_body(
        s_popup,
        "Designed and developed by khelix.\n"
        "Built with the open-source ESP-IDF, LVGL, ESP-Hosted, Klipper and "
        "Moonraker projects.",
        28,
        302,
        664);
    ui_popup_add_standard_footer_divider(s_popup);
    ui_popup_add_footer_action(
        s_popup,
        UI_POPUP_ACTION_CLOSE,
        LV_SYMBOL_CLOSE " CLOSE",
        220,
        UI_POPUP_FOOTER_CENTER,
        close_cb,
        NULL,
        NULL);

    lv_obj_move_foreground(s_popup);
}
