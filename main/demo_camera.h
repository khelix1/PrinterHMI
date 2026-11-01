#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "moonraker_probe.h"

#define DEMO_CAMERA_WIDTH 480
#define DEMO_CAMERA_HEIGHT 320
#define DEMO_CAMERA_FRONT_URL "http://simulation.invalid/camera/1"
#define DEMO_CAMERA_SIDE_URL "http://simulation.invalid/camera/2"
/* Pure sample artwork, shared by the desktop and tablet. */
void demo_camera_draw(uint16_t *pixels, bool side);
bool demo_camera_seed(void);
bool demo_camera_stream_start(const char *url);
bool demo_camera_stream_busy(void);
void demo_camera_stream_stop(void);
bool demo_camera_stream_take(uint8_t **pixels, size_t *size, int *width, int *height, bool *ok);
bool demo_camera_test_start(const char *url);
bool demo_camera_test_take(bool *ok, int *width, int *height, size_t *bytes);
bool demo_camera_discovery_start(int profile, const char *host, int port);
bool demo_camera_discovery_take(moonraker_webcam_t *webcam, bool *found, size_t *count);
