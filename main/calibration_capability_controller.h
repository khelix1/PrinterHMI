#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct cJSON;

typedef struct {
    bool discovered;

    bool bed_mesh;
    bool screws_tilt;
    bool quad_gantry_level;
    bool z_tilt;
    bool axis_twist;

    bool input_shaper;
    bool accelerometer;

    bool hotend_pid;
    bool bed_pid;
    bool generic_heater_pid;
    bool pressure_advance;

    bool probe;
    bool bltouch;
    bool load_cell_probe;

    size_t calibration_macro_count;
    size_t tool_count;

    uint32_t device_generation;
    uint32_t macro_generation;
    uint32_t command_generation;
} calibration_capabilities_t;

/*
 * Merges command capabilities reported by Klipper's gcode status object.
 * Returns true when the supplied status contained a usable command catalog.
 */
bool calibration_capability_controller_merge_status(
    const struct cJSON *status);

/* Invalidates command capabilities during disconnect or Klipper restart. */
void calibration_capability_controller_reset(void);

/*
 * Derives calibration capabilities from the permanent device and macro
 * catalogs plus the last synchronized Klipper command catalog. This
 * controller performs no command dispatch.
 */
void calibration_capability_controller_snapshot(
    calibration_capabilities_t *output);
