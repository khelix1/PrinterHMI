#include "moonraker_probe.h"
#include "network_activity_controller.h"

#include "esp_err.h"
#include "esp_http_client.h"

#include <stdio.h>
#include <string.h>


typedef struct {
    char *buf;
    size_t len;
    size_t cap;
} moonraker_probe_capture_t;


static esp_err_t probe_http_event_handler(
    esp_http_client_event_t *event)
{
    if (!event || !event->user_data) {
        return ESP_OK;
    }

    moonraker_probe_capture_t *capture =
        (moonraker_probe_capture_t *)event->user_data;

    if (event->event_id == HTTP_EVENT_ON_DATA &&
        event->data &&
        event->data_len > 0 &&
        capture->buf &&
        capture->cap > 0) {

        size_t available =
            capture->cap - capture->len - 1;

        size_t copy_len =
            (size_t)event->data_len < available
                ? (size_t)event->data_len
                : available;

        if (copy_len > 0) {
            memcpy(
                capture->buf + capture->len,
                event->data,
                copy_len);

            capture->len += copy_len;
            capture->buf[capture->len] = '\0';
        }
    }

    return ESP_OK;
}


bool moonraker_probe_host(const char *host, int port)
{
    if (!host || !host[0] || port <= 0 || port >= 65536) {
        return false;
    }

    char url[96];

    int written = snprintf(
        url,
        sizeof(url),
        "http://%s:%d/server/info",
        host,
        port);

    if (written < 0 || written >= (int)sizeof(url)) {
        return false;
    }

    char body[256] = {0};

    moonraker_probe_capture_t capture = {
        .buf = body,
        .len = 0,
        .cap = sizeof(body),
    };

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 350,
        .event_handler = probe_http_event_handler,
        .user_data = &capture,
    };

    esp_http_client_handle_t client =
        esp_http_client_init(&config);

    if (!client) {
        return false;
    }

    if (!network_activity_controller_try_begin_shared()) {
        esp_http_client_cleanup(client);
        return false;
    }

    esp_err_t err = esp_http_client_perform(client);
    network_activity_controller_end_shared();

    int code =
        esp_http_client_get_status_code(client);

    esp_http_client_cleanup(client);

    if (err != ESP_OK || code != 200) {
        return false;
    }

    return strstr(body, "klippy") != NULL ||
           strstr(body, "moonraker") != NULL ||
           strstr(body, "result") != NULL;
}
