#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Lightweight multi-printer health cache.
 *
 * The existing application runtime worker calls poll_one(). No additional
 * FreeRTOS task, timer, mutex, or scheduler allocation is introduced.
 */
void printer_profile_health_poll_one(void);
void printer_profile_health_reset(void);

/* Preserve unchanged endpoints; clear health only where config changed. */
void printer_profile_health_reconcile(void);

bool printer_profile_health_get(
    int profile_index,
    bool *known_out);


#define PRINTER_PROFILE_HEALTH_STATE_LENGTH 32

/* Updated by the combined inactive-printer status/preview worker. */
void printer_profile_health_set(
    int profile_index,
    bool known,
    bool online);

/* One normalized print_stats.state snapshot per inactive profile. */
void printer_profile_health_set_live_state(
    int profile_index,
    bool known,
    bool online,
    const char *printer_state);

bool printer_profile_health_get_live_state(
    int profile_index,
    char *out,
    size_t out_size);

bool printer_profile_health_live_state_fresh(
    int profile_index,
    int64_t maximum_age_us);

/* Reports one serialized inactive-worker result. Failed requests require
 * confirmation before changing an already-online profile to OFFLINE. */
void printer_profile_health_report_live_state(
    int profile_index,
    bool online,
    const char *printer_state);

/* Indicates a reachable Moonraker endpoint while Klipper is starting. */
void printer_profile_health_set_verifying(int profile_index);


/* Shared round-robin cursor for the existing runtime worker. */
int printer_profile_health_take_next_index(void);
