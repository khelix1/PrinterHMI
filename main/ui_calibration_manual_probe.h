#pragma once

#include <stdbool.h>

#include "lvgl.h"

/*
 * Creates the one active manual-probe adjustment surface shared by Probe/Z
 * and Axis Twist. Workflow-specific command dispatch remains with the caller.
 */
bool ui_calibration_manual_probe_show(
    const char *title,
    const char *instructions,
    const char *accept_label,
    lv_event_cb_t step_cb,
    lv_event_cb_t abort_cb,
    lv_event_cb_t accept_cb);

void ui_calibration_manual_probe_set_status(
    const char *status);

bool ui_calibration_manual_probe_is_visible(void);

void ui_calibration_manual_probe_hide(void);
