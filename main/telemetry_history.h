#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "moonraker.h"

#define TELEMETRY_HISTORY_CAPACITY 300
#define TELEMETRY_HISTORY_SAMPLE_INTERVAL_US 2000000LL

typedef struct {
    double nozzle_temp;
    double nozzle_target;
    double bed_temp;
    double bed_target;

    double air_temp;
    double center_temp;
    double humidity;

    double live_velocity;
    double live_flow;

    double part_fan_speed;
    double drybox_fan_speed;
    double speed_factor;
    double flow_factor;
} telemetry_sample_t;

bool telemetry_history_init(void);

/*
 * Clears samples when the active printer changes so histories from
 * different machines are never combined.
 */
void telemetry_history_reset(void);

/*
 * Records at most one sample every two seconds.
 *
 * Returns true only when a new valid sample was committed.
 */
bool telemetry_history_sample(
    const moonraker_state_t *state,
    int64_t now_us);

size_t telemetry_history_count(void);

bool telemetry_history_get(
    size_t logical_index,
    telemetry_sample_t *out);
