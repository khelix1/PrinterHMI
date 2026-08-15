#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Fetches one JPEG from an MJPEG endpoint per call. The returned frame belongs
 * to the caller and must be released with heap_caps_free(). */
bool camera_stream_start(const char *url);
bool camera_stream_busy(void);
void camera_stream_stop(void);
bool camera_stream_take_result(
    uint8_t **pixels,
    size_t *pixel_size,
    int *width,
    int *height,
    bool *ok);
