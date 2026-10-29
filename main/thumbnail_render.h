#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

/* Shared source resolution for Dashboard and Printer previews. Files rows use
 * their own smaller canvas.
 * The lightbox initially uses this stable canvas, then loads the full original.
 */
#define THUMBNAIL_PREVIEW_WIDTH 286
#define THUMBNAIL_PREVIEW_HEIGHT 215

/* Fullscreen keeps the complete original thumbnail, including its edges. */
bool thumbnail_render_to_rgb565_fit(const lv_image_dsc_t *image,
    uint16_t *destination, int destination_width, int destination_height);

/* Complete source, tightly packed within max_width * max_height pixel capacity.
 * Returns actual dimensions/stride (width * 2), with no baked-in letterbox.
 * Width/height are zero on failure. Tiny aspect rounding is at most one pixel.
 */
bool thumbnail_render_to_rgb565_aspect(const lv_image_dsc_t *image,
    uint16_t *destination, int max_width, int max_height, int *width, int *height);
