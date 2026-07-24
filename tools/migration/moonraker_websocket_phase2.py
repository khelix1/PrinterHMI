#!/usr/bin/env python3
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
MAIN = ROOT / "main" / "main.c"
CMAKE = ROOT / "main" / "CMakeLists.txt"
MOON_H = ROOT / "main" / "moonraker.h"
MOON_C = ROOT / "main" / "moonraker.c"
WS_H = ROOT / "main" / "moonraker_live_websocket.h"
WS_C = ROOT / "main" / "moonraker_live_websocket.c"
CHOOSER = ROOT / "main" / "ui_printer_chooser_v32.c"


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"expected one {label}, found {count}")
    return text.replace(old, new, 1)


for required in (MAIN, CMAKE, MOON_H, MOON_C, WS_H, WS_C, CHOOSER):
    if not required.exists():
        raise RuntimeError(f"missing required file: {required}")

main = MAIN.read_text()
cmake = CMAKE.read_text()
moon_h = MOON_H.read_text()
moon_c = MOON_C.read_text()
ws_h = WS_H.read_text()
ws_c = WS_C.read_text()
chooser = CHOOSER.read_text()

if "MOONRAKER_WEBSOCKET_PHASE2" in main:
    print("PASS: Moonraker WebSocket Phase 2 already installed")
    raise SystemExit(0)

if "MOONRAKER_WEBSOCKET_PHASE1" not in main:
    raise RuntimeError("Phase 1 marker missing from main.c")


# -------------------------------------------------------------------------
# Component dependency
# -------------------------------------------------------------------------

if "        json\n" not in cmake:
    cmake = replace_once(
        cmake,
        "        esp_http_client\n",
        "        esp_http_client\n        json\n",
        "HTTP component requirement",
    )


# -------------------------------------------------------------------------
# State model and synchronized public API
# -------------------------------------------------------------------------

moon_h = replace_once(
    moon_h,
    '''    double humidity;

    double heater_target;
''',
    '''    double humidity;

    int drybox_selected_program;
    int drybox_active_program;

    double heater_target;
''',
    "Moonraker Drybox state fields",
)

moon_h = replace_once(
    moon_h,
    '''const moonraker_state_t *moonraker_state_get(void);
void moonraker_state_reset(void);
''',
    '''const moonraker_state_t *moonraker_state_get(void);
void moonraker_state_snapshot(moonraker_state_t *out);
void moonraker_state_reset(void);

void moonraker_state_set_drybox_programs(
    int selected_program,
    int active_program);

void moonraker_state_set_connection(
    bool live_data_ok,
    bool moonraker_ok);

/*
 * Merge a complete Moonraker WebSocket JSON-RPC message. Both the initial
 * subscription result and incremental notify_status_update messages are
 * accepted. Missing fields retain their previous values.
 */
bool moonraker_state_merge_websocket_json(
    const char *json,
    size_t length);
''',
    "Moonraker synchronized state API",
)

moon_c = replace_once(
    moon_c,
    '#include "esp_heap_caps.h"\n',
    '#include "esp_heap_caps.h"\n'
    '#include "cJSON.h"\n'
    '#include "freertos/FreeRTOS.h"\n'
    '#include "freertos/semphr.h"\n',
    "Moonraker includes",
)

moon_c = replace_once(
    moon_c,
    '''void moonraker_module_init(void)
{
    moonraker_state_reset();
}

static moonraker_state_t g_moonraker_state;
''',
    '''static moonraker_state_t g_moonraker_state;
static StaticSemaphore_t s_state_mutex_buffer;
static SemaphoreHandle_t s_state_mutex = NULL;


static void state_lock(void)
{
    if (s_state_mutex) {
        xSemaphoreTake(s_state_mutex, portMAX_DELAY);
    }
}


static void state_unlock(void)
{
    if (s_state_mutex) {
        xSemaphoreGive(s_state_mutex);
    }
}


void moonraker_module_init(void)
{
    s_state_mutex =
        xSemaphoreCreateMutexStatic(&s_state_mutex_buffer);
    moonraker_state_reset();
}
''',
    "Moonraker state mutex initialization",
)

moon_c = replace_once(
    moon_c,
    '''const moonraker_state_t *moonraker_state_get(void)
{
    return &g_moonraker_state;
}

void moonraker_state_reset(void)
{
    memset(&g_moonraker_state, 0, sizeof(g_moonraker_state));
''',
    '''const moonraker_state_t *moonraker_state_get(void)
{
    /* Compatibility accessor. New asynchronous consumers use snapshots. */
    return &g_moonraker_state;
}


void moonraker_state_snapshot(moonraker_state_t *out)
{
    if (!out) return;

    state_lock();
    memcpy(out, &g_moonraker_state, sizeof(*out));
    state_unlock();
}


void moonraker_state_reset(void)
{
    state_lock();
    memset(&g_moonraker_state, 0, sizeof(g_moonraker_state));
''',
    "Moonraker snapshot/reset start",
)

moon_c = replace_once(
    moon_c,
    '''    mr_safe_copy(g_moonraker_state.printer_file, sizeof(g_moonraker_state.printer_file), "No file");
}

void moonraker_state_update_from_legacy(
''',
    '''    mr_safe_copy(g_moonraker_state.printer_file, sizeof(g_moonraker_state.printer_file), "No file");
    state_unlock();
}


void moonraker_state_set_drybox_programs(
    int selected_program,
    int active_program)
{
    state_lock();
    g_moonraker_state.drybox_selected_program = selected_program;
    g_moonraker_state.drybox_active_program = active_program;
    state_unlock();
}


void moonraker_state_set_connection(
    bool live_data_ok,
    bool moonraker_ok)
{
    state_lock();
    g_moonraker_state.live_data_ok = live_data_ok;
    g_moonraker_state.moonraker_ok = moonraker_ok;
    state_unlock();
}


void moonraker_state_update_from_legacy(
''',
    "Moonraker reset end and Drybox setter",
)

moon_c = replace_once(
    moon_c,
    '''{
    g_moonraker_state.chamber_temp = chamber_temp;
''',
    '''{
    state_lock();
    g_moonraker_state.chamber_temp = chamber_temp;
''',
    "legacy state update lock",
)

moon_c = replace_once(
    moon_c,
    '''    mr_safe_copy(g_moonraker_state.printer_state, sizeof(g_moonraker_state.printer_state), printer_state);
    mr_safe_copy(g_moonraker_state.printer_file, sizeof(g_moonraker_state.printer_file), printer_file);
}


const char *json_find_string''',
    '''    mr_safe_copy(g_moonraker_state.printer_state, sizeof(g_moonraker_state.printer_state), printer_state);
    mr_safe_copy(g_moonraker_state.printer_file, sizeof(g_moonraker_state.printer_file), printer_file);
    state_unlock();
}


static cJSON *json_status_object(cJSON *root)
{
    cJSON *method = cJSON_GetObjectItemCaseSensitive(root, "method");

    if (cJSON_IsString(method) && method->valuestring &&
        strcmp(method->valuestring, "notify_status_update") == 0) {
        cJSON *params = cJSON_GetObjectItemCaseSensitive(root, "params");
        return cJSON_IsArray(params)
            ? cJSON_GetArrayItem(params, 0)
            : NULL;
    }

    cJSON *result = cJSON_GetObjectItemCaseSensitive(root, "result");
    return cJSON_IsObject(result)
        ? cJSON_GetObjectItemCaseSensitive(result, "status")
        : NULL;
}


static bool json_number(
    cJSON *object,
    const char *name,
    double *out)
{
    cJSON *item = cJSON_IsObject(object)
        ? cJSON_GetObjectItemCaseSensitive(object, name)
        : NULL;

    if (!cJSON_IsNumber(item) || !out) return false;
    *out = item->valuedouble;
    return true;
}


bool moonraker_state_merge_websocket_json(
    const char *json,
    size_t length)
{
    if (!json || length == 0) return false;

    cJSON *root = cJSON_ParseWithLength(json, length);
    if (!root) return false;

    cJSON *status = json_status_object(root);
    if (!cJSON_IsObject(status)) {
        cJSON_Delete(root);
        return false;
    }

    int updates = 0;
    double value = 0.0;

    state_lock();

#define MERGE_NUMBER(object_name, field_name, destination, scale) do { \
    cJSON *object = cJSON_GetObjectItemCaseSensitive(status, object_name); \
    if (json_number(object, field_name, &value)) { \
        g_moonraker_state.destination = value * (scale); \
        ++updates; \
    } \
} while (0)

    MERGE_NUMBER("temperature_sensor drybox_center", "temperature", chamber_temp, 1.0);
    MERGE_NUMBER("sht3x drybox_env", "temperature", air_temp, 1.0);
    MERGE_NUMBER("sht3x drybox_env", "humidity", humidity, 1.0);
    MERGE_NUMBER("heater_generic drybox_heater", "target", heater_target, 1.0);
    MERGE_NUMBER("fan_generic drybox_fan", "speed", drybox_fan_speed, 100.0);
    MERGE_NUMBER("fan", "speed", part_fan_speed, 100.0);
    MERGE_NUMBER("gcode_move", "speed_factor", speed_factor, 100.0);
    MERGE_NUMBER("gcode_move", "extrude_factor", flow_factor, 100.0);
    MERGE_NUMBER("motion_report", "live_velocity", live_velocity, 1.0);
    MERGE_NUMBER("motion_report", "live_extruder_velocity", live_flow, 1.0);
    MERGE_NUMBER("extruder", "temperature", nozzle_temp, 1.0);
    MERGE_NUMBER("extruder", "target", nozzle_target, 1.0);
    MERGE_NUMBER("heater_bed", "temperature", bed_temp, 1.0);
    MERGE_NUMBER("heater_bed", "target", bed_target, 1.0);
    MERGE_NUMBER("display_status", "progress", progress, 1.0);
    MERGE_NUMBER("print_stats", "print_duration", print_duration, 1.0);

#undef MERGE_NUMBER

    cJSON *heater = cJSON_GetObjectItemCaseSensitive(
        status, "heater_generic drybox_heater");
    if (json_number(heater, "power", &value)) {
        g_moonraker_state.heater_on = value > 0.01;
        ++updates;
    }

    cJSON *macro = cJSON_GetObjectItemCaseSensitive(
        status, "gcode_macro DRYBOX_VARS");
    if (json_number(macro, "selected_program", &value)) {
        g_moonraker_state.drybox_selected_program = (int)value;
        ++updates;
    }
    if (json_number(macro, "active_program", &value)) {
        g_moonraker_state.drybox_active_program = (int)value;
        ++updates;
    }

    cJSON *print_stats = cJSON_GetObjectItemCaseSensitive(
        status, "print_stats");
    cJSON *state = cJSON_IsObject(print_stats)
        ? cJSON_GetObjectItemCaseSensitive(print_stats, "state")
        : NULL;
    cJSON *filename = cJSON_IsObject(print_stats)
        ? cJSON_GetObjectItemCaseSensitive(print_stats, "filename")
        : NULL;

    if (cJSON_IsString(state) && state->valuestring) {
        mr_safe_copy(
            g_moonraker_state.printer_state,
            sizeof(g_moonraker_state.printer_state),
            state->valuestring);
        ++updates;
    }

    if (cJSON_IsString(filename) && filename->valuestring) {
        mr_safe_copy(
            g_moonraker_state.printer_file,
            sizeof(g_moonraker_state.printer_file),
            filename->valuestring[0] ? filename->valuestring : "No file");
        ++updates;
    }

    cJSON *info = cJSON_IsObject(print_stats)
        ? cJSON_GetObjectItemCaseSensitive(print_stats, "info")
        : NULL;
    if (json_number(info, "current_layer", &value)) {
        g_moonraker_state.current_layer = (int)(value + 0.5);
        ++updates;
    }
    if (json_number(info, "total_layer", &value)) {
        g_moonraker_state.total_layer = (int)(value + 0.5);
        ++updates;
    }

    if (updates > 0) {
        g_moonraker_state.live_data_ok = true;
        g_moonraker_state.moonraker_ok = true;
    }

    state_unlock();
    cJSON_Delete(root);
    return updates > 0;
}


const char *json_find_string''',
    "WebSocket incremental JSON merge",
)


# -------------------------------------------------------------------------
# Fragment-safe WebSocket assembly and freshness
# -------------------------------------------------------------------------

ws_h = replace_once(
    ws_h,
    '''bool moonraker_live_websocket_connected(void);
bool moonraker_live_websocket_subscribed(void);
void moonraker_live_websocket_stop(void);
''',
    '''bool moonraker_live_websocket_connected(void);
bool moonraker_live_websocket_subscribed(void);
bool moonraker_live_websocket_fresh(int64_t maximum_age_us);
void moonraker_live_websocket_stop(void);
''',
    "WebSocket freshness API",
)

ws_c = replace_once(
    ws_c,
    '#include "esp_log.h"\n',
    '#include "esp_log.h"\n#include "esp_heap_caps.h"\n',
    "WebSocket heap include",
)
ws_c = replace_once(
    ws_c,
    '#include "freertos/FreeRTOS.h"\n',
    '#include "freertos/FreeRTOS.h"\n#include "moonraker.h"\n',
    "WebSocket state include",
)

ws_c = replace_once(
    ws_c,
    '''static volatile uint32_t s_receive_count = 0;

static uint32_t s_generation = 0;
''',
    '''static volatile uint32_t s_receive_count = 0;
static volatile uint32_t s_status_merge_count = 0;
static volatile int64_t s_last_status_update_us = 0;

static char *s_message_buffer = NULL;
static size_t s_message_capacity = 0;
static uint32_t s_message_generation = 0;

static uint32_t s_generation = 0;
''',
    "WebSocket assembly state",
)

event_marker = '''static void websocket_event_handler(
'''
assembly_helper = r'''static bool ensure_message_capacity(size_t required)
{
    if (required <= s_message_capacity && s_message_buffer) return true;
    if (required > 32 * 1024) return false;

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

    if (moonraker_state_merge_websocket_json(
            s_message_buffer,
            (size_t)payload_length)) {
        s_last_status_update_us = esp_timer_get_time();

        uint32_t count = ++s_status_merge_count;
        if (count <= 5 || count % 100 == 0) {
            ESP_LOGI(TAG, "WS_STATUS_MERGED count=%u generation=%u",
                     (unsigned)count,
                     (unsigned)s_generation);
        }
    }
}


'''

ws_c = replace_once(
    ws_c,
    event_marker,
    assembly_helper + event_marker,
    "WebSocket event handler anchor",
)

old_data_case = '''    case WEBSOCKET_EVENT_DATA: {
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
'''

new_data_case = '''    case WEBSOCKET_EVENT_DATA: {
        esp_websocket_event_data_t *data =
            (esp_websocket_event_data_t *)event_data;

        handle_websocket_data(data);

        uint32_t count = ++s_receive_count;
        if (count <= 5 || count % 100 == 0) {
            ESP_LOGI(
                TAG,
                "WS_RX count=%u bytes=%d payload=%d offset=%d",
                (unsigned)count,
                data ? data->data_len : 0,
                data ? data->payload_len : 0,
                data ? data->payload_offset : 0);
        }
        break;
    }
'''

ws_c = replace_once(
    ws_c,
    old_data_case,
    new_data_case,
    "Phase 1 WebSocket data event",
)

ws_c = replace_once(
    ws_c,
    '''    s_connected = false;
    s_subscribed = false;
    s_subscribe_pending = false;

    if (!client) return;
''',
    '''    s_connected = false;
    s_subscribed = false;
    s_subscribe_pending = false;
    s_last_status_update_us = 0;
    s_status_merge_count = 0;
    s_message_generation = 0;

    if (!client) return;
''',
    "WebSocket destroy freshness reset",
)

ws_c = replace_once(
    ws_c,
    '''bool moonraker_live_websocket_subscribed(void)
{
    return s_connected && s_subscribed;
}
''',
    '''bool moonraker_live_websocket_subscribed(void)
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
''',
    "WebSocket freshness implementation",
)


# -------------------------------------------------------------------------
# Main bridge: HTTP fallback, state->legacy compatibility, safe snapshots
# -------------------------------------------------------------------------

main = replace_once(
    main,
    '''        printer_file);

return true;
''',
    '''        printer_file);

    moonraker_state_set_drybox_programs(
        (int)s_drybox_selected_program,
        (int)s_drybox_active_program);

return true;
''',
    "HTTP Drybox program state publication",
)

main = replace_once(
    main,
    '''        s_live_data_ok,
        s_moonraker_ok,
        printer_state,
''',
    '''        s_live_data_ok,
        true,
        printer_state,
''',
    "successful HTTP Moonraker state flag",
)

main = replace_once(
    main,
    '''    if (!transport_ok) {
        s_live_data_ok = false;
        return false;
    }
''',
    '''    if (!transport_ok) {
        s_live_data_ok = false;
        moonraker_state_set_connection(false, false);
        return false;
    }
''',
    "HTTP transport failure state",
)

main = replace_once(
    main,
    '''static void moonraker_live_poll_tasklet(void)
{
    moonraker_poll_result_t result =
''',
    '''static void moonraker_live_poll_tasklet(void)
{
    /* WebSocket is authoritative only while subscribed status is fresh.
     * The proven HTTP poller automatically resumes after three seconds.
     */
    if (!s_got_ip) {
        moonraker_state_set_connection(false, false);
    }

    static bool websocket_was_authoritative = false;
    bool websocket_is_fresh =
        moonraker_live_websocket_fresh(3000000LL);

    if (websocket_is_fresh) {
        if (!websocket_was_authoritative) {
            ESP_LOGI(TAG, "LIVE_TRANSPORT websocket authoritative");
        }
        websocket_was_authoritative = true;
        s_moonraker_code = 200;
        s_moonraker_ok = true;
        s_live_data_ok = true;
        return;
    }

    if (websocket_was_authoritative) {
        ESP_LOGW(TAG, "LIVE_TRANSPORT HTTP fallback");
    }
    websocket_was_authoritative = false;

    moonraker_poll_result_t result =
''',
    "HTTP polling fallback gate",
)

legacy_sync = r'''static void moonraker_sync_legacy_from_state(void)
{
    moonraker_state_t state;
    moonraker_state_snapshot(&state);

    live_chamber_temp = state.chamber_temp;
    live_air_temp = state.air_temp;
    live_humidity = state.humidity;
    live_heater_target = state.heater_target;
    live_heater_power = state.heater_on;
    live_fan_speed = state.drybox_fan_speed;
    printer_part_fan_speed = state.part_fan_speed;
    printer_speed_factor = state.speed_factor;
    printer_flow_factor = state.flow_factor;
    printer_live_velocity = state.live_velocity;
    printer_live_flow = state.live_flow;
    printer_nozzle_temp = state.nozzle_temp;
    printer_nozzle_target = state.nozzle_target;
    printer_bed_temp = state.bed_temp;
    printer_bed_target = state.bed_target;
    printer_progress = state.progress;
    printer_print_duration = state.print_duration;
    printer_current_layer = state.current_layer;
    printer_total_layer = state.total_layer;
    s_live_data_ok = state.live_data_ok;
    s_moonraker_ok = state.moonraker_ok;

    s_drybox_selected_program =
        (ui_drybox_program_v32_t)state.drybox_selected_program;
    s_drybox_active_program =
        (ui_drybox_program_v32_t)state.drybox_active_program;

    safe_copy(printer_state, sizeof(printer_state), state.printer_state);
    safe_copy(printer_file, sizeof(printer_file), state.printer_file);
}


'''

main = replace_once(
    main,
    "static void ui_refresh_timer_cb(lv_timer_t *timer)\n",
    legacy_sync + "static void ui_refresh_timer_cb(lv_timer_t *timer)\n",
    "UI refresh callback anchor",
)

main = replace_once(
    main,
    '''    ota_manager_pump_ui();


    (void)timer;
''',
    '''    ota_manager_pump_ui();
    moonraker_sync_legacy_from_state();

    (void)timer;
''',
    "UI refresh state bridge",
)

reader_block = '''    const moonraker_state_t *mr_state =
        moonraker_state_get();
'''
reader_replacement = '''    moonraker_state_t mr_state_snapshot;
    moonraker_state_snapshot(&mr_state_snapshot);
    const moonraker_state_t *mr_state = &mr_state_snapshot;
'''
reader_count = main.count(reader_block)
if reader_count != 4:
    raise RuntimeError(
        f"expected four main state reader blocks, found {reader_count}"
    )
main = main.replace(reader_block, reader_replacement)

main = replace_once(
    main,
    '''    ui_telemetry_v32_refresh(
        moonraker_state_get(),
        esp_timer_get_time());
''',
    '''    moonraker_state_t telemetry_state;
    moonraker_state_snapshot(&telemetry_state);

    ui_telemetry_v32_refresh(
        &telemetry_state,
        esp_timer_get_time());
''',
    "telemetry state reader",
)

main = main.replace(
    "        /* MOONRAKER_WEBSOCKET_PHASE1\n",
    "        /* MOONRAKER_WEBSOCKET_PHASE2\n",
    1,
)

chooser = replace_once(
    chooser,
    '''    int active = moonraker_config_active_profile_index();
    const moonraker_state_t *state = moonraker_state_get();
''',
    '''    int active = moonraker_config_active_profile_index();
    moonraker_state_t state_snapshot;
    moonraker_state_snapshot(&state_snapshot);
    const moonraker_state_t *state = &state_snapshot;
''',
    "chooser state reader",
)


CMAKE.write_text(cmake)
MOON_H.write_text(moon_h)
MOON_C.write_text(moon_c)
WS_H.write_text(ws_h)
WS_C.write_text(ws_c)
MAIN.write_text(main)
CHOOSER.write_text(chooser)

print("PASS: Moonraker WebSocket Phase 2 installed")
print("  - fragment-safe PSRAM message assembly")
print("  - incremental cJSON status merge")
print("  - mutex-protected state snapshots")
print("  - 500 ms UI compatibility bridge")
print("  - HTTP polling retained as 3-second stale fallback")
print("Next: idf.py build")

