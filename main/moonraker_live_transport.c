#include "moonraker_live_transport.h"
#include "network_activity_controller.h"

#include <stdio.h>
#include <string.h>

#include "esp_err.h"
#include "esp_http_client.h"
#include "esp_log.h"


static const char *TAG = "moon_live_transport";


typedef struct {
    char *buffer;
    size_t length;
    size_t capacity;
    bool overflowed;
} moonraker_live_capture_t;


static esp_err_t moonraker_live_http_event_handler(
    esp_http_client_event_t *event)
{
    if (!event) {
        return ESP_OK;
    }

    moonraker_live_capture_t *capture =
        (moonraker_live_capture_t *)event->user_data;

    if (!capture || !capture->buffer || capture->capacity == 0) {
        return ESP_OK;
    }

    if (event->event_id != HTTP_EVENT_ON_DATA ||
        !event->data ||
        event->data_len <= 0) {
        return ESP_OK;
    }

    size_t incoming = (size_t)event->data_len;
    size_t available = 0;

    if (capture->length < capture->capacity - 1) {
        available = capture->capacity - 1 - capture->length;
    }

    size_t copy_length = incoming;
    if (copy_length > available) {
        copy_length = available;
        capture->overflowed = true;
    }

    if (copy_length > 0) {
        memcpy(
            capture->buffer + capture->length,
            event->data,
            copy_length);

        capture->length += copy_length;
        capture->buffer[capture->length] = '\0';
    }

    return ESP_OK;
}


bool moonraker_live_transport_fetch(
    const char *host,
    int port,
    const char *api_key,
    char *response_buffer,
    size_t buffer_size,
    int *http_status_out)
{
    if (http_status_out) {
        *http_status_out = 0;
    }

    if (!host ||
        !host[0] ||
        port <= 0 ||
        !response_buffer ||
        buffer_size < 2) {
        return false;
    }

    response_buffer[0] = '\0';

    char url[512];
    int written = snprintf(
        url,
        sizeof(url),
        "http://%s:%d/printer/objects/query?"
        "temperature_sensor%%20drybox_center"
        "&sht3x%%20drybox_env"
        "&heater_generic%%20drybox_heater"
        "&fan_generic%%20drybox_fan"
        "&gcode_macro%%20DRYBOX_VARS"
        "&print_stats&motion_report"
        "&display_status"
        "&virtual_sdcard"
        "&gcode_move"
        "&fan"
        "&extruder"
        "&heater_bed"
        "&toolhead",
        host,
        port);

    if (written < 0 || (size_t)written >= sizeof(url)) {
        ESP_LOGE(TAG, "Live-object URL was truncated");
        return false;
    }

    moonraker_live_capture_t capture = {
        .buffer = response_buffer,
        .length = 0,
        .capacity = buffer_size,
        .overflowed = false,
    };

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 3000,
        .event_handler = moonraker_live_http_event_handler,
        .user_data = &capture,
    };

    esp_http_client_handle_t client =
        esp_http_client_init(&config);

    if (!client) {
        ESP_LOGE(TAG, "Live-object HTTP client init failed");
        return false;
    }

    if (api_key && api_key[0]) {
        esp_http_client_set_header(
            client,
            "X-Api-Key",
            api_key);
    }

    esp_err_t error = ESP_ERR_INVALID_STATE;
    int status_code = 0;

    if (network_activity_controller_try_begin_shared()) {
        error = esp_http_client_perform(client);
        status_code = esp_http_client_get_status_code(client);
        network_activity_controller_end_shared();
    }

    if (http_status_out) {
        *http_status_out = status_code;
    }

    esp_http_client_cleanup(client);

    if (capture.overflowed) {
        ESP_LOGE(
            TAG,
            "Live-object response exceeded %u bytes",
            (unsigned)buffer_size);
        return false;
    }

    if (error != ESP_OK) {
        ESP_LOGW(
            TAG,
            "Live-object request failed: %s",
            esp_err_to_name(error));
        return false;
    }

    if (status_code != 200) {
        ESP_LOGW(
            TAG,
            "Live-object request returned HTTP %d",
            status_code);
        return false;
    }

    if (capture.length == 0) {
        ESP_LOGW(TAG, "Live-object response was empty");
        return false;
    }

    return true;
}
