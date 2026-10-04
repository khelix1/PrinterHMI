#include "motion_diagnostics_controller.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "moonraker_config_controller.h"

static const char TAG[] = "motion_diagnostics";
static motion_diagnostics_snapshot_t *s_store;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;

bool motion_diagnostics_controller_init(void)
{
    if (s_store) return true;
    s_store = heap_caps_calloc(1, sizeof(*s_store), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_store) s_store = heap_caps_calloc(1, sizeof(*s_store), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!s_store) { ESP_LOGE(TAG, "Unable to allocate motion diagnostics"); return false; }
    return true;
}

void motion_diagnostics_controller_reset(void)
{
    if (!s_store) return;
    portENTER_CRITICAL(&s_lock);
    memset(s_store, 0, sizeof(*s_store));
    portEXIT_CRITICAL(&s_lock);
}

static void owner_locked(uint32_t generation)
{
    if (s_store->owner_generation != generation) memset(s_store, 0, sizeof(*s_store));
    s_store->owner_generation = generation;
}

void motion_diagnostics_controller_update_objects(const struct cJSON *objects)
{
    if (!s_store) return;
    uint32_t generation = moonraker_config_generation();
    portENTER_CRITICAL(&s_lock);
    owner_locked(generation);
    s_store->driver_count = 0;
    s_store->drivers_truncated = false;
    memset(s_store->drivers, 0, sizeof(s_store->drivers));
    s_store->discovered = cJSON_IsArray(objects);
    const cJSON *entry;
    cJSON_ArrayForEach(entry, objects) {
        if (!cJSON_IsString(entry) || strncmp(entry->valuestring, "tmc", 3) ||
            !strchr(entry->valuestring, ' ')) continue;
        if (s_store->driver_count == MOTION_DRIVER_MAX) { s_store->drivers_truncated = true; continue; }
        motion_driver_snapshot_t *driver = &s_store->drivers[s_store->driver_count++];
        snprintf(driver->name, sizeof(driver->name), "%s", entry->valuestring);
    }
    portEXIT_CRITICAL(&s_lock);
}

static bool number(const cJSON *value, double *out)
{
    if (!cJSON_IsNumber(value) || !isfinite(value->valuedouble)) return false;
    *out = value->valuedouble;
    return true;
}

static bool parse_positive(const char *text, double *out)
{
    if (!text || !*text) return false;
    char *end;
    double value = strtod(text, &end);
    if (end == text) return false;
    while (isspace((unsigned char)*end)) ++end;
    if (*end || !isfinite(value) || value <= 0) return false;
    *out = value;
    return true;
}

void motion_diagnostics_controller_update_config(const struct cJSON *config)
{
    if (!s_store || !cJSON_IsObject(config)) return;
    uint32_t generation = moonraker_config_generation();
    portENTER_CRITICAL(&s_lock);
    owner_locked(generation);
    const cJSON *printer = cJSON_GetObjectItem(config, "printer");
    const cJSON *kinematics = cJSON_GetObjectItemCaseSensitive(printer, "kinematics");
    snprintf(s_store->kinematics, sizeof(s_store->kinematics), "%s",
             cJSON_IsString(kinematics) ? kinematics->valuestring : "unknown");
    const char *axes[] = {"stepper_x", "stepper_y", "stepper_z"};
    for (size_t i = 0; i < 3; ++i) {
        const cJSON *section = cJSON_GetObjectItem(config, axes[i]);
        const cJSON *value = cJSON_GetObjectItemCaseSensitive(section, "rotation_distance");
        double distance;
        bool valid = number(value, &distance) ||
            (cJSON_IsString(value) && parse_positive(value->valuestring, &distance));
        s_store->rotation_valid[i] = valid && distance > 0;
        s_store->rotation_distance[i] = s_store->rotation_valid[i] ? distance : 0;
    }
    portEXIT_CRITICAL(&s_lock);
}

static bool flag(const cJSON *status, const char *name)
{
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(status, name);
    return cJSON_IsTrue(value) || (cJSON_IsNumber(value) && value->valuedouble != 0);
}

static void update_driver(motion_driver_snapshot_t *driver, const cJSON *object)
{
    const char *fields[] = {"run_current", "hold_current", "temperature"};
    bool *valid[] = {&driver->run_valid, &driver->hold_valid, &driver->temperature_valid};
    double *values[] = {&driver->run_current, &driver->hold_current, &driver->temperature};
    for (size_t i = 0; i < 3; ++i) {
        const cJSON *entry = cJSON_GetObjectItemCaseSensitive(object, fields[i]);
        if (entry) *valid[i] = number(entry, values[i]) && (i == 2 || *values[i] >= 0);
    }
    const cJSON *status = cJSON_GetObjectItemCaseSensitive(object, "drv_status");
    if (!status) return; /* Partial updates retain fields not supplied. */
    driver->status_valid = cJSON_IsObject(status);
    driver->warning = driver->fault = false;
    driver->flags[0] = 0;
    if (!driver->status_valid) return;
    static const struct { const char *key, *text; bool fault; } flags[] = {
        {"otpw", "Overtemperature warning", false}, {"ot", "Overtemperature shutdown", true},
        {"s2ga", "Short to ground A", true}, {"s2gb", "Short to ground B", true},
        {"s2vsa", "Short to supply A", true}, {"s2vsb", "Short to supply B", true},
        {"drv_err", "Driver error", true}, {"uv_cp", "Charge pump undervoltage", true},
        {"ola", "Open load A", false}, {"olb", "Open load B", false},
        {"t120", "Temperature threshold 120C", false}, {"t143", "Temperature threshold 143C", false},
        {"t150", "Temperature threshold 150C", false}, {"t157", "Temperature threshold 157C", false}
    };
    for (size_t i = 0; i < sizeof(flags) / sizeof(flags[0]); ++i) {
        if (!flag(status, flags[i].key)) continue;
        if (flags[i].fault) driver->fault = true; else driver->warning = true;
        size_t used = strlen(driver->flags);
        if (used < sizeof(driver->flags) - 1)
            snprintf(driver->flags + used, sizeof(driver->flags) - used, "%s%s", used ? "; " : "", flags[i].text);
    }
}

bool motion_diagnostics_controller_merge_status(const struct cJSON *status)
{
    if (!s_store || !cJSON_IsObject(status)) return false;
    uint32_t generation = moonraker_config_generation();
    bool changed = false;
    portENTER_CRITICAL(&s_lock);
    owner_locked(generation);
    const cJSON *toolhead = cJSON_GetObjectItemCaseSensitive(status, "toolhead");
    const char *limits[] = {"max_velocity", "max_accel", "square_corner_velocity", "minimum_cruise_ratio"};
    for (size_t i = 0; i < 4; ++i) {
        const cJSON *entry = cJSON_GetObjectItemCaseSensitive(toolhead, limits[i]);
        if (entry) {
            bool valid = number(entry, &s_store->limits[i]);
            double value = s_store->limits[i];
            s_store->limit_valid[i] = valid &&
                (i < 2 ? value > 0 : i == 2 ? value >= 0 : value >= 0 && value <= 1);
            changed = true;
        }
    }
    for (size_t i = 0; i < s_store->driver_count; ++i) {
        motion_driver_snapshot_t *driver = &s_store->drivers[i];
        const cJSON *object = cJSON_GetObjectItemCaseSensitive(status, driver->name);
        if (cJSON_IsObject(object)) { update_driver(driver, object); changed = true; }
    }
    portEXIT_CRITICAL(&s_lock);
    return changed;
}

void motion_diagnostics_controller_snapshot(motion_diagnostics_snapshot_t *out)
{
    if (!out) return;
    memset(out, 0, sizeof(*out));
    if (!s_store) return;
    uint32_t generation = moonraker_config_generation();
    portENTER_CRITICAL(&s_lock);
    if (s_store->owner_generation == generation) *out = *s_store;
    portEXIT_CRITICAL(&s_lock);
}

bool motion_axis_distance_calculate(const char *pitch, const char *count,
    double *distance, const char **error)
{
    if (error) *error = "Enter pitch 0.000001-1000 mm and a whole count 1-10000.";
    double p, n;
    if (!distance || !parse_positive(pitch, &p) || !parse_positive(count, &n) ||
        p < 0.000001 || p > 1000 || n > 10000 || floor(n) != n) return false;
    double result = p * n;
    if (!isfinite(result) || result <= 0) return false;
    *distance = result;
    if (error) *error = NULL;
    return true;
}
