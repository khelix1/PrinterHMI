#pragma once

#include <stdbool.h>

#include "lvgl.h"

typedef bool (*ui_calibration_pa_send_gcode_cb_t)(
    const char *command);

typedef bool (*ui_calibration_pa_ready_cb_t)(
    const char *workflow);

/*
 * Owns Pressure Advance tower setup and its temporary-runtime-state guidance.
 * The parent Calibration page retains the thermal capability summary.
 */
void ui_calibration_pressure_advance_create(
    lv_obj_t *card,
    ui_calibration_pa_send_gcode_cb_t send_gcode_cb,
    ui_calibration_pa_ready_cb_t ready_cb);

void ui_calibration_pressure_advance_refresh(
    bool discovered,
    bool pressure_advance);

void ui_calibration_pressure_advance_hide(void);
