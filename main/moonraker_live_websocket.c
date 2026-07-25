#include "moonraker_live_websocket.h"

#include <stdio.h>
#include <string.h>

#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_app_desc.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_websocket_client.h"
#include "freertos/FreeRTOS.h"
#include "cJSON.h"
#include "moonraker.h"
#include "operator_event_log.h"


#define TAG "moon_live_ws"
#define WS_RETRY_INTERVAL_US 3000000LL
#define WS_MESSAGE_MAX_BYTES (128 * 1024)


static esp_websocket_client_handle_t s_client = NULL;
static bool s_started = false;
static volatile bool s_connected = false;
static volatile bool s_discovery_pending = false;
static volatile bool s_subscribe_pending = false;
static volatile bool s_subscription_ready = false;
static volatile bool s_subscribed = false;
static volatile int64_t s_last_status_update_us = 0;
static bool s_file_change_pending = false;

static char *s_message_buffer = NULL;
static size_t s_message_capacity = 0;
static uint32_t s_message_generation = 0;

static uint32_t s_generation = 0;
static int64_t s_retry_after_us = 0;
static char s_host[128] = "";
static char s_api_key[160] = "";
static char s_uri[256] = "";


static char s_subscription[1536] = "";
static size_t s_subscription_hotend_count = 0;


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


static bool ensure_message_capacity(size_t required)
{
    if (required <= s_message_capacity && s_message_buffer) return true;
    if (required > WS_MESSAGE_MAX_BYTES) return false;

    char *replacement = heap_caps_malloc(
        required,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (!replacement) {
        replacement = heap_caps_malloc(required, MALLOC_CAP_8BIT);
    }

    if (!replacement) return false;

    if (s_message_buffer) heap_caps_free(s_message_buffer);
    s_message_buffer = replacement;
    s_message_capacity = required;
    return true;
}



static bool object_list_contains(cJSON *objects, const char *name)
{
    if (!cJSON_IsArray(objects) || !name || !name[0]) return false;

    cJSON *entry = NULL;
    cJSON_ArrayForEach(entry, objects) {
        if (cJSON_IsString(entry) &&
            entry->valuestring &&
            strcmp(entry->valuestring, name) == 0) {
            return true;
        }
    }

    return false;
}


static bool append_subscription_text(
    size_t *used,
    const char *text)
{
    if (!used || !text || *used >= sizeof(s_subscription)) return false;

    int written = snprintf(
        s_subscription + *used,
        sizeof(s_subscription) - *used,
        "%s",
        text);

    if (written < 0 ||
        (size_t)written >= sizeof(s_subscription) - *used) {
        return false;
    }

    *used += (size_t)written;
    return true;
}


static bool build_subscription(
    const char names[][MOONRAKER_HOTEND_NAME_MAX],
    size_t count)
{
    size_t used = 0;
    s_subscription[0] = '\0';

    if (!append_subscription_text(
            &used,
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
            "\"fan\":null,")) {
        return false;
    }

    for (size_t i = 0; i < count; ++i) {
        int written = snprintf(
            s_subscription + used,
            sizeof(s_subscription) - used,
            "\"%s\":null,",
            names[i]);

        if (written < 0 ||
            (size_t)written >= sizeof(s_subscription) - used) {
            return false;
        }

        used += (size_t)written;
    }

    if (!append_subscription_text(
            &used,
            "\"heater_bed\":null,"
            "\"exclude_object\":[\"objects\",\"excluded_objects\","
            "\"current_object\"],"
            "\"toolhead\":null}},"
            "\"id\":1002}")) {
        return false;
    }

    s_subscription_hotend_count = count;
    return true;
}


static bool handle_object_list_response(
    const char *json,
    size_t length)
{
    cJSON *root = cJSON_ParseWithLength(json, length);
    if (!root) return false;

    cJSON *id = cJSON_GetObjectItemCaseSensitive(root, "id");
    if (!cJSON_IsNumber(id) || id->valueint != 1001) {
        cJSON_Delete(root);
        return false;
    }

    cJSON *result = cJSON_GetObjectItemCaseSensitive(root, "result");
    cJSON *objects = cJSON_IsObject(result)
        ? cJSON_GetObjectItemCaseSensitive(result, "objects")
        : NULL;

    static const char *candidates[MOONRAKER_MAX_HOTENDS] = {
        "extruder",
        "extruder1",
        "extruder2",
        "extruder3",
    };

    char names[MOONRAKER_MAX_HOTENDS][MOONRAKER_HOTEND_NAME_MAX] = {{0}};
    size_t count = 0;

    for (size_t i = 0; i < MOONRAKER_MAX_HOTENDS; ++i) {
        if (!object_list_contains(objects, candidates[i])) continue;

        snprintf(
            names[count],
            sizeof(names[count]),
            "%s",
            candidates[i]);
        ++count;
    }

    if (count == 0) {
        ESP_LOGW(TAG, "WS object discovery found no standard hotends");
        s_subscription_ready = false;
        s_subscribe_pending = false;
        cJSON_Delete(root);
        return true;
    }

    moonraker_state_configure_hotends(names, count);

    if (!build_subscription(names, count)) {
        ESP_LOGE(TAG, "WS dynamic subscription overflow");
        s_subscription_ready = false;
        s_subscribe_pending = false;
        cJSON_Delete(root);
        return true;
    }

    s_subscription_ready = true;
    s_subscribe_pending = true;

    ESP_LOGI(
        TAG,
        "WS_HOTENDS_DISCOVERED count=%u active subscription ready",
        (unsigned)count);

    cJSON_Delete(root);
    return true;
}


static void handle_websocket_data(esp_websocket_event_data_t *data)
{
    if (!data || !data->data_ptr || data->data_len <= 0) return;
    if (data->op_code != 0x1 && data->op_code != 0x0) return;

    int payload_length = data->payload_len > 0
        ? data->payload_len
        : data->data_len;

    if (payload_length <= 0 ||
        data->payload_offset < 0 ||
        data->data_len > payload_length - data->payload_offset) {
        ESP_LOGW(TAG, "WS invalid fragment geometry");
        return;
    }

    size_t required = (size_t)payload_length + 1;
    if (!ensure_message_capacity(required)) {
        ESP_LOGW(TAG, "WS message allocation failed: %u bytes",
                 (unsigned)required);
        return;
    }

    if (data->payload_offset == 0) {
        s_message_buffer[0] = '\0';
        s_message_generation = s_generation;
    }

    memcpy(
        s_message_buffer + data->payload_offset,
        data->data_ptr,
        (size_t)data->data_len);

    int received_end = data->payload_offset + data->data_len;
    if (received_end < payload_length) return;

    s_message_buffer[payload_length] = '\0';

    if (s_message_generation != s_generation) {
        ESP_LOGW(TAG, "WS discarded stale generation message");
        return;
    }

    if (handle_object_list_response(
            s_message_buffer,
            (size_t)payload_length)) {
        return;
    }

    moonraker_websocket_message_t message =
        moonraker_state_merge_websocket_json(
            s_message_buffer,
            (size_t)payload_length);

    switch (message) {
    case MOONRAKER_WEBSOCKET_MESSAGE_STATUS: {
        s_last_status_update_us = esp_timer_get_time();
        break;
    }

    case MOONRAKER_WEBSOCKET_MESSAGE_FILELIST_CHANGED:
        __atomic_store_n(&s_file_change_pending, true, __ATOMIC_RELEASE);
        ESP_LOGI(TAG, "WS_FILELIST_CHANGED generation=%u",
                 (unsigned)s_generation);
        break;

    case MOONRAKER_WEBSOCKET_MESSAGE_KLIPPY_READY:
        /* A Klippy restart invalidates the old object subscription. */
        s_last_status_update_us = 0;
        s_subscribed = false;
        s_subscription_ready = false;
        s_subscribe_pending = false;
        s_discovery_pending = true;
        ESP_LOGI(TAG, "WS_KLIPPY_READY rediscover generation=%u",
                 (unsigned)s_generation);
        operator_event_log_add(
            OPERATOR_EVENT_INFO,
            "Klipper ready; capabilities rediscovered");
        break;

    case MOONRAKER_WEBSOCKET_MESSAGE_KLIPPY_SHUTDOWN:
        s_last_status_update_us = esp_timer_get_time();
        ESP_LOGW(TAG, "WS_KLIPPY_SHUTDOWN generation=%u",
                 (unsigned)s_generation);
        operator_event_log_add(
            OPERATOR_EVENT_ERROR,
            "Klipper reported shutdown");
        break;

    case MOONRAKER_WEBSOCKET_MESSAGE_KLIPPY_DISCONNECTED:
        s_last_status_update_us = 0;
        s_subscribed = false;
        ESP_LOGW(TAG, "WS_KLIPPY_DISCONNECTED generation=%u",
                 (unsigned)s_generation);
        operator_event_log_add(
            OPERATOR_EVENT_WARNING,
            "Klipper disconnected");
        break;

    case MOONRAKER_WEBSOCKET_MESSAGE_IGNORED:
    default:
        break;
    }
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
        s_subscription_ready = false;
        s_subscribe_pending = false;
        s_discovery_pending = true;
        ESP_LOGI(TAG, "WS_CONNECTED %s", s_uri);
        operator_event_log_add(
            OPERATOR_EVENT_INFO,
            "Moonraker live connection established");
        break;

    case WEBSOCKET_EVENT_DISCONNECTED:
        s_connected = false;
        s_subscribed = false;
        s_subscribe_pending = false;
        s_last_status_update_us = 0;
        moonraker_state_set_connection(false, false);
        ESP_LOGW(TAG, "WS_DISCONNECTED %s", s_uri);
        operator_event_log_add(
            OPERATOR_EVENT_WARNING,
            "Moonraker live connection lost");
        break;

    case WEBSOCKET_EVENT_DATA: {
        esp_websocket_event_data_t *data =
            (esp_websocket_event_data_t *)event_data;

        /* WS_REBIND_CLIENT_IDENTITY_GUARD
         * stop() may deliver a final queued frame from the retiring client.
         * Generation alone cannot reject it until the replacement endpoint
         * has been installed, so require exact client ownership as well.
         */
        if (!data || data->client != s_client) {
            ESP_LOGW(TAG, "WS_STALE_CLIENT_EVENT discarded");
            break;
        }

        handle_websocket_data(data);
        break;
    }

    case WEBSOCKET_EVENT_ERROR:
        ESP_LOGW(TAG, "WS_ERROR %s", s_uri);
        operator_event_log_add(
            OPERATOR_EVENT_WARNING,
            "Moonraker WebSocket error");
        break;

    case WEBSOCKET_EVENT_CLOSED:
        s_connected = false;
        s_subscribed = false;
        s_subscribe_pending = false;
        s_last_status_update_us = 0;
        moonraker_state_set_connection(false, false);
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
    s_last_status_update_us = 0;
    s_message_generation = 0;
    __atomic_store_n(&s_file_change_pending, false, __ATOMIC_RELEASE);

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

    const esp_app_desc_t *app = esp_app_get_description();
    const char *app_version =
        app && app->version[0] ? app->version : "4.0.0";

    char request[512];
    int length = snprintf(
        request,
        sizeof(request),
        "{\"jsonrpc\":\"2.0\","
        "\"method\":\"server.connection.identify\","
        "\"params\":{"
        "\"client_name\":\"PrinterHMI\","
        "\"version\":\"%s\","
        "\"type\":\"display\","
        "\"url\":\"https://github.com/\","
        "\"api_key\":\"%s\"},"
        "\"id\":1000}",
        app_version,
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


static bool send_object_list(void)
{
    if (!s_client || !s_connected) return false;

    static const char request[] =
        "{\"jsonrpc\":\"2.0\","
        "\"method\":\"printer.objects.list\","
        "\"id\":1001}";

    int length = (int)strlen(request);
    int sent = esp_websocket_client_send_text(
        s_client,
        request,
        length,
        pdMS_TO_TICKS(1000));

    if (sent != length) {
        ESP_LOGW(TAG, "WS object-list send failed: %d/%d", sent, length);
        return false;
    }

    ESP_LOGI(TAG, "WS_OBJECT_LIST_SENT generation=%u",
             (unsigned)s_generation);
    return true;
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

    ESP_LOGI(
        TAG,
        "WS_SUBSCRIBE_SENT hotends=%u generation=%u",
        (unsigned)s_subscription_hotend_count,
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

    /* A replacement connection is not fresh until it merges its own first
     * subscription status message.
     */
    s_last_status_update_us = 0;
    s_message_generation = 0;

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

    if (s_connected && s_discovery_pending) {
        s_discovery_pending = false;

        bool identified = send_identify();
        bool requested = identified && send_object_list();

        if (!requested) {
            s_discovery_pending = true;
            ESP_LOGW(TAG, "WS discovery transaction failed");
        }
    }

    if (s_connected &&
        s_subscription_ready &&
        s_subscribe_pending) {
        s_subscribe_pending = false;

        bool subscribed = send_subscription();
        s_subscribed = subscribed;

        if (!subscribed) {
            s_subscribe_pending = true;
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


bool moonraker_live_websocket_fresh(int64_t maximum_age_us)
{
    int64_t updated = s_last_status_update_us;
    if (!s_connected || !s_subscribed || updated <= 0 || maximum_age_us <= 0) {
        return false;
    }

    int64_t age = esp_timer_get_time() - updated;
    return age >= 0 && age <= maximum_age_us;
}


bool moonraker_live_websocket_file_change_pending(void)
{
    return __atomic_load_n(&s_file_change_pending, __ATOMIC_ACQUIRE);
}


bool moonraker_live_websocket_take_file_change(void)
{
    return __atomic_exchange_n(
        &s_file_change_pending,
        false,
        __ATOMIC_ACQ_REL);
}
