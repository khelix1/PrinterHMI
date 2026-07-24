#include "printer_file_controller.h"

#include <stdio.h>

#include "esp_err.h"
#include "esp_log.h"

#include "moonraker.h"
#include "thumbnail_manager_v32.h"
#include "thumbnail_session_v32.h"
#include "ui_dashboard_v32.h"
#include "ui_printer_v32.h"

static const char *TAG = "printer_file_ctrl";

static void set_status(
    char *status_text,
    size_t status_text_size,
    const char *text)
{
    if (!status_text || status_text_size == 0) {
        return;
    }

    snprintf(
        status_text,
        status_text_size,
        "%s",
        text ? text : "");
}

void printer_file_controller_select_file(
    const char *path,
    printer_file_controller_popup_cb_t show_detail_popup)
{
    if (!path || !path[0]) {
        return;
    }

    snprintf(
        thumbnail_session_v32_selected_file(),
        thumbnail_session_v32_selected_file_size(),
        "%s",
        path);

    thumbnail_manager_v32_set_force_refresh(true);

    if (ui_dashboard_v32_thumb_canvas_file()) {
        ui_dashboard_v32_thumb_canvas_file()[0] = '\0';
    }

    ui_printer_v32_preview_reset();

    ESP_LOGI(
        TAG,
        "PRINTER_FILE_SELECTED %s",
        thumbnail_session_v32_selected_file());

    thumbnail_session_v32_save_last_selected_file();

    if (show_detail_popup) {
        show_detail_popup();
    }
}

bool printer_file_controller_start_file(
    bool network_ready,
    const char *host,
    int port,
    const char *api_key,
    const char *filename,
    char *status_text,
    size_t status_text_size)
{
    if (!network_ready ||
        !host ||
        !host[0] ||
        !filename ||
        !filename[0]) {

        set_status(
            status_text,
            status_text_size,
            "Start print: no WiFi or file");

        return false;
    }

    int http_code = 0;
    esp_err_t err = ESP_FAIL;

    ESP_LOGI(
        TAG,
        "START_PRINT_API filename=%s",
        filename);

    bool ok = moonraker_start_print_file(
        host,
        port,
        api_key,
        filename,
        &http_code,
        &err);

    if (ok) {
        if (status_text && status_text_size > 0) {
            snprintf(
                status_text,
                status_text_size,
                "Start print: %.80s",
                filename);
        }

        return true;
    }

    ESP_LOGW(
        TAG,
        "START_PRINT_API failed err=%s code=%d",
        esp_err_to_name(err),
        http_code);

    if (status_text && status_text_size > 0) {
        snprintf(
            status_text,
            status_text_size,
            "Start print failed HTTP %d %s",
            http_code,
            esp_err_to_name(err));
    }

    return false;
}

bool printer_file_controller_start_selected_file(
    bool network_ready,
    const char *host,
    int port,
    const char *api_key,
    char *status_text,
    size_t status_text_size,
    printer_file_controller_popup_cb_t close_detail_popup)
{
    const char *selected_file =
        thumbnail_session_v32_selected_file();

    if (!selected_file || !selected_file[0]) {
        if (close_detail_popup) {
            close_detail_popup();
        }

        return false;
    }

    ESP_LOGI(
        TAG,
        "START_PRINT_BUTTON selected_print_file=%s",
        selected_file);

    bool ok = printer_file_controller_start_file(
        network_ready,
        host,
        port,
        api_key,
        selected_file,
        status_text,
        status_text_size);

    if (!ok) {
        ESP_LOGW(TAG, "START_PRINT_API failed");
        return false;
    }

    /*
     * Live preview remains owned by the normal Moonraker refresh path.
     */
    if (close_detail_popup) {
        close_detail_popup();
    }

    return true;
}
