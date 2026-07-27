#include "moonraker.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "esp_heap_caps.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <limits.h>
static moonraker_state_t g_moonraker_state;
static moonraker_exclude_state_t *g_moonraker_exclude_state = NULL;
static moonraker_filament_state_t
    *g_moonraker_filament_state = NULL;
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

    g_moonraker_filament_state = heap_caps_calloc(
        1,
        sizeof(*g_moonraker_filament_state),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (!g_moonraker_filament_state) {
        g_moonraker_filament_state =
            calloc(1, sizeof(*g_moonraker_filament_state));
    }

    g_moonraker_exclude_state = heap_caps_calloc(
        1,
        sizeof(*g_moonraker_exclude_state),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (!g_moonraker_exclude_state) {
        g_moonraker_exclude_state =
            calloc(1, sizeof(*g_moonraker_exclude_state));
    }

    moonraker_state_reset();
}

static void mr_safe_copy(char *dst, size_t dst_sz, const char *src)
{
    if (!dst || dst_sz == 0) return;
    if (!src) src = "";
    size_t n = strlen(src);
    if (n >= dst_sz) n = dst_sz - 1;
    memcpy(dst, src, n);
    dst[n] = 0;
}

const moonraker_state_t *moonraker_state_get(void)
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


void moonraker_filament_state_snapshot(
    moonraker_filament_state_t *out)
{
    if (!out) return;

    state_lock();
    if (g_moonraker_filament_state) {
        memcpy(
            out,
            g_moonraker_filament_state,
            sizeof(*out));
    } else {
        memset(out, 0, sizeof(*out));
    }
    state_unlock();
}


void moonraker_exclude_state_snapshot(moonraker_exclude_state_t *out)
{
    if (!out) return;

    state_lock();
    if (g_moonraker_exclude_state) {
        memcpy(out, g_moonraker_exclude_state, sizeof(*out));
    } else {
        memset(out, 0, sizeof(*out));
    }
    state_unlock();
}


bool moonraker_exclude_objects_available(void)
{
    bool available = false;

    state_lock();
    if (g_moonraker_exclude_state &&
        g_moonraker_exclude_state->available) {
        for (size_t i = 0;
             i < g_moonraker_exclude_state->object_count;
             ++i) {
            if (!g_moonraker_exclude_state->objects[i].excluded) {
                available = true;
                break;
            }
        }
    }
    state_unlock();

    return available;
}


void moonraker_state_reset(void)
{
    state_lock();
    memset(&g_moonraker_state, 0, sizeof(g_moonraker_state));
    if (g_moonraker_exclude_state) {
        memset(g_moonraker_exclude_state, 0,
               sizeof(*g_moonraker_exclude_state));
    }
    if (g_moonraker_filament_state) {
        memset(
            g_moonraker_filament_state,
            0,
            sizeof(*g_moonraker_filament_state));
    }

    g_moonraker_state.chamber_temp = -999.0;
    g_moonraker_state.air_temp = -999.0;
    g_moonraker_state.humidity = -999.0;
    g_moonraker_state.heater_target = -999.0;
    g_moonraker_state.drybox_fan_speed = -999.0;
    g_moonraker_state.part_fan_speed = -1.0;
    g_moonraker_state.nozzle_temp = -999.0;
    g_moonraker_state.nozzle_target = -999.0;

    g_moonraker_state.hotend_count = 1;
    mr_safe_copy(
        g_moonraker_state.active_hotend,
        sizeof(g_moonraker_state.active_hotend),
        "extruder");
    mr_safe_copy(
        g_moonraker_state.hotends[0].object_name,
        sizeof(g_moonraker_state.hotends[0].object_name),
        "extruder");
    g_moonraker_state.hotends[0].temperature = -999.0;
    g_moonraker_state.hotends[0].target = -999.0;
    g_moonraker_state.hotends[0].active = true;

    g_moonraker_state.bed_temp = -999.0;
    g_moonraker_state.bed_target = -999.0;
    g_moonraker_state.progress = -1.0;
    g_moonraker_state.current_layer = -1;
    g_moonraker_state.total_layer = -1;

    g_moonraker_state.toolhead_position_valid = false;
    g_moonraker_state.toolhead_x = 0.0;
    g_moonraker_state.toolhead_y = 0.0;
    g_moonraker_state.toolhead_z = 0.0;
    g_moonraker_state.homed_axes[0] = '\0';
    g_moonraker_state.z_offset = 0.0;

    mr_safe_copy(g_moonraker_state.printer_state, sizeof(g_moonraker_state.printer_state), "--");
    mr_safe_copy(g_moonraker_state.printer_file, sizeof(g_moonraker_state.printer_file), "No file");
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


void moonraker_state_configure_capabilities(
    const moonraker_capabilities_t *capabilities)
{
    if (!capabilities) {
        return;
    }

    state_lock();
    g_moonraker_state.capabilities =
        *capabilities;
    state_unlock();
}


void moonraker_filament_state_configure(
    const char names[][MOONRAKER_FILAMENT_SENSOR_NAME_MAX],
    size_t count,
    size_t total_count)
{
    if (count > MOONRAKER_MAX_FILAMENT_SENSORS) {
        count = MOONRAKER_MAX_FILAMENT_SENSORS;
    }

    state_lock();

    if (g_moonraker_filament_state) {
        memset(
            g_moonraker_filament_state,
            0,
            sizeof(*g_moonraker_filament_state));

        g_moonraker_filament_state->discovered = true;
        g_moonraker_filament_state->total_count =
            total_count;
        g_moonraker_filament_state->sensor_count =
            count;
        g_moonraker_filament_state->truncated =
            total_count > count;

        for (size_t i = 0; i < count; ++i) {
            moonraker_filament_sensor_t *sensor =
                &g_moonraker_filament_state->sensors[i];

            mr_safe_copy(
                sensor->object_name,
                sizeof(sensor->object_name),
                names ? names[i] : "");

            /*
             * Klipper normally publishes enabled in the first response.
             * Treat a missing field as enabled until told otherwise.
             */
            sensor->enabled = true;
        }
    }

    state_unlock();
}


moonraker_filament_status_t moonraker_filament_state_status(
    const moonraker_filament_state_t *state,
    size_t *present_out,
    size_t *enabled_out)
{
    if (present_out) *present_out = 0;
    if (enabled_out) *enabled_out = 0;

    if (!state || !state->discovered) {
        return MOONRAKER_FILAMENT_UNKNOWN;
    }

    if (state->total_count == 0) {
        return MOONRAKER_FILAMENT_ABSENT;
    }

    size_t present = 0;
    size_t enabled = 0;
    size_t known = 0;
    bool runout = false;

    for (size_t i = 0; i < state->sensor_count; ++i) {
        const moonraker_filament_sensor_t *sensor =
            &state->sensors[i];

        if (!sensor->enabled) {
            continue;
        }

        ++enabled;

        if (!sensor->status_known) {
            continue;
        }

        ++known;

        if (sensor->filament_detected) {
            ++present;
        } else {
            runout = true;
        }
    }

    if (present_out) *present_out = present;
    if (enabled_out) *enabled_out = enabled;

    if (runout) {
        return MOONRAKER_FILAMENT_RUNOUT;
    }

    if (enabled == 0 && state->sensor_count > 0) {
        return MOONRAKER_FILAMENT_DISABLED;
    }

    if (state->truncated || known < enabled) {
        return MOONRAKER_FILAMENT_CHECKING;
    }

    return MOONRAKER_FILAMENT_READY;
}


void moonraker_state_configure_hotends(
    const char names[][MOONRAKER_HOTEND_NAME_MAX],
    size_t count)
{
    if (!names || count == 0) {
        return;
    }

    if (count > MOONRAKER_MAX_HOTENDS) {
        count = MOONRAKER_MAX_HOTENDS;
    }

    state_lock();

    memset(
        g_moonraker_state.hotends,
        0,
        sizeof(g_moonraker_state.hotends));

    g_moonraker_state.hotend_count = count;

    for (size_t i = 0; i < count; ++i) {
        mr_safe_copy(
            g_moonraker_state.hotends[i].object_name,
            sizeof(g_moonraker_state.hotends[i].object_name),
            names[i]);

        g_moonraker_state.hotends[i].temperature = -999.0;
        g_moonraker_state.hotends[i].target = -999.0;
    }

    bool active_found = false;

    for (size_t i = 0; i < count; ++i) {
        bool active =
            strcmp(
                g_moonraker_state.hotends[i].object_name,
                g_moonraker_state.active_hotend) == 0;

        g_moonraker_state.hotends[i].active = active;
        active_found = active_found || active;
    }

    if (!active_found) {
        mr_safe_copy(
            g_moonraker_state.active_hotend,
            sizeof(g_moonraker_state.active_hotend),
            g_moonraker_state.hotends[0].object_name);

        g_moonraker_state.hotends[0].active = true;
    }

    state_unlock();
}


void moonraker_state_update_from_legacy(
    double chamber_temp,
    double air_temp,
    double humidity,
    double heater_target,
    bool heater_on,
    double drybox_fan_speed,
    double part_fan_speed,
    double speed_factor,
    double flow_factor,
    double live_velocity,
    double live_flow,
    double nozzle_temp,
    double nozzle_target,
    double bed_temp,
    double bed_target,
    double progress,
    double print_duration,
    int current_layer,
    int total_layer,
    bool live_data_ok,
    bool moonraker_ok,
    const char *printer_state,
    const char *printer_file
)
{
    state_lock();
    g_moonraker_state.chamber_temp = chamber_temp;
    g_moonraker_state.air_temp = air_temp;
    g_moonraker_state.humidity = humidity;
    g_moonraker_state.heater_target = heater_target;
    g_moonraker_state.heater_on = heater_on;
    g_moonraker_state.drybox_fan_speed = drybox_fan_speed;
    g_moonraker_state.part_fan_speed = part_fan_speed;
    g_moonraker_state.speed_factor = speed_factor;
    g_moonraker_state.flow_factor = flow_factor;
    g_moonraker_state.live_velocity = live_velocity;
    g_moonraker_state.live_flow = live_flow;
    g_moonraker_state.nozzle_temp = nozzle_temp;
    g_moonraker_state.nozzle_target = nozzle_target;

    /*
     * The HTTP fallback currently queries the conventional primary
     * extruder. Preserve that result in the capability-aware state.
     */
    if (g_moonraker_state.hotend_count > 0) {
        g_moonraker_state.hotends[0].temperature = nozzle_temp;
        g_moonraker_state.hotends[0].target = nozzle_target;
    }

    g_moonraker_state.bed_temp = bed_temp;
    g_moonraker_state.bed_target = bed_target;
    g_moonraker_state.progress = progress;
    g_moonraker_state.print_duration = print_duration;
    g_moonraker_state.current_layer = current_layer;
    g_moonraker_state.total_layer = total_layer;
    g_moonraker_state.live_data_ok = live_data_ok;
    g_moonraker_state.moonraker_ok = moonraker_ok;

    mr_safe_copy(g_moonraker_state.printer_state, sizeof(g_moonraker_state.printer_state), printer_state);
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


static bool json_xy_array(
    cJSON *array,
    moonraker_exclude_point_t *out)
{
    if (!cJSON_IsArray(array) || !out ||
        cJSON_GetArraySize(array) < 2) {
        return false;
    }

    cJSON *x = cJSON_GetArrayItem(array, 0);
    cJSON *y = cJSON_GetArrayItem(array, 1);
    if (!cJSON_IsNumber(x) || !cJSON_IsNumber(y)) {
        return false;
    }

    out->x = x->valuedouble;
    out->y = y->valuedouble;
    return true;
}


static moonraker_exclude_object_t *find_exclude_object_locked(
    const char *name)
{
    if (!g_moonraker_exclude_state || !name || !name[0]) {
        return NULL;
    }

    for (size_t i = 0;
         i < g_moonraker_exclude_state->object_count;
         ++i) {
        moonraker_exclude_object_t *object =
            &g_moonraker_exclude_state->objects[i];

        if (strcmp(name, object->name) == 0) {
            return object;
        }
    }

    return NULL;
}


static void refresh_exclude_flags_locked(void)
{
    if (!g_moonraker_exclude_state) return;

    for (size_t i = 0;
         i < g_moonraker_exclude_state->object_count;
         ++i) {
        moonraker_exclude_object_t *object =
            &g_moonraker_exclude_state->objects[i];

        object->current =
            g_moonraker_exclude_state->current_object[0] &&
            strcmp(object->name,
                   g_moonraker_exclude_state->current_object) == 0;
    }
}


static void merge_exclude_object_locked(cJSON *exclude_object)
{
    if (!g_moonraker_exclude_state) return;

    g_moonraker_exclude_state->available = true;

    cJSON *objects = cJSON_GetObjectItemCaseSensitive(
        exclude_object, "objects");

    if (cJSON_IsArray(objects)) {
        memset(g_moonraker_exclude_state->objects, 0,
               sizeof(g_moonraker_exclude_state->objects));
        g_moonraker_exclude_state->object_count = 0;
        g_moonraker_exclude_state->truncated = false;
        g_moonraker_exclude_state->current_object[0] = '\0';

        int count = cJSON_GetArraySize(objects);
        if (count > MOONRAKER_EXCLUDE_MAX_OBJECTS) {
            g_moonraker_exclude_state->truncated = true;
            count = MOONRAKER_EXCLUDE_MAX_OBJECTS;
        }

        for (int i = 0; i < count; ++i) {
            cJSON *definition = cJSON_GetArrayItem(objects, i);
            cJSON *name = cJSON_IsObject(definition)
                ? cJSON_GetObjectItemCaseSensitive(definition, "name")
                : NULL;

            if (!cJSON_IsString(name) || !name->valuestring ||
                !name->valuestring[0]) {
                continue;
            }

            moonraker_exclude_object_t *destination =
                &g_moonraker_exclude_state->objects
                    [g_moonraker_exclude_state->object_count++];

            mr_safe_copy(destination->name,
                         sizeof(destination->name),
                         name->valuestring);

            cJSON *center = cJSON_GetObjectItemCaseSensitive(
                definition, "center");
            destination->has_center =
                json_xy_array(center, &destination->center);

            cJSON *polygon = cJSON_GetObjectItemCaseSensitive(
                definition, "polygon");
            if (cJSON_IsArray(polygon)) {
                int point_count = cJSON_GetArraySize(polygon);
                if (point_count > MOONRAKER_EXCLUDE_MAX_POLYGON_POINTS) {
                    point_count = MOONRAKER_EXCLUDE_MAX_POLYGON_POINTS;
                    g_moonraker_exclude_state->truncated = true;
                }

                for (int point_index = 0;
                     point_index < point_count;
                     ++point_index) {
                    cJSON *point = cJSON_GetArrayItem(
                        polygon, point_index);
                    if (json_xy_array(
                            point,
                            &destination->polygon
                                [destination->polygon_count])) {
                        ++destination->polygon_count;
                    }
                }
            }
        }
    }

    cJSON *excluded = cJSON_GetObjectItemCaseSensitive(
        exclude_object, "excluded_objects");

    if (cJSON_IsArray(excluded)) {
        for (size_t i = 0;
             i < g_moonraker_exclude_state->object_count;
             ++i) {
            g_moonraker_exclude_state->objects[i].excluded = false;
        }

        int count = cJSON_GetArraySize(excluded);
        if (count > MOONRAKER_EXCLUDE_MAX_OBJECTS) {
            g_moonraker_exclude_state->truncated = true;
            count = MOONRAKER_EXCLUDE_MAX_OBJECTS;
        }

        for (int i = 0; i < count; ++i) {
            cJSON *name = cJSON_GetArrayItem(excluded, i);
            if (!cJSON_IsString(name) || !name->valuestring ||
                !name->valuestring[0]) {
                continue;
            }

            moonraker_exclude_object_t *object =
                find_exclude_object_locked(name->valuestring);
            if (object) {
                object->excluded = true;
            }
        }
    }

    cJSON *current = cJSON_GetObjectItemCaseSensitive(
        exclude_object, "current_object");

    if (cJSON_IsString(current) && current->valuestring) {
        mr_safe_copy(g_moonraker_exclude_state->current_object,
                     sizeof(g_moonraker_exclude_state->current_object),
                     current->valuestring);
    } else if (cJSON_IsNull(current)) {
        g_moonraker_exclude_state->current_object[0] = '\0';
    }

    refresh_exclude_flags_locked();
}


moonraker_websocket_message_t moonraker_state_merge_websocket_json(
    const char *json,
    size_t length)
{
    if (!json || length == 0) {
        return MOONRAKER_WEBSOCKET_MESSAGE_IGNORED;
    }

    cJSON *root = cJSON_ParseWithLength(json, length);
    if (!root) return MOONRAKER_WEBSOCKET_MESSAGE_IGNORED;

    cJSON *method = cJSON_GetObjectItemCaseSensitive(root, "method");
    const char *method_name =
        cJSON_IsString(method) ? method->valuestring : NULL;

    if (method_name &&
        strcmp(method_name, "notify_filelist_changed") == 0) {
        cJSON_Delete(root);
        return MOONRAKER_WEBSOCKET_MESSAGE_FILELIST_CHANGED;
    }

    moonraker_websocket_message_t lifecycle =
        MOONRAKER_WEBSOCKET_MESSAGE_IGNORED;

    if (method_name && strcmp(method_name, "notify_klippy_ready") == 0) {
        lifecycle = MOONRAKER_WEBSOCKET_MESSAGE_KLIPPY_READY;
    } else if (method_name &&
               strcmp(method_name, "notify_klippy_shutdown") == 0) {
        lifecycle = MOONRAKER_WEBSOCKET_MESSAGE_KLIPPY_SHUTDOWN;
    } else if (method_name &&
               strcmp(method_name, "notify_klippy_disconnected") == 0) {
        lifecycle = MOONRAKER_WEBSOCKET_MESSAGE_KLIPPY_DISCONNECTED;
    }

    if (lifecycle != MOONRAKER_WEBSOCKET_MESSAGE_IGNORED) {
        state_lock();

        if (g_moonraker_exclude_state) {
            memset(g_moonraker_exclude_state, 0,
                   sizeof(*g_moonraker_exclude_state));
        }

        if (lifecycle == MOONRAKER_WEBSOCKET_MESSAGE_KLIPPY_READY) {
            g_moonraker_state.live_data_ok = false;
            g_moonraker_state.moonraker_ok = true;
            mr_safe_copy(
                g_moonraker_state.printer_state,
                sizeof(g_moonraker_state.printer_state),
                "--");
        } else if (lifecycle ==
                   MOONRAKER_WEBSOCKET_MESSAGE_KLIPPY_SHUTDOWN) {
            g_moonraker_state.live_data_ok = false;
            g_moonraker_state.moonraker_ok = true;
            mr_safe_copy(
                g_moonraker_state.printer_state,
                sizeof(g_moonraker_state.printer_state),
                "error");
        } else {
            g_moonraker_state.live_data_ok = false;
            g_moonraker_state.moonraker_ok = false;
            mr_safe_copy(
                g_moonraker_state.printer_state,
                sizeof(g_moonraker_state.printer_state),
                "--");
        }

        state_unlock();
        cJSON_Delete(root);
        return lifecycle;
    }

    cJSON *status = json_status_object(root);
    if (!cJSON_IsObject(status)) {
        cJSON_Delete(root);
        return MOONRAKER_WEBSOCKET_MESSAGE_IGNORED;
    }

    int updates = 0;
    double value = 0.0;

    state_lock();

    cJSON *exclude_object = cJSON_GetObjectItemCaseSensitive(
        status, "exclude_object");
    if (cJSON_IsObject(exclude_object)) {
        merge_exclude_object_locked(exclude_object);
        ++updates;
    }

    cJSON *toolhead = cJSON_GetObjectItemCaseSensitive(
        status, "toolhead");

    cJSON *active_extruder = cJSON_IsObject(toolhead)
        ? cJSON_GetObjectItemCaseSensitive(toolhead, "extruder")
        : NULL;

    if (cJSON_IsString(active_extruder) &&
        active_extruder->valuestring) {
        mr_safe_copy(
            g_moonraker_state.active_hotend,
            sizeof(g_moonraker_state.active_hotend),
            active_extruder->valuestring);
        ++updates;
    }

    if (cJSON_IsObject(toolhead)) {
        cJSON *position = cJSON_GetObjectItemCaseSensitive(
            toolhead,
            "position");

        if (cJSON_IsArray(position) &&
            cJSON_GetArraySize(position) >= 3) {
            cJSON *x = cJSON_GetArrayItem(position, 0);
            cJSON *y = cJSON_GetArrayItem(position, 1);
            cJSON *z = cJSON_GetArrayItem(position, 2);

            if (cJSON_IsNumber(x) &&
                cJSON_IsNumber(y) &&
                cJSON_IsNumber(z)) {
                g_moonraker_state.toolhead_x = x->valuedouble;
                g_moonraker_state.toolhead_y = y->valuedouble;
                g_moonraker_state.toolhead_z = z->valuedouble;
                g_moonraker_state.toolhead_position_valid = true;
                ++updates;
            }
        }

        cJSON *homed_axes = cJSON_GetObjectItemCaseSensitive(
            toolhead,
            "homed_axes");

        if (cJSON_IsString(homed_axes) &&
            homed_axes->valuestring) {
            mr_safe_copy(
                g_moonraker_state.homed_axes,
                sizeof(g_moonraker_state.homed_axes),
                homed_axes->valuestring);
            ++updates;
        }
    }

    if (cJSON_IsObject(toolhead) && g_moonraker_exclude_state) {
        moonraker_exclude_point_t minimum;
        moonraker_exclude_point_t maximum;
        cJSON *axis_minimum = cJSON_GetObjectItemCaseSensitive(
            toolhead, "axis_minimum");
        cJSON *axis_maximum = cJSON_GetObjectItemCaseSensitive(
            toolhead, "axis_maximum");

        if (json_xy_array(axis_minimum, &minimum) &&
            json_xy_array(axis_maximum, &maximum) &&
            maximum.x > minimum.x && maximum.y > minimum.y) {
            g_moonraker_exclude_state->bed_min_x = minimum.x;
            g_moonraker_exclude_state->bed_min_y = minimum.y;
            g_moonraker_exclude_state->bed_max_x = maximum.x;
            g_moonraker_exclude_state->bed_max_y = maximum.y;
            g_moonraker_exclude_state->bed_bounds_valid = true;
            ++updates;
        }
    }

    cJSON *gcode_move = cJSON_GetObjectItemCaseSensitive(
        status,
        "gcode_move");

    if (cJSON_IsObject(gcode_move)) {
        cJSON *homing_origin = cJSON_GetObjectItemCaseSensitive(
            gcode_move,
            "homing_origin");

        if (cJSON_IsArray(homing_origin) &&
            cJSON_GetArraySize(homing_origin) >= 3) {
            cJSON *z_origin = cJSON_GetArrayItem(homing_origin, 2);

            if (cJSON_IsNumber(z_origin)) {
                g_moonraker_state.z_offset = z_origin->valuedouble;
                ++updates;
            }
        }
    }

#define MERGE_NUMBER(object_name, field_name, destination, scale) do {     cJSON *object = cJSON_GetObjectItemCaseSensitive(status, object_name);     if (json_number(object, field_name, &value)) {         g_moonraker_state.destination = value * (scale);         ++updates;     } } while (0)

    MERGE_NUMBER("temperature_sensor drybox_center", "temperature", chamber_temp, 1.0);
    MERGE_NUMBER("sht3x drybox_env", "temperature", air_temp, 1.0);
    MERGE_NUMBER("sht3x drybox_env", "humidity", humidity, 1.0);
    MERGE_NUMBER("heater_generic drybox_heater", "target", heater_target, 1.0);
    MERGE_NUMBER("fan_generic drybox_fan", "speed", drybox_fan_speed, 100.0);
    MERGE_NUMBER("fan", "speed", part_fan_speed, 100.0);
    MERGE_NUMBER("gcode_move", "speed_factor", speed_factor, 100.0);
    MERGE_NUMBER("gcode_move", "extrude_factor", flow_factor, 100.0);
    MERGE_NUMBER("motion_report", "live_velocity", live_velocity, 1.0);

    /*
     * Klipper reports live_extruder_velocity as linear filament speed
     * in mm/s. Convert it to volumetric flow so the WebSocket path
     * matches the existing HTTP fallback and the UI mm3/s label.
     *
     * Standard 1.75 mm filament:
     *     area = pi * (1.75 / 2)^2 = 2.405281875 mm2
     */
    cJSON *motion_report = cJSON_GetObjectItemCaseSensitive(
        status,
        "motion_report");

    if (json_number(
            motion_report,
            "live_extruder_velocity",
            &value)) {
        static const double filament_area_mm2 = 2.405281875;

        g_moonraker_state.live_flow =
            fabs(value) * filament_area_mm2;

        if (g_moonraker_state.live_flow < 0.01) {
            g_moonraker_state.live_flow = 0.0;
        }

        ++updates;
    }

    MERGE_NUMBER("heater_bed", "temperature", bed_temp, 1.0);
    MERGE_NUMBER("heater_bed", "target", bed_target, 1.0);
    MERGE_NUMBER("display_status", "progress", progress, 1.0);
    MERGE_NUMBER("print_stats", "print_duration", print_duration, 1.0);

#undef MERGE_NUMBER

    for (size_t i = 0;
         i < g_moonraker_state.hotend_count;
         ++i) {
        moonraker_hotend_t *hotend =
            &g_moonraker_state.hotends[i];

        cJSON *object = cJSON_GetObjectItemCaseSensitive(
            status,
            hotend->object_name);

        bool active =
            strcmp(
                hotend->object_name,
                g_moonraker_state.active_hotend) == 0;

        hotend->active = active;

        if (json_number(object, "temperature", &value)) {
            hotend->temperature = value;
            ++updates;
        }

        if (json_number(object, "target", &value)) {
            hotend->target = value;
            ++updates;
        }

        if (active) {
            g_moonraker_state.nozzle_temp =
                hotend->temperature;
            g_moonraker_state.nozzle_target =
                hotend->target;
        }
    }

    if (g_moonraker_filament_state) {
        for (size_t i = 0;
             i < g_moonraker_filament_state->sensor_count;
             ++i) {
            moonraker_filament_sensor_t *sensor =
                &g_moonraker_filament_state->sensors[i];

            cJSON *object =
                cJSON_GetObjectItemCaseSensitive(
                    status,
                    sensor->object_name);

            if (!cJSON_IsObject(object)) {
                continue;
            }

            cJSON *enabled =
                cJSON_GetObjectItemCaseSensitive(
                    object,
                    "enabled");

            if (cJSON_IsBool(enabled)) {
                sensor->enabled =
                    cJSON_IsTrue(enabled);
                ++updates;
            }

            cJSON *detected =
                cJSON_GetObjectItemCaseSensitive(
                    object,
                    "filament_detected");

            if (cJSON_IsBool(detected)) {
                sensor->filament_detected =
                    cJSON_IsTrue(detected);
                sensor->status_known = true;
                ++updates;
            }
        }
    }

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
    return updates > 0
        ? MOONRAKER_WEBSOCKET_MESSAGE_STATUS
        : MOONRAKER_WEBSOCKET_MESSAGE_IGNORED;
}


const char *json_find_string(const char *json, const char *key, char *out, size_t out_len)
{
    if (!json || !key || !out || out_len == 0) return NULL;

    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);

    const char *p = strstr(json, pattern);
    if (!p) return NULL;

    p = strchr(p, ':');
    if (!p) return NULL;
    p++;

    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    if (*p != '"') return NULL;
    p++;

    size_t i = 0;
    while (*p && *p != '"' && i + 1 < out_len) {
        out[i++] = *p++;
    }
    out[i] = 0;

    return out;
}

bool json_find_number_after(const char *json, const char *anchor, const char *key, double *out)
{
    if (!json || !anchor || !key || !out) return false;

    const char *a = strstr(json, anchor);
    if (!a) return false;

    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);

    const char *p = strstr(a, pattern);
    if (!p) return false;

    p = strchr(p, ':');
    if (!p) return false;
    p++;

    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;

    char *end = NULL;
    double val = strtod(p, &end);
    if (end == p) return false;

    *out = val;
    return true;
}

bool json_find_number_after_key(const char *json, const char *key, double *out)
{
    if (!json || !key || !out) return false;

    const char *p = strstr(json, key);
    if (!p) return false;

    p = strchr(p, ':');
    if (!p) return false;
    p++;

    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;

    char *end = NULL;
    double v = strtod(p, &end);
    if (end == p) return false;

    *out = v;
    return true;
}

bool json_find_best_thumbnail_path(const char *json, char *out, size_t out_sz)
{
    if (!json || !out || out_sz == 0) return false;
    out[0] = 0;

    const char *best = strstr(json, "400x300");
    if (!best) best = strstr(json, "relative_path");
    if (!best) return false;

    const char *rp = best;
    while (rp > json && strncmp(rp, "relative_path", 13) != 0) {
        rp--;
    }

    if (strncmp(rp, "relative_path", 13) != 0) {
        rp = strstr(best, "relative_path");
        if (!rp) return false;
    }

    const char *colon = strchr(rp, ':');
    if (!colon) return false;

    const char *q1 = strchr(colon, '"');
    if (!q1) return false;
    q1++;

    const char *q2 = strchr(q1, '"');
    if (!q2) return false;

    size_t n = q2 - q1;
    if (n >= out_sz) n = out_sz - 1;

    memcpy(out, q1, n);
    out[n] = 0;
    return out[0] != 0;
}

esp_err_t moonraker_http_event_handler(esp_http_client_event_t *evt)
{
    http_capture_t *cap = (http_capture_t *)evt->user_data;

    if (evt->event_id == HTTP_EVENT_ON_DATA && cap && evt->data && evt->data_len > 0) {
        int room = cap->cap - cap->len - 1;
        int copy = evt->data_len < room ? evt->data_len : room;
        if (copy > 0) {
            memcpy(cap->buf + cap->len, evt->data, copy);
            cap->len += copy;
            cap->buf[cap->len] = 0;
        }
    }

    return ESP_OK;
}

static bool moonraker_http_get_raw(
    const char *host,
    int port,
    const char *api_key,
    const char *path,
    int timeout_ms,
    uint8_t *buffer,
    size_t buffer_size,
    size_t *captured_size,
    int *http_code,
    esp_err_t *err_out)
{
    if (captured_size) {
        *captured_size = 0;
    }

    if (http_code) {
        *http_code = 0;
    }

    if (err_out) {
        *err_out = ESP_FAIL;
    }

    if (!host ||
        !host[0] ||
        port <= 0 ||
        !path ||
        path[0] != '/' ||
        !buffer ||
        buffer_size < 2 ||
        buffer_size > INT_MAX) {
        if (err_out) {
            *err_out = ESP_ERR_INVALID_ARG;
        }

        return false;
    }

    buffer[0] = 0;

    char url[512];

    int url_len = snprintf(
        url,
        sizeof(url),
        "http://%s:%d%s",
        host,
        port,
        path);

    if (url_len < 0 || (size_t)url_len >= sizeof(url)) {
        if (err_out) {
            *err_out = ESP_ERR_INVALID_SIZE;
        }

        return false;
    }

    http_capture_t cap = {
        .buf = (char *)buffer,
        .len = 0,
        .cap = (int)buffer_size,
    };

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = timeout_ms,
        .event_handler = moonraker_http_event_handler,
        .user_data = &cap,
    };

    esp_http_client_handle_t client =
        esp_http_client_init(&config);

    if (!client) {
        if (err_out) {
            *err_out = ESP_FAIL;
        }

        return false;
    }

    if (api_key && api_key[0]) {
        esp_http_client_set_header(
            client,
            "X-Api-Key",
            api_key);
    }

    esp_err_t err =
        esp_http_client_perform(client);

    int code =
        esp_http_client_get_status_code(client);

    esp_http_client_cleanup(client);

    if (captured_size) {
        *captured_size = (size_t)cap.len;
    }

    if (http_code) {
        *http_code = code;
    }

    if (err_out) {
        *err_out = err;
    }

    return err == ESP_OK && code == 200;
}


static bool moonraker_http_get_text(
    const char *host,
    int port,
    const char *api_key,
    const char *path,
    int timeout_ms,
    char *body,
    size_t body_sz,
    int *http_code,
    esp_err_t *err_out)
{
    size_t captured_size = 0;

    return moonraker_http_get_raw(
        host,
        port,
        api_key,
        path,
        timeout_ms,
        (uint8_t *)body,
        body_sz,
        &captured_size,
        http_code,
        err_out);
}




bool moonraker_test_connection(const char *host,
                                int port,
                                int *http_code,
                                esp_err_t *err_out)
{
    char body[256];

    return moonraker_http_get_text(
        host,
        port,
        NULL,
        "/server/info",
        1200,
        body,
        sizeof(body),
        http_code,
        err_out);
}



bool moonraker_fetch_file_list(const char *host,
                               int port,
                               const char *api_key,
                               char *body,
                               size_t body_sz,
                               int *http_code,
                               esp_err_t *err_out)
{
    return moonraker_http_get_text(
        host,
        port,
        api_key,
        "/server/files/list?root=gcodes",
        2500,
        body,
        body_sz,
        http_code,
        err_out);
}



bool moonraker_fetch_print_stats(const char *host,
                                 int port,
                                 const char *api_key,
                                 char *body,
                                 size_t body_sz,
                                 int *http_code,
                                 esp_err_t *err_out)
{
    return moonraker_http_get_text(
        host,
        port,
        api_key,
        "/printer/objects/query?print_stats",
        1200,
        body,
        body_sz,
        http_code,
        err_out);
}



bool moonraker_fetch_thumbnail_encoded(const char *host,
                                       int port,
                                       const char *encoded_thumb_path,
                                       uint8_t **out_buf,
                                       size_t *out_len)
{
    if (!out_buf || !out_len) {
        return false;
    }

    *out_buf = NULL;
    *out_len = 0;

    if (!host ||
        !host[0] ||
        port <= 0 ||
        !encoded_thumb_path ||
        !encoded_thumb_path[0]) {
        return false;
    }

    const size_t max_len = 64 * 1024;

    uint8_t *buf = heap_caps_malloc(
        max_len,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (!buf) {
        buf = heap_caps_malloc(
            max_len,
            MALLOC_CAP_8BIT);
    }

    if (!buf) {
        return false;
    }

    char path[384];

    int path_len = snprintf(
        path,
        sizeof(path),
        "/server/files/gcodes/%s",
        encoded_thumb_path);

    if (path_len < 0 ||
        (size_t)path_len >= sizeof(path)) {
        heap_caps_free(buf);
        return false;
    }

    size_t captured_size = 0;
    int http_code = 0;
    esp_err_t err = ESP_FAIL;

    bool ok = moonraker_http_get_raw(
        host,
        port,
        NULL,
        path,
        1500,
        buf,
        max_len,
        &captured_size,
        &http_code,
        &err);

    if (!ok || captured_size < 16) {
        heap_caps_free(buf);
        return false;
    }

    *out_buf = buf;
    *out_len = captured_size;

    return true;
}



bool moonraker_fetch_file_metadata(const char *host,
                                   int port,
                                   const char *api_key,
                                   const char *encoded_filename,
                                   char *body,
                                   size_t body_sz,
                                   int *http_code,
                                   esp_err_t *err_out)
{
    if (http_code) {
        *http_code = 0;
    }

    if (err_out) {
        *err_out = ESP_FAIL;
    }

    if (!encoded_filename || !encoded_filename[0]) {
        if (err_out) {
            *err_out = ESP_ERR_INVALID_ARG;
        }

        return false;
    }

    char path[320];

    int path_len = snprintf(
        path,
        sizeof(path),
        "/server/files/metadata?filename=%s",
        encoded_filename);

    if (path_len < 0 || (size_t)path_len >= sizeof(path)) {
        if (err_out) {
            *err_out = ESP_ERR_INVALID_SIZE;
        }

        return false;
    }

    return moonraker_http_get_text(
        host,
        port,
        api_key,
        path,
        2500,
        body,
        body_sz,
        http_code,
        err_out);
}



bool moonraker_send_gcode_script(const char *host,
                                  int port,
                                  const char *api_key,
                                  const char *cmd,
                                  int *http_code,
                                  esp_err_t *err_out)
{
    if (http_code) {
        *http_code = 0;
    }

    if (err_out) {
        *err_out = ESP_FAIL;
    }

    if (!host || !host[0] || port <= 0 || !cmd || !cmd[0]) {
        return false;
    }

    char url[256];
    int url_len = snprintf(url,
                           sizeof(url),
                           "http://%s:%d/printer/gcode/script",
                           host,
                           port);

    if (url_len < 0 || (size_t)url_len >= sizeof(url)) {
        return false;
    }

    char body[160];
    int body_len = snprintf(body,
                            sizeof(body),
                            "{\"script\":\"%s\"}",
                            cmd);

    if (body_len < 0 || (size_t)body_len >= sizeof(body)) {
        return false;
    }

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 3000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        return false;
    }

    esp_http_client_set_header(client,
                               "Content-Type",
                               "application/json");

    if (api_key && api_key[0]) {
        esp_http_client_set_header(client,
                                   "X-Api-Key",
                                   api_key);
    }

    esp_http_client_set_post_field(client,
                                   body,
                                   body_len);

    esp_err_t err = esp_http_client_perform(client);
    int code = esp_http_client_get_status_code(client);

    esp_http_client_cleanup(client);

    if (http_code) {
        *http_code = code;
    }

    if (err_out) {
        *err_out = err;
    }

    return err == ESP_OK && code >= 200 && code < 300;
}

bool moonraker_start_print_file(const char *host,
                                int port,
                                const char *api_key,
                                const char *filename,
                                int *http_code,
                                esp_err_t *err_out)
{
    if (http_code) *http_code = 0;
    if (err_out) *err_out = ESP_FAIL;

    if (!host || !host[0] || port <= 0 || !filename || !filename[0]) {
        return false;
    }

    char url[160];
    snprintf(url, sizeof(url),
             "http://%s:%d/printer/print/start",
             host, port);

    char body[260];
    snprintf(body, sizeof(body), "{\"filename\":\"%s\"}", filename);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 5000,
        .event_handler = moonraker_http_event_handler,
        .user_data = NULL,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        if (err_out) *err_out = ESP_FAIL;
        return false;
    }

    esp_http_client_set_header(client, "Content-Type", "application/json");

    if (api_key && api_key[0]) {
        esp_http_client_set_header(client, "X-Api-Key", api_key);
    }

    esp_http_client_set_post_field(client, body, strlen(body));

    esp_err_t err = esp_http_client_perform(client);
    int code = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (http_code) *http_code = code;
    if (err_out) *err_out = err;

    return err == ESP_OK && code >= 200 && code < 300;
}
