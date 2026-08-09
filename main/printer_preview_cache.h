#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lvgl.h"

/*
 * Profile-indexed rendered preview cache.
 *
 * The active pipeline publishes RGB565. The inactive-printer worker publishes
 * downloaded PNG data through the same renderer. UI consumers only see a
 * completed, profile-validated image descriptor.
 *
 * Publish functions must be called while the LVGL/display lock is held.
 */
bool printer_preview_cache_publish_active(
    const char *file,
    const uint16_t *pixels,
    int width,
    int height);

bool printer_preview_cache_publish_png(
    int profile_index,
    const char *expected_host,
    int expected_port,
    const char *file,
    const uint8_t *png,
    size_t png_size,
    int width,
    int height);

bool printer_preview_cache_matches(
    int profile_index,
    const char *file);

const lv_image_dsc_t *printer_preview_cache_image(
    int profile_index,
    const char **file_out,
    uint32_t *revision_out);

void printer_preview_cache_invalidate(int profile_index);
void printer_preview_cache_reset(void);
