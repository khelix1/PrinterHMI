#pragma once

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include "cJSON.h"

/* Keep headroom for display rounding and signed integer conversion. */
static inline bool moonraker_telemetry_number(const cJSON *item, double scale, double *out)
{
    if (!cJSON_IsNumber(item) || !isfinite(item->valuedouble)) return false;
    double value = item->valuedouble * scale;
    if (!isfinite(value) || fabs(value) > INT32_MAX / 2) return false;
    if (out) *out = value;
    return true;
}

static inline bool moonraker_drybox_program(double value, int maximum)
{
    return value >= 0 && value <= maximum && value == (int)value;
}
