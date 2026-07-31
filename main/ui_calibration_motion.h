#pragma once

#include <stdbool.h>

#include "lvgl.h"

typedef bool (*ui_calibration_motion_send_gcode_cb_t)(
    const char *command);

typedef bool (*ui_calibration_motion_ready_cb_t)(
    const char *workflow);

typedef void (*ui_calibration_motion_show_results_cb_t)(
    const char *title,
    const char *waiting_text);

typedef void (*ui_calibration_motion_refresh_results_cb_t)(void);

/*
 * Owns the Input Shaper, Resonance, and accelerometer-check actions and
 * popups. The parent Calibration page retains card layout and capability
 * summaries.
 */
void ui_calibration_motion_create(
    lv_obj_t *card,
    ui_calibration_motion_send_gcode_cb_t send_gcode_cb,
    ui_calibration_motion_ready_cb_t ready_cb,
    ui_calibration_motion_show_results_cb_t show_results_cb,
    ui_calibration_motion_refresh_results_cb_t refresh_results_cb);

void ui_calibration_motion_refresh(
    bool discovered,
    bool input_shaper,
    bool accelerometer);

void ui_calibration_motion_hide(void);
