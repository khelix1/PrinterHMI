#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Persistent profile-owned preview snapshots.
 *
 * PSRAM remains the live authority. This module stores one compressed PNG
 * plus endpoint/file identity per profile and restores one slot per call from
 * the existing runtime worker. It creates no task or timer.
 */
bool printer_preview_store_v32_store_png(
    int profile_index,
    const char *expected_host,
    int expected_port,
    const char *file,
    const uint8_t *png,
    size_t png_size);

bool printer_preview_store_v32_store_active(
    const char *file,
    const uint8_t *png,
    size_t png_size);

void printer_preview_store_v32_restore_one(bool sd_available);
void printer_preview_store_v32_reset_restore(void);
void printer_preview_store_v32_invalidate(int profile_index);
