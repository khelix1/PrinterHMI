#include "ota_manager.h"

#include "ui_ota_popup.h"
#include "operator_event_log.h"
#include "network_activity_controller.h"

#include "esp_err.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "nvs.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static const char *TAG = "ota_manager";

static volatile int s_progress_pct = 0;
static char s_progress_text[160] = "";

static volatile int s_bytes_read = 0;
static volatile int s_content_length = 0;

static volatile bool s_task_running = false;
static volatile bool s_cancel_requested = false;
static volatile bool s_cancel_allowed = false;
static volatile bool s_cancel_complete = false;
static char s_task_url[192] = "";


#ifndef OTA_TEST_URL
#define OTA_TEST_URL ""
#endif

#define OTA_NVS_NAMESPACE "ota"
#define OTA_NVS_KEY_URL   "url"

static char s_ota_url[192] = "";
static bool s_ota_url_loaded = false;

static void ota_manager_load_url(void)
{
    if (s_ota_url_loaded) {
        return;
    }

    s_ota_url_loaded = true;

    nvs_handle_t handle;
    size_t length = sizeof(s_ota_url);

    esp_err_t err = nvs_open(
        OTA_NVS_NAMESPACE,
        NVS_READONLY,
        &handle
    );

    if (err == ESP_OK) {
        err = nvs_get_str(
            handle,
            OTA_NVS_KEY_URL,
            s_ota_url,
            &length
        );

        nvs_close(handle);

        if (err == ESP_OK && s_ota_url[0] != '\0') {
            return;
        }
    }

    snprintf(
        s_ota_url,
        sizeof(s_ota_url),
        "%s",
        OTA_TEST_URL
    );
}

const char *ota_manager_get_url(void)
{
    ota_manager_load_url();
    return s_ota_url;
}

size_t ota_manager_url_capacity(void)
{
    return sizeof(s_ota_url);
}

void ota_manager_set_url(const char *url)
{
    if (!url || !url[0]) {
        return;
    }

    snprintf(
        s_ota_url,
        sizeof(s_ota_url),
        "%s",
        url
    );

    s_ota_url_loaded = true;

    nvs_handle_t handle;

    esp_err_t err = nvs_open(
        OTA_NVS_NAMESPACE,
        NVS_READWRITE,
        &handle
    );

    if (err != ESP_OK) {
        ESP_LOGW(
            TAG,
            "OTA: failed to open NVS for URL save: %s",
            esp_err_to_name(err)
        );
        return;
    }

    err = nvs_set_str(
        handle,
        OTA_NVS_KEY_URL,
        s_ota_url
    );

    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }

    nvs_close(handle);

    if (err == ESP_OK) {
        ESP_LOGI(
            TAG,
            "OTA: saved URL to NVS: %s",
            s_ota_url
        );
    } else {
        ESP_LOGW(
            TAG,
            "OTA: failed to save URL: %s",
            esp_err_to_name(err)
        );
    }
}

static void set_state(const char *text, int percent)
{
    if (text) {
        snprintf(s_progress_text,
                 sizeof(s_progress_text),
                 "%s",
                 text);
    }

    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;

    s_progress_pct = percent;
}

static esp_err_t http_event_handler(
    esp_http_client_event_t *event)
{
    if (!event) return ESP_OK;

    if (event->event_id == HTTP_EVENT_ON_HEADER &&
        event->header_key &&
        event->header_value &&
        strcasecmp(event->header_key,
                   "Content-Length") == 0) {

        s_content_length = atoi(event->header_value);

        ESP_LOGI(TAG,
                 "content length=%d",
                 s_content_length);
    }

    if (event->event_id == HTTP_EVENT_ON_DATA &&
        event->data_len > 0) {

        s_bytes_read += event->data_len;

        if (s_content_length > 0) {
            int percent =
                (s_bytes_read * 100) / s_content_length;

            if (percent > 99) percent = 99;

            char message[160];

            snprintf(message,
                     sizeof(message),
                     "Downloading firmware... %d%%\n"
                     "%d / %d bytes",
                     percent,
                     s_bytes_read,
                     s_content_length);

            set_state(message, percent);
        } else {
            char message[160];

            snprintf(message,
                     sizeof(message),
                     "Downloading firmware...\n%d bytes",
                     s_bytes_read);

            set_state(message, 35);
        }
    }

    return ESP_OK;
}

static void update_task(void *arg)
{
    (void)arg;

    ESP_LOGI(TAG,
             "task starting from %s",
             s_task_url);

    s_bytes_read = 0;
    s_content_length = 0;

    esp_https_ota_handle_t ota_handle = NULL;
    esp_err_t result = ESP_FAIL;

    set_state("Preparing Network...", 5);

    /* Block new shared HTTP work and wait for existing requests plus the
     * persistent Moonraker WebSocket to drain before starting TLS.
     */
    int elapsed_ms = 0;
    for (; elapsed_ms < 20000; elapsed_ms += 50) {
        if (s_cancel_requested) {
            goto cancelled;
        }

        if (network_activity_controller_exclusive_ready()) {
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }

    if (!network_activity_controller_exclusive_ready()) {
        result = ESP_ERR_TIMEOUT;
        goto failed;
    }

    /* Allow the final SDIO/TCP teardown traffic to settle before OTA. */
    vTaskDelay(pdMS_TO_TICKS(1500));

    set_state("Checking Server...", 10);

    esp_http_client_config_t http_config = {
        .url = s_task_url,
        .timeout_ms = 15000,
        .keep_alive_enable = true,
        .event_handler = http_event_handler,
        .crt_bundle_attach = esp_crt_bundle_attach,

        /*
         * GitHub redirects browser_download_url to a signed asset URL whose
         * request target is longer than ESP HTTP client's 512-byte default.
         * This is the request/header buffer, not a firmware download buffer.
         */
        .buffer_size_tx = 2048,
    };

    esp_https_ota_config_t ota_config = {
        .http_config = &http_config,
    };

    result = esp_https_ota_begin(&ota_config, &ota_handle);

    if (result != ESP_OK) {
        if (s_cancel_requested) {
            goto cancelled;
        }
        goto failed;
    }

    while (true) {
        if (s_cancel_requested) {
            goto cancelled;
        }

        result = esp_https_ota_perform(ota_handle);

        if (result == ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            continue;
        }

        break;
    }

    if (s_cancel_requested) {
        goto cancelled;
    }

    if (result != ESP_OK) {
        goto failed;
    }

    if (!esp_https_ota_is_complete_data_received(ota_handle)) {
        result = ESP_FAIL;
        goto failed;
    }

    /*
     * Cancellation ends here. Once finish begins it may validate the image
     * and switch the boot partition, so the UI must no longer offer CANCEL.
     */
    s_cancel_allowed = false;
    set_state("Validating Firmware...", 99);

    result = esp_https_ota_finish(ota_handle);
    ota_handle = NULL;

    if (result == ESP_OK) {
        set_state("Update Complete\nRebooting...", 100);

        ESP_LOGI(TAG, "success, rebooting");
        operator_event_log_add(
            OPERATOR_EVENT_INFO,
            "Firmware update completed; rebooting");

        vTaskDelay(pdMS_TO_TICKS(500));
        esp_restart();
    }

failed:
    s_cancel_allowed = false;

    if (ota_handle) {
        esp_err_t abort_result =
            esp_https_ota_abort(ota_handle);

        if (abort_result != ESP_OK) {
            ESP_LOGW(TAG,
                     "OTA abort after failure returned: %s",
                     esp_err_to_name(abort_result));
        }

        ota_handle = NULL;
    }

    set_state(
        "OTA failed. Check server URL and WiFi.",
        0);

    ESP_LOGE(TAG,
             "failed: %s",
             esp_err_to_name(result));

    operator_event_log_add(
        OPERATOR_EVENT_ERROR,
        "Firmware update failed: %s",
        esp_err_to_name(result));

    network_activity_controller_release_exclusive();
    s_task_running = false;
    vTaskDelete(NULL);
    return;

cancelled:
    s_cancel_allowed = false;

    if (ota_handle) {
        esp_err_t abort_result =
            esp_https_ota_abort(ota_handle);

        if (abort_result != ESP_OK) {
            ESP_LOGW(TAG,
                     "OTA cancel abort returned: %s",
                     esp_err_to_name(abort_result));
        }

        ota_handle = NULL;
    }

    set_state("Update Cancelled", 0);

    ESP_LOGI(TAG, "OTA cancelled safely");
    operator_event_log_add(
        OPERATOR_EVENT_WARNING,
        "Firmware update cancelled safely");

    network_activity_controller_release_exclusive();
    s_task_running = false;
    s_cancel_complete = true;
    vTaskDelete(NULL);
}

bool ota_manager_start(const char *url)
{
    if (!url || !url[0] || s_task_running) {
        return false;
    }

    if (!network_activity_controller_request_exclusive()) {
        ESP_LOGW(TAG, "another exclusive network operation is active");
        set_state("Network is busy. Try again.", 0);
        return false;
    }

    snprintf(s_task_url,
             sizeof(s_task_url),
             "%s",
             url);

    s_cancel_requested = false;
    s_cancel_complete = false;
    s_cancel_allowed = true;

    set_state("Starting OTA...", 5);
    ui_ota_progress_show(ota_manager_cancel);

    ESP_LOGI(TAG,
             "starting task for %s",
             s_task_url);

    operator_event_log_add(
        OPERATOR_EVENT_INFO,
        "Firmware update started");

    s_task_running = true;

    BaseType_t result = xTaskCreate(
        update_task,
        "ota_update",
        12288,
        NULL,
        5,
        NULL);

    if (result != pdPASS) {
        network_activity_controller_release_exclusive();
        s_task_running = false;
        s_cancel_allowed = false;
        set_state("Unable to start OTA task.", 0);
        ESP_LOGE(TAG, "failed to create OTA task");
        operator_event_log_add(
            OPERATOR_EVENT_ERROR,
            "Unable to start firmware-update task");
        return false;
    }

    return true;
}

bool ota_manager_is_running(void)
{
    return s_task_running;
}

void ota_manager_cancel(void)
{
    if (!s_task_running || !s_cancel_allowed) {
        return;
    }

    s_cancel_requested = true;
    set_state("Cancelling OTA...", s_progress_pct);

    ESP_LOGI(TAG, "OTA cancellation requested");
}

void ota_manager_pump_ui(void)
{
    if (s_cancel_complete) {
        s_cancel_complete = false;
        ui_ota_progress_close();
        return;
    }

    ui_ota_progress_pump(
        s_progress_text,
        s_progress_pct,
        s_bytes_read,
        s_content_length,
        s_cancel_allowed);
}
