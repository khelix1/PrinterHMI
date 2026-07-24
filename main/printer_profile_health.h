#pragma once

#include <stdbool.h>

/*
 * Lightweight multi-printer health cache.
 *
 * The existing application runtime worker calls poll_one(). No additional
 * FreeRTOS task, timer, mutex, or scheduler allocation is introduced.
 */
void printer_profile_health_poll_one(void);
void printer_profile_health_reset(void);

bool printer_profile_health_get(
    int profile_index,
    bool *known_out);


/* Updated by the combined inactive-printer status/preview worker. */
void printer_profile_health_set(
    int profile_index,
    bool known,
    bool online);


/* Shared round-robin cursor for the existing runtime worker. */
int printer_profile_health_take_next_index(void);
