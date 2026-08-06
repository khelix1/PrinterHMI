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
#include "console_controller.h"
#include "macro_controller.h"
#include "device_catalog_controller.h"
#include "calibration_capability_controller.h"


#define TAG "moon_live_ws"
#define WS_RETRY_INTERVAL_US 3000000LL
/* Profile changes remain owned by the network task. Once that task observes
 * the new generation, retire and reopen during the same pass as v5.0 did.
 * v6.0.0 generation fencing still rejects every event from the old client.
 */
#define WS_MESSAGE_MAX_BYTES (128 * 1024)
#define WS_SUBSCRIPTION_CAPACITY (16 * 1024)
#define WS_COMMAND_CAPACITY 1024
#define WS_COMMAND_ID_FIRST 2000U


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
static uint32_t s_accepted_generation = 0;
static int64_t s_retry_after_us = 0;
static char s_host[128] = "";
static char s_api_key[160] = "";
static char s_uri[256] = "";

static char *s_subscription = NULL;
static size_t s_subscription_capacity = 0;
static size_t s_subscription_hotend_count = 0;
static size_t s_subscription_generic_count = 0;

static char *s_command_buffer = NULL;
static size_t s_command_capacity = 0;
static uint32_t s_command_request_id =
    WS_COMMAND_ID_FIRST;


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



static bool ensure_subscription_buffer(void)
{
    if (s_subscription &&
        s_subscription_capacity >= WS_SUBSCRIPTION_CAPACITY) {
        return true;
    }

    /*
     * The subscription exists for the full application lifetime. Allocate it
     * once from PSRAM and keep it; internal RAM is only the failure fallback.
     */
    s_subscription = heap_caps_calloc(
        1,
        WS_SUBSCRIPTION_CAPACITY,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (s_subscription) {
        s_subscription_capacity = WS_SUBSCRIPTION_CAPACITY;
        ESP_LOGI(
            TAG,
            "WS subscription allocated permanently in PSRAM: %u bytes",
            (unsigned)s_subscription_capacity);
        return true;
    }

    s_subscription = heap_caps_calloc(
        1,
        WS_SUBSCRIPTION_CAPACITY,
        MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

    if (!s_subscription) {
        s_subscription_capacity = 0;
        ESP_LOGE(TAG, "WS subscription allocation failed");
        return false;
    }

    s_subscription_capacity = WS_SUBSCRIPTION_CAPACITY;
    ESP_LOGW(
        TAG,
        "WS subscription using internal RAM fallback: %u bytes",
        (unsigned)s_subscription_capacity);
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


static size_t object_list_collect_prefix(
    cJSON *objects,
    const char *prefix,
    char names[][MOONRAKER_FILAMENT_SENSOR_NAME_MAX],
    size_t count)
{
    if (!cJSON_IsArray(objects) ||
        !prefix ||
        !prefix[0] ||
        !names) {
        return count;
    }

    size_t prefix_length = strlen(prefix);
    cJSON *entry = NULL;

    cJSON_ArrayForEach(entry, objects) {
        if (count >= MOONRAKER_MAX_FILAMENT_SENSORS) {
            break;
        }

        if (!cJSON_IsString(entry) ||
            !entry->valuestring ||
            strncmp(
                entry->valuestring,
                prefix,
                prefix_length) != 0) {
            continue;
        }

        copy_text(
            names[count],
            MOONRAKER_FILAMENT_SENSOR_NAME_MAX,
            entry->valuestring);
        ++count;
    }

    return count;
}


static size_t object_list_count_prefix(
    cJSON *objects,
    const char *prefix)
{
    if (!cJSON_IsArray(objects) ||
        !prefix ||
        !prefix[0]) {
        return 0;
    }

    size_t count = 0;
    size_t prefix_length = strlen(prefix);
    cJSON *entry = NULL;

    cJSON_ArrayForEach(entry, objects) {
        if (cJSON_IsString(entry) &&
            entry->valuestring &&
            strncmp(
                entry->valuestring,
                prefix,
                prefix_length) == 0) {
            ++count;
        }
    }

    return count;
}


static bool append_subscription_text(
    size_t *used,
    const char *text)
{
    if (!used || !text || !s_subscription ||
        *used >= s_subscription_capacity) {
        return false;
    }

    int written = snprintf(
        s_subscription + *used,
        s_subscription_capacity - *used,
        "%s",
        text);

    if (written < 0 ||
        (size_t)written >= s_subscription_capacity - *used) {
        return false;
    }

    *used += (size_t)written;
    return true;
}


static bool append_subscription_object(
    size_t *used,
    const char *object_name,
    const char *fields)
{
    if (!used || !object_name || !object_name[0] ||
        !s_subscription || *used >= s_subscription_capacity) {
        return false;
    }

    int written = snprintf(
        s_subscription + *used,
        s_subscription_capacity - *used,
        "\"%s\":%s,",
        object_name,
        fields ? fields : "null");

    if (written < 0 ||
        (size_t)written >=
            s_subscription_capacity - *used) {
        return false;
    }

    *used += (size_t)written;
    return true;
}


static bool build_subscription(
    const char names[][MOONRAKER_HOTEND_NAME_MAX],
    size_t count,
    const char filament_names[]
        [MOONRAKER_FILAMENT_SENSOR_NAME_MAX],
    size_t filament_count,
    const moonraker_capabilities_t *capabilities,
    cJSON *objects)
{
    if (!capabilities ||
        !cJSON_IsArray(objects) ||
        !ensure_subscription_buffer()) {
        return false;
    }

    size_t used = 0;
    s_subscription_generic_count = 0;
    s_subscription[0] = '\0';

    if (!append_subscription_text(
            &used,
            "{\"jsonrpc\":\"2.0\","
            "\"method\":\"printer.objects.subscribe\","
            "\"params\":{\"objects\":{"
            "\"print_stats\":null,"
            "\"motion_report\":null,"
            "\"display_status\":null,"
            "\"virtual_sdcard\":null,"
            "\"gcode_move\":null,")) {
        return false;
    }

    if (capabilities->has_drybox_center_sensor &&
        !append_subscription_object(
            &used,
            "temperature_sensor drybox_center",
            NULL)) {
        return false;
    }

    if (capabilities->has_drybox_environment_sensor &&
        !append_subscription_object(
            &used,
            "sht3x drybox_env",
            NULL)) {
        return false;
    }

    if (capabilities->has_drybox_heater &&
        !append_subscription_object(
            &used,
            "heater_generic drybox_heater",
            NULL)) {
        return false;
    }

    if (capabilities->has_drybox_fan &&
        !append_subscription_object(
            &used,
            "fan_generic drybox_fan",
            NULL)) {
        return false;
    }

    if (capabilities->has_drybox_macros &&
        !append_subscription_object(
            &used,
            "gcode_macro DRYBOX_VARS",
            NULL)) {
        return false;
    }

    if (capabilities->has_part_fan &&
        !append_subscription_object(
            &used,
            "fan",
            NULL)) {
        return false;
    }

    for (size_t i = 0; i < count; ++i) {
        if (!append_subscription_object(
                &used,
                names[i],
                NULL)) {
            return false;
        }
    }

    if (capabilities->has_heated_bed &&
        !append_subscription_object(
            &used,
            "heater_bed",
            NULL)) {
        return false;
    }

    for (size_t i = 0; i < filament_count; ++i) {
        if (!append_subscription_object(
                &used,
                filament_names[i],
                "[\"enabled\",\"filament_detected\"]")) {
            return false;
        }
    }

    cJSON *entry = NULL;
    cJSON_ArrayForEach(entry, objects) {
        if (!cJSON_IsString(entry) ||
            !entry->valuestring ||
            !entry->valuestring[0]) {
            continue;
        }

        const char *fields = NULL;

        if (!device_catalog_controller_subscription_fields(
                entry->valuestring,
                &fields)) {
            continue;
        }

        if (!append_subscription_object(
                &used,
                entry->valuestring,
                fields)) {
            return false;
        }

        ++s_subscription_generic_count;
    }

    if (capabilities->has_exclude_object &&
        !append_subscription_object(
            &used,
            "exclude_object",
            "[\"objects\",\"excluded_objects\",\"current_object\"]")) {
        return false;
    }

    if (capabilities->has_bed_mesh &&
        !append_subscription_object(
            &used,
            "bed_mesh",
            "[\"profile_name\",\"profiles\",\"mesh_min\",\"mesh_max\",\"probed_matrix\",\"mesh_matrix\"]")) {
        return false;
    }

    if (!append_subscription_text(
            &used,
            "\"gcode\":[\"commands\"],"
            "\"toolhead\":null}},"
            "\"id\":1002}")) {
        return false;
    }

    s_subscription_hotend_count = count;
    return true;
}



static bool ensure_command_buffer(void)
{
    if (s_command_buffer &&
        s_command_capacity >= WS_COMMAND_CAPACITY) {
        return true;
    }

    s_command_buffer = heap_caps_calloc(
        1,
        WS_COMMAND_CAPACITY,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (s_command_buffer) {
        s_command_capacity = WS_COMMAND_CAPACITY;
        ESP_LOGI(
            TAG,
            "WS command buffer allocated permanently in PSRAM: %u bytes",
            (unsigned)s_command_capacity);
        return true;
    }

    s_command_buffer = heap_caps_calloc(
        1,
        WS_COMMAND_CAPACITY,
        MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

    if (!s_command_buffer) {
        s_command_capacity = 0;
        ESP_LOGE(TAG, "WS command buffer allocation failed");
        return false;
    }

    s_command_capacity = WS_COMMAND_CAPACITY;
    ESP_LOGW(
        TAG,
        "WS command buffer using internal RAM fallback");
    return true;
}


static bool append_command_text(
    size_t *used,
    const char *text)
{
    if (!used || !text || !s_command_buffer) {
        return false;
    }

    size_t length = strlen(text);
    if (length >= s_command_capacity - *used) {
        return false;
    }

    memcpy(
        s_command_buffer + *used,
        text,
        length);
    *used += length;
    s_command_buffer[*used] = '\0';
    return true;
}


static bool append_command_json_string(
    size_t *used,
    const char *text)
{
    if (!used || !text) {
        return false;
    }

    for (const unsigned char *cursor =
             (const unsigned char *)text;
         *cursor;
         ++cursor) {
        const char *escape = NULL;
        char plain[2] = {(char)*cursor, '\0'};

        switch (*cursor) {
        case '\\':
            escape = "\\\\";
            break;
        case '"':
            escape = "\\\"";
            break;
        case '\n':
            escape = "\\n";
            break;
        case '\r':
            escape = "\\r";
            break;
        case '\t':
            escape = "\\t";
            break;
        default:
            if (*cursor < 0x20) {
                return false;
            }
            escape = plain;
            break;
        }

        if (!append_command_text(used, escape)) {
            return false;
        }
    }

    return true;
}


bool moonraker_live_websocket_send_gcode(
    const char *script)
{
    if (!script || !script[0] ||
        !s_client || !s_connected ||
        !ensure_command_buffer()) {
        return false;
    }

    uint32_t request_id =
        __atomic_fetch_add(
            &s_command_request_id,
            1U,
            __ATOMIC_RELAXED);

    if (request_id < WS_COMMAND_ID_FIRST) {
        request_id = WS_COMMAND_ID_FIRST;
        __atomic_store_n(
            &s_command_request_id,
            request_id + 1U,
            __ATOMIC_RELAXED);
    }

    size_t used = 0;
    s_command_buffer[0] = '\0';

    if (!append_command_text(
            &used,
            "{\"jsonrpc\":\"2.0\","
            "\"method\":\"printer.gcode.script\","
            "\"params\":{\"script\":\"") ||
        !append_command_json_string(
            &used,
            script)) {
        ESP_LOGE(TAG, "WS command request overflow");
        return false;
    }

    char suffix[48];
    int suffix_length = snprintf(
        suffix,
        sizeof(suffix),
        "\"},\"id\":%u}",
        (unsigned)request_id);

    if (suffix_length <= 0 ||
        (size_t)suffix_length >= sizeof(suffix) ||
        !append_command_text(&used, suffix)) {
        ESP_LOGE(TAG, "WS command suffix overflow");
        return false;
    }

    int sent = esp_websocket_client_send_text(
        s_client,
        s_command_buffer,
        (int)used,
        pdMS_TO_TICKS(1000));

    if (sent != (int)used) {
        ESP_LOGW(
            TAG,
            "WS command send failed: %d/%u",
            sent,
            (unsigned)used);
        return false;
    }

    ESP_LOGI(
        TAG,
        "WS_GCODE_SENT id=%u command=%.96s",
        (unsigned)request_id,
        script);
    return true;
}


static bool handle_command_response(
    const char *json,
    size_t length)
{
    cJSON *root =
        cJSON_ParseWithLength(json, length);
    if (!root) {
        return false;
    }

    cJSON *id =
        cJSON_GetObjectItemCaseSensitive(
            root,
            "id");

    if (!cJSON_IsNumber(id) ||
        id->valuedouble < WS_COMMAND_ID_FIRST) {
        cJSON_Delete(root);
        return false;
    }

    cJSON *error =
        cJSON_GetObjectItemCaseSensitive(
            root,
            "error");

    if (cJSON_IsObject(error)) {
        cJSON *message =
            cJSON_GetObjectItemCaseSensitive(
                error,
                "message");

        console_controller_add(
            CONSOLE_ENTRY_ERROR,
            "Moonraker command error: %s",
            cJSON_IsString(message) &&
                    message->valuestring
                ? message->valuestring
                : "Unknown JSON-RPC error");
    }

    cJSON_Delete(root);
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

    macro_controller_update_from_objects(objects);
    device_catalog_controller_update_from_objects(objects);

    static const char *candidates[MOONRAKER_MAX_HOTENDS] = {
        "extruder",
        "extruder1",
        "extruder2",
        "extruder3",
    };

    char names[MOONRAKER_MAX_HOTENDS]
              [MOONRAKER_HOTEND_NAME_MAX] = {{0}};
    size_t count = 0;

    char filament_names[MOONRAKER_MAX_FILAMENT_SENSORS]
                       [MOONRAKER_FILAMENT_SENSOR_NAME_MAX] = {{0}};
    size_t filament_count = 0;

    moonraker_capabilities_t capabilities = {
        .discovered = true,
        .has_heated_bed =
            object_list_contains(
                objects,
                "heater_bed"),
        .has_part_fan =
            object_list_contains(
                objects,
                "fan"),
        .has_exclude_object =
            object_list_contains(
                objects,
                "exclude_object"),
        .has_bed_mesh =
            object_list_contains(
                objects,
                "bed_mesh"),
        .has_drybox_center_sensor =
            object_list_contains(
                objects,
                "temperature_sensor drybox_center"),
        .has_drybox_environment_sensor =
            object_list_contains(
                objects,
                "sht3x drybox_env"),
        .has_drybox_heater =
            object_list_contains(
                objects,
                "heater_generic drybox_heater"),
        .has_drybox_fan =
            object_list_contains(
                objects,
                "fan_generic drybox_fan"),
        .has_drybox_macros =
            object_list_contains(
                objects,
                "gcode_macro DRYBOX_VARS"),
    };

    capabilities.filament_sensor_count =
        object_list_count_prefix(
            objects,
            "filament_switch_sensor ") +
        object_list_count_prefix(
            objects,
            "filament_motion_sensor ");

    filament_count =
        object_list_collect_prefix(
            objects,
            "filament_switch_sensor ",
            filament_names,
            filament_count);

    filament_count =
        object_list_collect_prefix(
            objects,
            "filament_motion_sensor ",
            filament_names,
            filament_count);

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
    moonraker_filament_state_configure(
        filament_names,
        filament_count,
        capabilities.filament_sensor_count);
    moonraker_state_configure_capabilities(
        &capabilities);

    if (!build_subscription(
            names,
            count,
            filament_names,
            filament_count,
            &capabilities,
            objects)) {
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
        "WS_CAPABILITIES hotends=%u bed=%d fan=%d drybox=%d "
        "filament_sensors=%u subscription ready",
        (unsigned)count,
        capabilities.has_heated_bed,
        capabilities.has_part_fan,
        capabilities.has_drybox_center_sensor ||
            capabilities.has_drybox_environment_sensor ||
            capabilities.has_drybox_heater ||
            capabilities.has_drybox_fan ||
            capabilities.has_drybox_macros,
        (unsigned)capabilities.filament_sensor_count);

    operator_event_log_add(
        OPERATOR_EVENT_INFO,
        "Capabilities: %u hotend%s, bed %s, fan %s, drybox %s, "
        "filament %u",
        (unsigned)count,
        count == 1 ? "" : "s",
        capabilities.has_heated_bed ? "yes" : "no",
        capabilities.has_part_fan ? "yes" : "no",
        (capabilities.has_drybox_center_sensor ||
         capabilities.has_drybox_environment_sensor ||
         capabilities.has_drybox_heater ||
         capabilities.has_drybox_fan ||
         capabilities.has_drybox_macros)
            ? "yes"
            : "no",
        (unsigned)capabilities.filament_sensor_count);

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

    if (console_controller_ingest_websocket_json(
            s_message_buffer,
            (size_t)payload_length)) {
        return;
    }

    if (handle_command_response(
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
        device_catalog_controller_reset();
        calibration_capability_controller_reset();
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
        device_catalog_controller_reset();
        calibration_capability_controller_reset();
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
    (void)event_base;

    esp_websocket_client_handle_t event_client =
        (esp_websocket_client_handle_t)handler_argument;
    bool current_owner =
        event_client &&
        event_client == s_client &&
        s_generation == __atomic_load_n(
            &s_accepted_generation,
            __ATOMIC_ACQUIRE);

    if ((esp_websocket_event_id_t)event_id !=
            WEBSOCKET_EVENT_BEFORE_CONNECT &&
        !current_owner) {
        ESP_LOGW(TAG, "WS_STALE_CLIENT_EVENT discarded id=%ld",
                 (long)event_id);
        return;
    }

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
        device_catalog_controller_reset();
        calibration_capability_controller_reset();
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

        if (!data || data->client != event_client) {
            ESP_LOGW(TAG, "WS_INVALID_CLIENT_EVENT discarded");
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
        device_catalog_controller_reset();
        calibration_capability_controller_reset();
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
    device_catalog_controller_reset();
    calibration_capability_controller_reset();

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
    __atomic_store_n(&s_accepted_generation, 0, __ATOMIC_RELEASE);
    s_retry_after_us = 0;
}


void moonraker_live_websocket_prepare_profile_change(
    uint32_t configuration_generation)
{
    if (!s_client || configuration_generation == s_generation) return;

    ESP_LOGI(TAG, "WS_REBIND_IMMEDIATE generation=%u->%u",
             (unsigned)s_generation,
             (unsigned)configuration_generation);

    /*
     * Destroy the old endpoint before any request for the new profile starts.
     * The next runtime tick creates the new client without a settle timer.
     */
    moonraker_live_websocket_stop();
}


static bool send_identify(void)
{
    if (!s_client || !s_connected) return false;

    const esp_app_desc_t *app = esp_app_get_description();
    const char *app_version =
        app && app->version[0] ? app->version : "unknown";

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
        0);

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
        0);

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
        0);

    if (sent != length) {
        ESP_LOGW(TAG, "WS subscription send failed: %d/%d", sent, length);
        return false;
    }

    ESP_LOGI(
        TAG,
        "WS_SUBSCRIBE_SENT hotends=%u generic=%u bytes=%u generation=%u",
        (unsigned)s_subscription_hotend_count,
        (unsigned)s_subscription_generic_count,
        (unsigned)length,
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
    __atomic_store_n(
        &s_accepted_generation,
        generation,
        __ATOMIC_RELEASE);

    /* A replacement connection is not fresh until it merges its own first
     * subscription status message.
     */
    s_last_status_update_us = 0;
    s_message_generation = 0;

    esp_websocket_client_config_t config = {
        .uri = s_uri,
        .task_stack = 4096,
        .buffer_size = 4096,
        .reconnect_timeout_ms = 5000,
        .network_timeout_ms = 10000,
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
        s_client);

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

    int64_t now = esp_timer_get_time();

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
        moonraker_live_websocket_prepare_profile_change(
            configuration_generation);
        return;
    }

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


bool moonraker_live_websocket_running(void)
{
    return s_client != NULL;
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
