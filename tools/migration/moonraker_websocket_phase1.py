#!/usr/bin/env python3
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
MAIN = ROOT / "main" / "main.c"
CMAKE = ROOT / "main" / "CMakeLists.txt"
MANIFEST = ROOT / "main" / "idf_component.yml"
WS_HEADER = ROOT / "main" / "moonraker_live_websocket.h"
WS_SOURCE = ROOT / "main" / "moonraker_live_websocket.c"


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"expected one {label}, found {count}")
    return text.replace(old, new, 1)


for required in (MAIN, CMAKE, MANIFEST):
    if not required.exists():
        raise RuntimeError(f"missing required file: {required}")

if WS_HEADER.exists() or WS_SOURCE.exists():
    raise RuntimeError(
        "moonraker_live_websocket module already exists; audit before rerunning"
    )

manifest = MANIFEST.read_text()
if "espressif/esp_websocket_client" in manifest:
    raise RuntimeError("WebSocket dependency already exists in manifest")

manifest = replace_once(
    manifest,
    "dependencies:\n",
    'dependencies:\n  espressif/esp_websocket_client: "^1.7.0"\n',
    "manifest dependencies anchor",
)

cmake = CMAKE.read_text()
cmake = replace_once(
    cmake,
    '        "moonraker_live_transport.c"\n',
    '        "moonraker_live_transport.c"\n'
    '        "moonraker_live_websocket.c"\n',
    "Moonraker transport source anchor",
)
cmake = replace_once(
    cmake,
    "        esp_http_client\n",
    "        esp_http_client\n"
    "        espressif__esp_websocket_client\n",
    "HTTP component requirement anchor",
)

main = MAIN.read_text()
include_anchor = '#include "moonraker_poll.h"\n'
if include_anchor not in main:
    include_anchor = '#include "moonraker_live_transport.h"\n'

main = replace_once(
    main,
    include_anchor,
    include_anchor + '#include "moonraker_live_websocket.h"\n',
    "Moonraker include anchor",
)

poll_anchor = "        moonraker_live_poll_tasklet();\n"
main = replace_once(
    main,
    poll_anchor,
    '''        /* MOONRAKER_WEBSOCKET_PHASE1
         * Observe the active profile over a persistent subscription while
         * the proven HTTP poller remains authoritative.
         */
        moonraker_live_websocket_tasklet(
            s_got_ip,
            moonraker_config_host(),
            moonraker_config_port(),
            MOONRAKER_API_KEY,
            moonraker_config_generation());

        moonraker_live_poll_tasklet();
''',
    "runtime Moonraker poll call",
)

header = r'''#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Active-profile Moonraker WebSocket lifecycle.
 *
 * Phase 1 observes and logs a real subscription while HTTP polling remains
 * authoritative. Call only from the existing application runtime task.
 */
void moonraker_live_websocket_tasklet(
    bool wifi_ready,
    const char *host,
    int port,
    const char *api_key,
    uint32_t configuration_generation);

bool moonraker_live_websocket_connected(void);
bool moonraker_live_websocket_subscribed(void);
void moonraker_live_websocket_stop(void);

#ifdef __cplusplus
}
#endif
'''

source = r'''#include "moonraker_live_websocket.h"

#include <stdio.h>
#include <string.h>

#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_websocket_client.h"
#include "freertos/FreeRTOS.h"


#define TAG "moon_live_ws"
#define WS_RETRY_INTERVAL_US 3000000LL


static esp_websocket_client_handle_t s_client = NULL;
static bool s_started = false;
static volatile bool s_connected = false;
static volatile bool s_subscribe_pending = false;
static volatile bool s_subscribed = false;
static volatile uint32_t s_receive_count = 0;

static uint32_t s_generation = 0;
static int64_t s_retry_after_us = 0;
static char s_host[128] = "";
static char s_api_key[160] = "";
static char s_uri[256] = "";


static const char s_subscription[] =
    "{\"jsonrpc\":\"2.0\","
    "\"method\":\"printer.objects.subscribe\","
    "\"params\":{\"objects\":{"
    "\"temperature_sensor drybox_center\":null,"
    "\"sht3x drybox_env\":null,"
    "\"heater_generic drybox_heater\":null,"
    "\"fan_generic drybox_fan\":null,"
    "\"gcode_macro DRYBOX_VARS\":null,"
    "\"print_stats\":null,"
    "\"motion_report\":null,"
    "\"display_status\":null,"
    "\"gcode_move\":null,"
    "\"fan\":null,"
    "\"extruder\":null,"
    "\"heater_bed\":null,"
    "\"toolhead\":null}},"
    "\"id\":1001}";


static void copy_text(
    char *destination,
    size_t destination_size,
    const char *source)
{
    if (!destination || destination_size == 0) return;

    snprintf(
        destination,
        destination_size,
        "%s",
        source ? source : "");
}


static void websocket_event_handler(
    void *handler_argument,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data)
{
    (void)handler_argument;
    (void)event_base;

    switch ((esp_websocket_event_id_t)event_id) {
    case WEBSOCKET_EVENT_CONNECTED:
        s_connected = true;
        s_subscribed = false;
        s_subscribe_pending = true;
        s_receive_count = 0;
        ESP_LOGI(TAG, "WS_CONNECTED %s", s_uri);
        break;

    case WEBSOCKET_EVENT_DISCONNECTED:
        s_connected = false;
        s_subscribed = false;
        s_subscribe_pending = false;
        ESP_LOGW(TAG, "WS_DISCONNECTED %s", s_uri);
        break;

    case WEBSOCKET_EVENT_DATA: {
        esp_websocket_event_data_t *data =
            (esp_websocket_event_data_t *)event_data;

        uint32_t count = ++s_receive_count;
        if (count <= 5 || count % 100 == 0) {
            ESP_LOGI(
                TAG,
                "WS_RX count=%u bytes=%d",
                (unsigned)count,
                data ? data->data_len : 0);
        }
        break;
    }

    case WEBSOCKET_EVENT_ERROR:
        ESP_LOGW(TAG, "WS_ERROR %s", s_uri);
        break;

    case WEBSOCKET_EVENT_CLOSED:
        s_connected = false;
        s_subscribed = false;
        s_subscribe_pending = false;
        ESP_LOGW(TAG, "WS_CLOSED %s", s_uri);
        break;

    case WEBSOCKET_EVENT_BEFORE_CONNECT:
    default:
        break;
    }
}


static void destroy_client(void)
{
    esp_websocket_client_handle_t client = s_client;
    s_client = NULL;

    s_connected = false;
    s_subscribed = false;
    s_subscribe_pending = false;

    if (!client) return;

    if (s_started) {
        esp_websocket_client_stop(client);
    }

    esp_websocket_client_destroy(client);
    s_started = false;
}


void moonraker_live_websocket_stop(void)
{
    destroy_client();
    s_host[0] = '\0';
    s_api_key[0] = '\0';
    s_uri[0] = '\0';
    s_generation = 0;
    s_retry_after_us = 0;
}


static bool send_identify(void)
{
    if (!s_client || !s_connected) return false;

    char request[512];
    int length = snprintf(
        request,
        sizeof(request),
        "{\"jsonrpc\":\"2.0\","
        "\"method\":\"server.connection.identify\","
        "\"params\":{"
        "\"client_name\":\"PrinterHMI\","
        "\"version\":\"3.2\","
        "\"type\":\"display\","
        "\"url\":\"https://github.com/\","
        "\"api_key\":\"%s\"},"
        "\"id\":1000}",
        s_api_key);

    if (length <= 0 || (size_t)length >= sizeof(request)) {
        ESP_LOGE(TAG, "WS identify request overflow");
        return false;
    }

    int sent = esp_websocket_client_send_text(
        s_client,
        request,
        length,
        pdMS_TO_TICKS(1000));

    return sent == length;
}


static bool send_subscription(void)
{
    if (!s_client || !s_connected) return false;

    int length = (int)strlen(s_subscription);
    int sent = esp_websocket_client_send_text(
        s_client,
        s_subscription,
        length,
        pdMS_TO_TICKS(1000));

    if (sent != length) {
        ESP_LOGW(TAG, "WS subscription send failed: %d/%d", sent, length);
        return false;
    }

    ESP_LOGI(TAG, "WS_SUBSCRIBE_SENT objects=13 generation=%u",
             (unsigned)s_generation);
    return true;
}


static bool create_client(
    const char *host,
    int port,
    const char *api_key,
    uint32_t generation)
{
    int written = snprintf(
        s_uri,
        sizeof(s_uri),
        "ws://%s:%d/websocket",
        host,
        port);

    if (written <= 0 || (size_t)written >= sizeof(s_uri)) {
        ESP_LOGE(TAG, "WS URI overflow");
        return false;
    }

    copy_text(s_host, sizeof(s_host), host);
    copy_text(s_api_key, sizeof(s_api_key), api_key);
    s_generation = generation;

    esp_websocket_client_config_t config = {
        .uri = s_uri,
        .task_stack = 4096,
        .buffer_size = 4096,
    };

    s_client = esp_websocket_client_init(&config);
    if (!s_client) {
        ESP_LOGE(TAG, "WS client init failed");
        return false;
    }

    esp_err_t error = esp_websocket_register_events(
        s_client,
        WEBSOCKET_EVENT_ANY,
        websocket_event_handler,
        NULL);

    if (error != ESP_OK) {
        ESP_LOGE(TAG, "WS event registration failed: %s",
                 esp_err_to_name(error));
        destroy_client();
        return false;
    }

    error = esp_websocket_client_start(s_client);
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "WS start failed: %s", esp_err_to_name(error));
        destroy_client();
        return false;
    }

    s_started = true;
    ESP_LOGI(TAG, "WS_START generation=%u uri=%s",
             (unsigned)s_generation, s_uri);
    return true;
}


void moonraker_live_websocket_tasklet(
    bool wifi_ready,
    const char *host,
    int port,
    const char *api_key,
    uint32_t configuration_generation)
{
    bool endpoint_valid =
        host && host[0] && port > 0 && port <= 65535;

    if (!wifi_ready || !endpoint_valid) {
        if (s_client) {
            ESP_LOGI(TAG, "WS_STOP network or endpoint unavailable");
            destroy_client();
        }
        return;
    }

    bool endpoint_changed =
        s_client &&
        (s_generation != configuration_generation ||
         strcmp(s_host, host) != 0);

    if (endpoint_changed) {
        ESP_LOGI(TAG, "WS_REBIND generation=%u->%u host=%s->%s",
                 (unsigned)s_generation,
                 (unsigned)configuration_generation,
                 s_host,
                 host);
        destroy_client();
        s_retry_after_us = 0;
    }

    int64_t now = esp_timer_get_time();

    if (!s_client) {
        if (now < s_retry_after_us) return;

        if (!create_client(
                host,
                port,
                api_key,
                configuration_generation)) {
            s_retry_after_us = now + WS_RETRY_INTERVAL_US;
            return;
        }
    }

    if (s_connected && s_subscribe_pending) {
        s_subscribe_pending = false;

        bool identified = send_identify();
        bool subscribed = identified && send_subscription();
        s_subscribed = subscribed;

        if (!subscribed) {
            ESP_LOGW(TAG, "WS subscribe transaction failed");
        }
    }
}


bool moonraker_live_websocket_connected(void)
{
    return s_connected;
}


bool moonraker_live_websocket_subscribed(void)
{
    return s_connected && s_subscribed;
}
'''

MANIFEST.write_text(manifest)
CMAKE.write_text(cmake)
MAIN.write_text(main)
WS_HEADER.write_text(header)
WS_SOURCE.write_text(source)

print("PASS: Moonraker WebSocket Phase 1 installed")
print("  - managed esp_websocket_client dependency added")
print("  - active-profile generation owns connection lifetime")
print("  - Moonraker identify + 13-object subscription added")
print("  - HTTP polling remains authoritative")
print("Next: idf.py reconfigure && idf.py build")

