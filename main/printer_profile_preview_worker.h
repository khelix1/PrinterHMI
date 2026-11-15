#pragma once

#include <stdbool.h>

/*
 * Refresh due inactive printer statuses, then at most one print preview.
 * Requests use that profile's own credentials.
 * Called by the existing application runtime worker; never creates a task.
 */
void printer_profile_preview_worker_poll_cycle(void);

/* Starts the low-priority worker that owns blocking inactive-profile I/O. */
void printer_profile_preview_worker_start(void);
void printer_profile_preview_worker_reset(void);
void printer_profile_preview_worker_set_network_ready(bool ready);
