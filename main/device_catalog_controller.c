#include "device_catalog_controller.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

typedef struct {
    device_catalog_status_t status;
    device_descriptor_t devices[DEVICE_CATALOG_MAX_DEVICES];
} device_catalog_store_t;

static const char TAG[] = "device_catalog";
static device_catalog_store_t *s_store;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;


static bool starts_with(
    const char *text,
    const char *prefix)
{
    if (!text || !prefix) {
        return false;
    }

    return strncmp(text, prefix, strlen(prefix)) == 0;
}


static device_kind_t classify_object(
    const char *name)
{
    if (!name || !name[0]) {
        return DEVICE_KIND_OTHER;
    }

    if (strcmp(name, "heater_bed") == 0 ||
        starts_with(name, "extruder") ||
        starts_with(name, "heater_generic ") ||
        starts_with(name, "temperature_fan ")) {
        return DEVICE_KIND_THERMAL;
    }

    if (strcmp(name, "fan") == 0 ||
        starts_with(name, "fan_generic ") ||
        starts_with(name, "controller_fan ") ||
        starts_with(name, "heater_fan ")) {
        return DEVICE_KIND_AIR;
    }

    if (starts_with(name, "power ") ||
        starts_with(name, "power_device ")) {
        return DEVICE_KIND_POWER;
    }

    if (starts_with(name, "output_pin ") ||
        starts_with(name, "pwm_tool ") ||
        starts_with(name, "led ") ||
        starts_with(name, "neopixel ") ||
        starts_with(name, "dotstar ") ||
        starts_with(name, "servo ")) {
        return DEVICE_KIND_OUTPUT;
    }

    if (strcmp(name, "stepper_enable") == 0 ||
        strcmp(name, "toolhead") == 0 ||
        starts_with(name, "tmc") ||
        starts_with(name, "manual_stepper ") ||
        starts_with(name, "dual_carriage")) {
        return DEVICE_KIND_MOTION;
    }

    if (starts_with(name, "temperature_sensor ") ||
        starts_with(name, "filament_switch_sensor ") ||
        starts_with(name, "filament_motion_sensor ") ||
        starts_with(name, "probe") ||
        starts_with(name, "load_cell") ||
        starts_with(name, "adxl") ||
        starts_with(name, "lis2") ||
        starts_with(name, "mpu9250") ||
        starts_with(name, "bme280") ||
        starts_with(name, "bme680") ||
        starts_with(name, "htu21d") ||
        starts_with(name, "sht3x") ||
        starts_with(name, "aht10")) {
        return DEVICE_KIND_SENSOR;
    }

    return DEVICE_KIND_OTHER;
}


bool device_catalog_controller_subscription_fields(
    const char *object_name,
    const char **fields_out)
{
    if (fields_out) {
        *fields_out = NULL;
    }

    if (!object_name || !object_name[0] || !fields_out) {
        return false;
    }

    /*
     * These objects already belong to the proven core subscription. Keeping
     * them out here prevents duplicate JSON keys and preserves their existing
     * specialized state handling.
     */
    if (strcmp(object_name, "heater_bed") == 0 ||
        strcmp(object_name, "fan") == 0 ||
        strcmp(object_name, "toolhead") == 0 ||
        starts_with(object_name, "extruder") ||
        starts_with(object_name, "filament_switch_sensor ") ||
        starts_with(object_name, "filament_motion_sensor ") ||
        strcmp(object_name, "temperature_sensor drybox_center") == 0 ||
        strcmp(object_name, "sht3x drybox_env") == 0 ||
        strcmp(object_name, "heater_generic drybox_heater") == 0 ||
        strcmp(object_name, "fan_generic drybox_fan") == 0) {
        return false;
    }

    if (starts_with(object_name, "heater_generic ")) {
        *fields_out = "[\"temperature\",\"target\",\"power\"]";
        return true;
    }

    if (starts_with(object_name, "temperature_fan ")) {
        *fields_out =
            "[\"temperature\",\"target\",\"speed\",\"rpm\"]";
        return true;
    }

    if (starts_with(object_name, "fan_generic ") ||
        starts_with(object_name, "controller_fan ") ||
        starts_with(object_name, "heater_fan ")) {
        *fields_out = "[\"speed\",\"rpm\"]";
        return true;
    }

    if (starts_with(object_name, "temperature_sensor ")) {
        *fields_out = "[\"temperature\"]";
        return true;
    }

    if (starts_with(object_name, "bme280") ||
        starts_with(object_name, "bme680") ||
        starts_with(object_name, "htu21d") ||
        starts_with(object_name, "sht3x") ||
        starts_with(object_name, "aht10")) {
        *fields_out =
            "[\"temperature\",\"humidity\",\"pressure\",\"gas\"]";
        return true;
    }

    if (starts_with(object_name, "output_pin ") ||
        starts_with(object_name, "pwm_tool ") ||
        starts_with(object_name, "servo ")) {
        *fields_out = "[\"value\"]";
        return true;
    }

    if (starts_with(object_name, "led ") ||
        starts_with(object_name, "neopixel ") ||
        starts_with(object_name, "dotstar ")) {
        *fields_out = "[\"color_data\"]";
        return true;
    }

    return false;
}


bool device_catalog_controller_has_live_value_source(
    const char *object_name)
{
    if (!object_name || !object_name[0]) {
        return false;
    }

    /* Values for these objects already come from PrinterHMI's core state. */
    if (strcmp(object_name, "heater_bed") == 0 ||
        strcmp(object_name, "fan") == 0 ||
        strcmp(object_name, "toolhead") == 0 ||
        starts_with(object_name, "extruder") ||
        starts_with(object_name, "filament_switch_sensor ") ||
        starts_with(object_name, "filament_motion_sensor ") ||
        strcmp(object_name, "temperature_sensor drybox_center") == 0 ||
        strcmp(object_name, "sht3x drybox_env") == 0 ||
        strcmp(object_name, "heater_generic drybox_heater") == 0 ||
        strcmp(object_name, "fan_generic drybox_fan") == 0) {
        return true;
    }

    const char *fields = NULL;
    return device_catalog_controller_subscription_fields(
        object_name,
        &fields);
}


static bool kind_is_controllable(
    device_kind_t kind)
{
    switch (kind) {
    case DEVICE_KIND_THERMAL:
    case DEVICE_KIND_AIR:
    case DEVICE_KIND_POWER:
    case DEVICE_KIND_OUTPUT:
        return true;

    case DEVICE_KIND_SENSOR:
    case DEVICE_KIND_MOTION:
    case DEVICE_KIND_OTHER:
    case DEVICE_KIND_COUNT:
    default:
        return false;
    }
}


static const char *object_suffix(
    const char *object_name)
{
    const char *space = object_name
        ? strchr(object_name, ' ')
        : NULL;

    return space && space[1]
        ? space + 1
        : object_name;
}


static void make_display_name(
    const char *object_name,
    char *output,
    size_t output_size)
{
    if (!output || output_size == 0) {
        return;
    }

    output[0] = '\0';

    if (!object_name || !object_name[0]) {
        snprintf(output, output_size, "Unknown");
        return;
    }

    if (strcmp(object_name, "heater_bed") == 0) {
        snprintf(output, output_size, "Heated Bed");
        return;
    }

    if (strcmp(object_name, "fan") == 0) {
        snprintf(output, output_size, "Part Fan");
        return;
    }

    const char *source = object_suffix(object_name);
    size_t used = 0;
    bool capitalize = true;

    while (*source && used + 1 < output_size) {
        unsigned char value = (unsigned char)*source++;

        if (value == '_' || value == '-') {
            output[used++] = ' ';
            capitalize = true;
            continue;
        }

        output[used++] = capitalize
            ? (char)toupper(value)
            : (char)value;
        capitalize = value == ' ';
    }

    output[used] = '\0';
}


static int compare_devices(
    const void *left,
    const void *right)
{
    const device_descriptor_t *a = left;
    const device_descriptor_t *b = right;

    if (a->kind != b->kind) {
        return (int)a->kind - (int)b->kind;
    }

    return strcmp(a->display_name, b->display_name);
}


static void store_object_locked(
    const char *object_name,
    device_kind_t kind)
{
    ++s_store->status.kind_count[kind];

    if (s_store->status.stored_count >=
        DEVICE_CATALOG_MAX_DEVICES) {
        s_store->status.truncated = true;
        return;
    }

    device_descriptor_t *device =
        &s_store->devices[s_store->status.stored_count++];

    snprintf(
        device->object_name,
        sizeof(device->object_name),
        "%.*s",
        DEVICE_CATALOG_OBJECT_NAME_MAX - 1,
        object_name);

    make_display_name(
        object_name,
        device->display_name,
        sizeof(device->display_name));

    device->kind = kind;
    device->controllable = kind_is_controllable(kind);
}


static bool status_number(
    const cJSON *object,
    const char *field,
    double *output)
{
    const cJSON *item = cJSON_IsObject(object)
        ? cJSON_GetObjectItemCaseSensitive(object, field)
        : NULL;

    if (!cJSON_IsNumber(item) || !output) {
        return false;
    }

    *output = item->valuedouble;
    return true;
}


static double status_percent(double value)
{
    return value >= 0.0 && value <= 1.01
        ? value * 100.0
        : value;
}


static bool format_generic_status_value(
    const device_descriptor_t *device,
    const cJSON *object,
    char *output,
    size_t output_size)
{
    if (!device || !cJSON_IsObject(object) ||
        !output || output_size == 0) {
        return false;
    }

    double temperature = 0.0;
    double target = 0.0;
    double power = 0.0;
    double speed = 0.0;
    double rpm = 0.0;
    double humidity = 0.0;
    double pressure = 0.0;
    double value = 0.0;

    bool has_temperature =
        status_number(object, "temperature", &temperature);
    bool has_target =
        status_number(object, "target", &target);
    bool has_power =
        status_number(object, "power", &power);
    bool has_speed =
        status_number(object, "speed", &speed);
    bool has_rpm =
        status_number(object, "rpm", &rpm);
    bool has_humidity =
        status_number(object, "humidity", &humidity);
    bool has_pressure =
        status_number(object, "pressure", &pressure);
    bool has_value =
        status_number(object, "value", &value);

    if (has_temperature && has_target) {
        snprintf(
            output,
            output_size,
            "%.1f / %.1f C",
            temperature,
            target);
        return true;
    }

    if (has_temperature && has_humidity) {
        snprintf(
            output,
            output_size,
            "%.1f C  %.0f%% RH",
            temperature,
            humidity);
        return true;
    }

    if (has_temperature) {
        snprintf(
            output,
            output_size,
            "%.1f C",
            temperature);
        return true;
    }

    if (has_speed && has_rpm) {
        snprintf(
            output,
            output_size,
            "%.0f%%  %.0f RPM",
            status_percent(speed),
            rpm);
        return true;
    }

    if (has_speed) {
        snprintf(
            output,
            output_size,
            "%.0f%%",
            status_percent(speed));
        return true;
    }

    if (has_power) {
        snprintf(
            output,
            output_size,
            "%.0f%% POWER",
            status_percent(power));
        return true;
    }

    if (has_humidity) {
        snprintf(
            output,
            output_size,
            "%.0f%% RH",
            humidity);
        return true;
    }

    if (has_pressure) {
        snprintf(
            output,
            output_size,
            "%.1f HPA",
            pressure);
        return true;
    }

    if (has_value) {
        snprintf(
            output,
            output_size,
            value >= 0.0 && value <= 1.01
                ? "%.0f%%"
                : "%.2f",
            value >= 0.0 && value <= 1.01
                ? value * 100.0
                : value);
        return true;
    }

    const cJSON *colors =
        cJSON_GetObjectItemCaseSensitive(object, "color_data");

    if (cJSON_IsArray(colors)) {
        int count = cJSON_GetArraySize(colors);
        snprintf(
            output,
            output_size,
            "%d LED%s",
            count,
            count == 1 ? "" : "S");
        return true;
    }

    const cJSON *state =
        cJSON_GetObjectItemCaseSensitive(object, "state");

    if (cJSON_IsString(state) && state->valuestring) {
        snprintf(
            output,
            output_size,
            "%.*s",
            DEVICE_CATALOG_VALUE_TEXT_MAX - 1,
            state->valuestring);
        return true;
    }

    if (cJSON_IsBool(state)) {
        snprintf(
            output,
            output_size,
            "%s",
            cJSON_IsTrue(state) ? "ON" : "OFF");
        return true;
    }

    return false;
}


bool device_catalog_controller_merge_status(
    const struct cJSON *status)
{
    if (!s_store || !cJSON_IsObject(status)) {
        return false;
    }

    bool received_value = false;
    bool changed = false;
    const cJSON *object = NULL;

    cJSON_ArrayForEach(object, status) {
        if (!object->string ||
            !object->string[0] ||
            !cJSON_IsObject(object)) {
            continue;
        }

        device_descriptor_t device;
        bool found = false;

        portENTER_CRITICAL(&s_lock);

        for (size_t index = 0;
             index < s_store->status.stored_count;
             ++index) {
            if (strcmp(
                    s_store->devices[index].object_name,
                    object->string) == 0) {
                device = s_store->devices[index];
                found = true;
                break;
            }
        }

        portEXIT_CRITICAL(&s_lock);

        if (!found) {
            continue;
        }

        char formatted[DEVICE_CATALOG_VALUE_TEXT_MAX];

        if (!format_generic_status_value(
                &device,
                object,
                formatted,
                sizeof(formatted))) {
            continue;
        }

        received_value = true;
        portENTER_CRITICAL(&s_lock);

        for (size_t index = 0;
             index < s_store->status.stored_count;
             ++index) {
            device_descriptor_t *destination =
                &s_store->devices[index];

            if (strcmp(
                    destination->object_name,
                    object->string) != 0) {
                continue;
            }

            if (!destination->live_value_valid ||
                strcmp(
                    destination->live_value,
                    formatted) != 0) {
                snprintf(
                    destination->live_value,
                    sizeof(destination->live_value),
                    "%s",
                    formatted);
                destination->live_value_valid = true;
                changed = true;
            }

            break;
        }

        portEXIT_CRITICAL(&s_lock);
    }

    if (changed) {
        portENTER_CRITICAL(&s_lock);
        ++s_store->status.value_generation;

        if (s_store->status.value_generation == 0) {
            s_store->status.value_generation = 1;
        }

        portEXIT_CRITICAL(&s_lock);
    }

    return received_value;
}


bool device_catalog_controller_init(void)
{
    if (s_store) {
        return true;
    }

    s_store = heap_caps_calloc(
        1,
        sizeof(*s_store),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (s_store) {
        ESP_LOGI(
            TAG,
            "Device catalog allocated permanently in PSRAM: %u bytes",
            (unsigned)sizeof(*s_store));
        return true;
    }

    s_store = heap_caps_calloc(
        1,
        sizeof(*s_store),
        MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

    if (!s_store) {
        ESP_LOGE(TAG, "Unable to allocate device catalog");
        return false;
    }

    ESP_LOGW(TAG, "Device catalog using internal RAM fallback");
    return true;
}


void device_catalog_controller_reset(void)
{
    if (!s_store) {
        return;
    }

    portENTER_CRITICAL(&s_lock);

    uint32_t generation =
        s_store->status.generation + 1;
    uint32_t value_generation =
        s_store->status.value_generation + 1;

    memset(s_store, 0, sizeof(*s_store));
    s_store->status.generation =
        generation ? generation : 1;
    s_store->status.value_generation =
        value_generation ? value_generation : 1;

    portEXIT_CRITICAL(&s_lock);
}


void device_catalog_controller_update_from_objects(
    const struct cJSON *objects)
{
    if (!s_store) {
        return;
    }

    portENTER_CRITICAL(&s_lock);

    uint32_t generation =
        s_store->status.generation + 1;
    uint32_t value_generation =
        s_store->status.value_generation + 1;

    memset(s_store, 0, sizeof(*s_store));
    s_store->status.discovered =
        cJSON_IsArray(objects);
    s_store->status.generation =
        generation ? generation : 1;
    s_store->status.value_generation =
        value_generation ? value_generation : 1;

    if (cJSON_IsArray(objects)) {
        const cJSON *entry = NULL;

        cJSON_ArrayForEach(entry, objects) {
            if (!cJSON_IsString(entry) ||
                !entry->valuestring ||
                !entry->valuestring[0]) {
                continue;
            }

            ++s_store->status.total_object_count;

            device_kind_t kind =
                classify_object(entry->valuestring);

            if (kind != DEVICE_KIND_OTHER) {
                store_object_locked(
                    entry->valuestring,
                    kind);
            }
        }

        /*
         * Unknown objects are retained after recognized hardware so a large
         * Klipper installation cannot crowd useful controls out of the
         * bounded catalog.
         */
        cJSON_ArrayForEach(entry, objects) {
            if (!cJSON_IsString(entry) ||
                !entry->valuestring ||
                !entry->valuestring[0] ||
                classify_object(entry->valuestring) !=
                    DEVICE_KIND_OTHER) {
                continue;
            }

            store_object_locked(
                entry->valuestring,
                DEVICE_KIND_OTHER);
        }

        qsort(
            s_store->devices,
            s_store->status.stored_count,
            sizeof(s_store->devices[0]),
            compare_devices);
    }

    device_catalog_status_t status =
        s_store->status;

    portEXIT_CRITICAL(&s_lock);

    ESP_LOGI(
        TAG,
        "Catalog generation=%u stored=%u total=%u truncated=%d",
        (unsigned)status.generation,
        (unsigned)status.stored_count,
        (unsigned)status.total_object_count,
        status.truncated);
}


void device_catalog_controller_status(
    device_catalog_status_t *output)
{
    if (!output) {
        return;
    }

    memset(output, 0, sizeof(*output));

    if (!s_store) {
        return;
    }

    portENTER_CRITICAL(&s_lock);
    *output = s_store->status;
    portEXIT_CRITICAL(&s_lock);
}


bool device_catalog_controller_get(
    size_t index,
    device_descriptor_t *output)
{
    if (!s_store || !output) {
        return false;
    }

    bool found = false;
    portENTER_CRITICAL(&s_lock);

    if (index < s_store->status.stored_count) {
        *output = s_store->devices[index];
        found = true;
    }

    portEXIT_CRITICAL(&s_lock);
    return found;
}


bool device_catalog_controller_contains(
    const char *object_name)
{
    if (!s_store || !object_name || !object_name[0]) {
        return false;
    }

    bool found = false;
    portENTER_CRITICAL(&s_lock);

    for (size_t index = 0;
         index < s_store->status.stored_count;
         ++index) {
        if (strcmp(
                s_store->devices[index].object_name,
                object_name) == 0) {
            found = true;
            break;
        }
    }

    portEXIT_CRITICAL(&s_lock);
    return found;
}


const char *device_catalog_kind_label(
    device_kind_t kind)
{
    switch (kind) {
    case DEVICE_KIND_THERMAL:
        return "THERMAL";
    case DEVICE_KIND_AIR:
        return "AIR";
    case DEVICE_KIND_POWER:
        return "POWER";
    case DEVICE_KIND_SENSOR:
        return "SENSOR";
    case DEVICE_KIND_OUTPUT:
        return "OUTPUT";
    case DEVICE_KIND_MOTION:
        return "MOTION";
    case DEVICE_KIND_OTHER:
    default:
        return "OTHER";
    }
}
