#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "moonraker.h"

#define TELEMETRY_HISTORY_CAPACITY 300
#define TELEMETRY_HISTORY_SAMPLE_INTERVAL_US 2000000LL

enum {TELEMETRY_NO_BED=1u<<0,TELEMETRY_NO_CENTER=1u<<1,TELEMETRY_NO_ENV=1u<<2,
    TELEMETRY_NO_PART_FAN=1u<<3,TELEMETRY_NO_DRYBOX_FAN=1u<<4};

typedef struct {
    int64_t time_us;
    uint32_t unavailable;
    size_t hotend_count;
    moonraker_hotend_t hotends[MOONRAKER_MAX_HOTENDS];
    char active_hotend[MOONRAKER_HOTEND_NAME_MAX];
    double heater_target;
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
 * Records at most one live sample per wall-clock two-second bin.
 * Actual timestamps are retained; missed/offline bins are never backfilled.
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

/* Capability-aware conversion; missing/unsupported values become NAN. */
void telemetry_history_from_state(const moonraker_state_t *state, int64_t now_us, telemetry_sample_t *out);
uint32_t telemetry_history_generation(void);
