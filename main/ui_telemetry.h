#pragma once

#include "moonraker.h"

void ui_telemetry_v32_show(void);
void ui_telemetry_v32_hide(void);

/*
 * Call continuously from the normal UI refresh path.
 *
 * History sampling continues even while the Telemetry page is hidden.
 */
void ui_telemetry_v32_refresh(
    const moonraker_state_t *state,
    int64_t now_us);
