#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

/* Shared source resolution for Files, Dashboard and Printer previews.
 * The lightbox initially uses this stable canvas, then loads the full original.
 */
#define THUMBNAIL_PREVIEW_WIDTH 286
#define THUMBNAIL_PREVIEW_HEIGHT 215

/*
 * Decode an LVGL image descriptor and proportionally fill an RGB565 buffer.
 * Center-crops edges when aspect ratios differ, without stretching.
 *
 * The destination buffer must contain at least:
 *
 *     destination_width * destination_height
 *
 * uint16_t pixels.
 */
bool thumbnail_render_to_rgb565(
    const lv_image_dsc_t *image,
    uint16_t *destination,
    int destination_width,
    int destination_height);

/* Fullscreen keeps the complete original thumbnail, including its edges. */
bool thumbnail_render_to_rgb565_fit(const lv_image_dsc_t *image,
    uint16_t *destination, int destination_width, int destination_height);
