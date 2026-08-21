#include "ui_devices_live_values.h"
#include "ui_text.h"

#include <stdbool.h>
#include <string.h>

#include "device_catalog_controller.h"
#include "moonraker.h"
#include "ui_theme.h"

#define DEVICE_LIVE_VALUE_CAPACITY 48

typedef struct {
    lv_obj_t *owner;
    lv_obj_t *labels[DEVICE_LIVE_VALUE_CAPACITY];
    size_t catalog_indices[DEVICE_LIVE_VALUE_CAPACITY];
    size_t count;
} ui_devices_live_values_state_t;

static ui_devices_live_values_state_t s_live;

static double percent_value(
    double value)
{
    if (value >= 0.0 && value <= 1.01) {
        return value * 100.0;
    }

    return value;
}


static bool format_known_live_value(
    const device_descriptor_t *device,
    const moonraker_state_t *state,
    const moonraker_filament_state_t *filament,
    char *output,
    size_t output_size)
{
    if (!device || !state || !output || output_size == 0) {
        return false;
    }

    for (size_t index = 0;
         index < state->hotend_count;
         ++index) {
        const moonraker_hotend_t *hotend =
            &state->hotends[index];

        if (strcmp(
                device->object_name,
                hotend->object_name) == 0) {
            lv_snprintf(
                output,
                output_size,
                "%.1f / %.1f C%s",
                hotend->temperature,
                hotend->target,
                hotend->active ? "  ACTIVE" : "");
            return true;
        }
    }

    if (strcmp(device->object_name, "heater_bed") == 0) {
        lv_snprintf(
            output,
            output_size,
            "%.1f / %.1f C",
            state->bed_temp,
            state->bed_target);
        return true;
    }

    if (strcmp(device->object_name, "fan") == 0) {
        lv_snprintf(
            output,
            output_size,
            "%.0f%%",
            percent_value(state->part_fan_speed));
        return true;
    }

    if (strcmp(
            device->object_name,
            "temperature_sensor drybox_center") == 0) {
        lv_snprintf(
            output,
            output_size,
            "%.1f C",
            state->chamber_temp);
        return true;
    }

    if (strcmp(
            device->object_name,
            "sht3x drybox_env") == 0) {
        lv_snprintf(
            output,
            output_size,
            "%.1f C  %.0f%% RH",
            state->air_temp,
            state->humidity);
        return true;
    }

    if (strcmp(
            device->object_name,
            "heater_generic drybox_heater") == 0) {
        lv_snprintf(
            output,
            output_size,
            "%s  TARGET %.0f C",
            state->heater_on ? "ON" : "OFF",
            state->heater_target);
        return true;
    }

    if (strcmp(
            device->object_name,
            "fan_generic drybox_fan") == 0) {
        lv_snprintf(
            output,
            output_size,
            "%.0f%%",
            percent_value(state->drybox_fan_speed));
        return true;
    }

    if (strcmp(device->object_name, "toolhead") == 0 &&
        state->toolhead_position_valid) {
        lv_snprintf(
            output,
            output_size,
            "X%.1f Y%.1f Z%.2f",
            state->toolhead_x,
            state->toolhead_y,
            state->toolhead_z);
        return true;
    }

    if (filament) {
        for (size_t index = 0;
             index < filament->sensor_count;
             ++index) {
            const moonraker_filament_sensor_t *sensor =
                &filament->sensors[index];

            if (strcmp(
                    device->object_name,
                    sensor->object_name) != 0) {
                continue;
            }

            if (!sensor->enabled) {
                lv_snprintf(
                    output,
                    output_size,
                    "DISABLED");
            } else if (!sensor->status_known) {
                lv_snprintf(
                    output,
                    output_size,
                    "CHECKING");
            } else {
                lv_snprintf(
                    output,
                    output_size,
                    "%s",
                    sensor->filament_detected
                        ? "FILAMENT PRESENT"
                        : "RUNOUT");
            }

            return true;
        }
    }

    return false;
}




void ui_devices_live_values_init(
    lv_obj_t *owner)
{
    ui_devices_live_values_close();
    s_live.owner = owner;
}


void ui_devices_live_values_clear(void)
{
    for (size_t index = 0;
         index < DEVICE_LIVE_VALUE_CAPACITY;
         ++index) {
        s_live.labels[index] = NULL;
        s_live.catalog_indices[index] = 0;
    }

    s_live.count = 0;
}


void ui_devices_live_values_register(
    size_t visible_index,
    lv_obj_t *value_label,
    size_t catalog_index)
{
    if (!s_live.owner ||
        !value_label ||
        visible_index >= DEVICE_LIVE_VALUE_CAPACITY) {
        return;
    }

    s_live.labels[visible_index] = value_label;
    s_live.catalog_indices[visible_index] =
        catalog_index;

    if (visible_index + 1 > s_live.count) {
        s_live.count = visible_index + 1;
    }
}


void ui_devices_live_values_update(void)
{
    if (!s_live.owner) {
        return;
    }

    moonraker_state_t state;
    moonraker_filament_state_t filament;

    moonraker_state_snapshot(&state);
    moonraker_filament_state_snapshot(&filament);

    for (size_t visible = 0;
         visible < s_live.count;
         ++visible) {
        lv_obj_t *label = s_live.labels[visible];

        if (!label) {
            continue;
        }

        device_descriptor_t device;

        if (!device_catalog_controller_get(
                s_live.catalog_indices[visible],
                &device)) {
            lv_label_set_text(label, ui_text("--"));
            continue;
        }

        char value[80];

        if (format_known_live_value(
                &device,
                &state,
                &filament,
                value,
                sizeof(value))) {
            lv_label_set_text(label, value);
            ui_apply_label_bright(label);
        } else if (device.live_value_valid) {
            lv_label_set_text(
                label,
                device.live_value);
            ui_apply_label_bright(label);
        } else {
            bool expects_live_value =
                device_catalog_controller_has_live_value_source(
                    device.object_name);

            lv_label_set_text(
                label,
                expects_live_value
                    ? ui_text("WAITING FOR DATA")
                    : ui_text("DISCOVERED"));
            ui_apply_label_dim(label);
        }
    }
}


void ui_devices_live_values_close(void)
{
    ui_devices_live_values_clear();
    s_live.owner = NULL;
}
