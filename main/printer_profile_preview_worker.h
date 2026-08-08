#pragma once

/*
 * Refresh one inactive printer's current-print preview.
 * Called by the existing application runtime worker; never creates a task.
 */
void printer_profile_preview_worker_v32_poll_one(const char *api_key);

/* Starts the low-priority worker that owns blocking inactive-profile I/O. */
void printer_profile_preview_worker_v32_start(const char *api_key);
void printer_profile_preview_worker_v32_reset(void);
