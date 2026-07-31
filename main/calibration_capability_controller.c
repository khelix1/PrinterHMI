#include "calibration_capability_controller.h"

#include <ctype.h>
#include <string.h>

#include "cJSON.h"
#include "device_catalog_controller.h"
#include "macro_controller.h"

static bool s_axis_twist_command_available;
static uint32_t s_command_generation;


static bool starts_with(
    const char *text,
    const char *prefix)
{
    if (!text || !prefix) {
        return false;
    }

    return strncmp(text, prefix, strlen(prefix)) == 0;
}


bool calibration_capability_controller_merge_status(
    const struct cJSON *status)
{
    const cJSON *gcode = cJSON_IsObject(status)
        ? cJSON_GetObjectItemCaseSensitive(status, "gcode")
        : NULL;
    const cJSON *commands = cJSON_IsObject(gcode)
        ? cJSON_GetObjectItemCaseSensitive(gcode, "commands")
        : NULL;

    if (!cJSON_IsObject(commands)) {
        return false;
    }

    bool available = cJSON_HasObjectItem(
        commands,
        "AXIS_TWIST_COMPENSATION_CALIBRATE");
    __atomic_store_n(
        &s_axis_twist_command_available,
        available,
        __ATOMIC_RELEASE);
    __atomic_add_fetch(
        &s_command_generation,
        1,
        __ATOMIC_ACQ_REL);
    return true;
}


void calibration_capability_controller_reset(void)
{
    __atomic_store_n(
        &s_axis_twist_command_available,
        false,
        __ATOMIC_RELEASE);
    __atomic_add_fetch(
        &s_command_generation,
        1,
        __ATOMIC_ACQ_REL);
}


static bool contains_case_insensitive(
    const char *text,
    const char *needle)
{
    if (!text || !needle || !needle[0]) {
        return false;
    }

    size_t needle_length = strlen(needle);

    for (const char *start = text; *start; ++start) {
        size_t index = 0;

        while (index < needle_length &&
               start[index] &&
               toupper((unsigned char)start[index]) ==
                   toupper((unsigned char)needle[index])) {
            ++index;
        }

        if (index == needle_length) {
            return true;
        }
    }

    return false;
}


static bool is_calibration_macro(const char *name)
{
    return contains_case_insensitive(name, "CALIBRAT") ||
        contains_case_insensitive(name, "SCREW") ||
        contains_case_insensitive(name, "GANTRY") ||
        contains_case_insensitive(name, "Z_TILT") ||
        contains_case_insensitive(name, "BED_MESH") ||
        contains_case_insensitive(name, "SHAPER") ||
        contains_case_insensitive(name, "RESONANCE") ||
        contains_case_insensitive(name, "PID") ||
        contains_case_insensitive(name, "Z_OFFSET") ||
        contains_case_insensitive(name, "PRESSURE_ADVANCE");
}


void calibration_capability_controller_snapshot(
    calibration_capabilities_t *output)
{
    if (!output) {
        return;
    }

    memset(output, 0, sizeof(*output));

    device_catalog_status_t device_status;
    macro_controller_status_t macro_status;

    device_catalog_controller_status(&device_status);
    macro_controller_status(&macro_status);

    output->discovered = device_status.discovered;
    output->device_generation = device_status.generation;
    output->macro_generation = macro_status.generation;
    output->command_generation =
        __atomic_load_n(
            &s_command_generation,
            __ATOMIC_ACQUIRE);

    for (size_t index = 0;
         index < device_status.stored_count;
         ++index) {
        device_descriptor_t device;

        if (!device_catalog_controller_get(index, &device)) {
            continue;
        }

        const char *name = device.object_name;

        output->bed_mesh =
            output->bed_mesh ||
            strcmp(name, "bed_mesh") == 0;
        output->screws_tilt =
            output->screws_tilt ||
            strcmp(name, "screws_tilt_adjust") == 0;
        output->quad_gantry_level =
            output->quad_gantry_level ||
            strcmp(name, "quad_gantry_level") == 0;
        output->z_tilt =
            output->z_tilt ||
            strcmp(name, "z_tilt") == 0;
        output->axis_twist =
            output->axis_twist ||
            strcmp(name, "axis_twist_compensation") == 0;

        output->input_shaper =
            output->input_shaper ||
            strcmp(name, "input_shaper") == 0;
        output->accelerometer =
            output->accelerometer ||
            starts_with(name, "adxl345") ||
            starts_with(name, "lis2dw") ||
            starts_with(name, "mpu9250");

        output->hotend_pid =
            output->hotend_pid ||
            starts_with(name, "extruder");
        output->bed_pid =
            output->bed_pid ||
            strcmp(name, "heater_bed") == 0;
        output->generic_heater_pid =
            output->generic_heater_pid ||
            starts_with(name, "heater_generic ");
        output->pressure_advance =
            output->pressure_advance ||
            starts_with(name, "extruder");

        output->bltouch =
            output->bltouch ||
            strcmp(name, "bltouch") == 0;
        output->load_cell_probe =
            output->load_cell_probe ||
            starts_with(name, "load_cell_probe") ||
            starts_with(name, "load_cell ");
        output->probe =
            output->probe ||
            strcmp(name, "probe") == 0 ||
            starts_with(name, "probe_eddy_current ") ||
            strcmp(name, "smart_effector") == 0 ||
            output->bltouch ||
            output->load_cell_probe;
    }

    output->axis_twist =
        output->axis_twist ||
        __atomic_load_n(
            &s_axis_twist_command_available,
            __ATOMIC_ACQUIRE);

    for (size_t index = 0;
         index < macro_status.count;
         ++index) {
        char macro_name[MACRO_CONTROLLER_NAME_MAX];

        if (macro_controller_get(
                index,
                macro_name,
                sizeof(macro_name)) &&
            is_calibration_macro(macro_name)) {
            ++output->calibration_macro_count;
        }
    }

    output->tool_count =
        (output->bed_mesh ? 1U : 0U) +
        (output->screws_tilt ? 1U : 0U) +
        (output->quad_gantry_level ? 1U : 0U) +
        (output->z_tilt ? 1U : 0U) +
        (output->axis_twist ? 1U : 0U) +
        (output->input_shaper ? 1U : 0U) +
        (output->accelerometer ? 1U : 0U) +
        (output->hotend_pid ? 1U : 0U) +
        (output->bed_pid ? 1U : 0U) +
        (output->generic_heater_pid ? 1U : 0U) +
        (output->pressure_advance ? 1U : 0U) +
        (output->probe ? 1U : 0U) +
        output->calibration_macro_count;
}
