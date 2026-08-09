#pragma once

#include <stdbool.h>

typedef void (*ui_calibration_open_bed_mesh_cb_t)(void);
typedef bool (*ui_calibration_send_gcode_cb_t)(
    const char *command);

void ui_calibration_show(
    ui_calibration_open_bed_mesh_cb_t open_bed_mesh_cb,
    ui_calibration_send_gcode_cb_t send_gcode_cb);

void ui_calibration_hide(void);
