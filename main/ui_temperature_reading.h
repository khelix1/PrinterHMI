#pragma once

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#define UI_TEMPERATURE_PAIR_UNKNOWN "--/--°"

static inline bool ui_temperature_current_valid(double current)
{
    return isfinite(current) && current > -100.0 && current <= 999.0;
}

/* Keep the current/target convention shared by tools, nozzle and bed. */
static inline void ui_format_temperature_pair(char *text, size_t size, double current, double target)
{
    char reading[8] = "--", setpoint[8] = "--";
    if (ui_temperature_current_valid(current)) snprintf(reading, sizeof(reading), "%.1f", current);
    if (isfinite(target) && target >= 0.0 && target <= 999.0)
        snprintf(setpoint, sizeof(setpoint), "%.1f", target);
    snprintf(text, size, "%s/%s°", reading, setpoint);
}
