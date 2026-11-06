#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Runs an MJPEG worker and delivers decoded RGB565 frames through take_result.
 * Each returned frame belongs to the caller; release it with heap_caps_free(). */
bool camera_stream_start(const char *url);
bool camera_stream_busy(void);
/* Navigation retires results immediately; worker cleanup completes asynchronously. */
void camera_stream_request_stop(void);
/* Explicit transport quiescence may wait for the worker (e.g. OTA setup). */
void camera_stream_stop(void);
bool camera_stream_take_result(
    uint8_t **pixels,
    size_t *pixel_size,
    int *width,
    int *height,
    bool *ok);
