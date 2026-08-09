#include "ota_ui_controller.h"

#include <stdio.h>

#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_ota_ops.h"

#include "ota_manager.h"
#include "ui_ota_popup.h"
#include "ui_ota_release_browser.h"
#include "ui_toast.h"

static const char *TAG = "PrinterHMI";

static bool ota_ui_controller_start_url(const char *url, bool save_custom_url)
{
    if (!url || !url[0]) return false;
    if (ota_manager_is_running()) {
        ESP_LOGW(TAG, "OTA: update already running");
        return false;
    }
    if (!ota_manager_start(url)) {
        ESP_LOGE(TAG, "OTA: unable to start update");
        ui_toast_show(UI_STATUS_DANGER, "OTA NOT STARTED",
                      "Another network operation is active. Try again.");
        return false;
    }
    if (save_custom_url) ota_manager_set_url(url);
    return true;
}

static void ota_popup_remote_bridge(void);

static void ota_popup_start_bridge(const char *url)
{
    (void)ota_ui_controller_start_url(url, true);
}

static void ota_catalog_start_bridge(const char *url)
{
    (void)ota_ui_controller_start_url(url, false);
}

static void ota_open_custom_url_bridge(void)
{
    const esp_app_desc_t *app = esp_app_get_description();
    const esp_partition_t *running = esp_ota_get_running_partition();
    char ota_info[320];
    snprintf(ota_info, sizeof(ota_info),
             "HTTPS verifies the server. Use HTTP only for a trusted local development server.  |  Current: %s  |  Built: %s %s  |  Slot: %s",
             app ? app->version : "unknown", app ? app->date : __DATE__,
             app ? app->time : __TIME__, running ? running->label : "unknown");
    ui_ota_popup_show(ota_manager_get_url(), ota_info,
                      ota_manager_url_capacity() - 1,
                      ota_popup_start_bridge, ota_popup_remote_bridge);
}

static void ota_popup_remote_bridge(void)
{
    ui_ota_popup_close();
    ui_ota_release_browser_show(ota_catalog_start_bridge,
                                ota_open_custom_url_bridge);
}

void ota_ui_controller_open_event_cb(lv_event_t *event)
{
    (void)event;
    ui_ota_release_browser_show(ota_catalog_start_bridge,
                                ota_open_custom_url_bridge);
}
