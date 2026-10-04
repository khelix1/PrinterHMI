#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

/* Shared source resolution for Files, Dashboard and Printer previews.
 * All three open this same aspect-fitted RGB565 canvas in the lightbox.
 */
#define THUMBNAIL_PREVIEW_WIDTH 286
#define THUMBNAIL_PREVIEW_HEIGHT 215

/*
 * Decode an LVGL image descriptor and aspect-fit it into an RGB565 buffer.
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
