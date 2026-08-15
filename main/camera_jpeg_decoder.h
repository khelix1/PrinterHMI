#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Decodes a baseline JPEG to a PSRAM-first RGB565 buffer owned by the caller. */
bool camera_jpeg_decode_rgb565(
    const uint8_t *jpeg,
    size_t jpeg_size,
    uint16_t **pixels,
    int *width,
    int *height);
