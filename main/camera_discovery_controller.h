#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "moonraker_probe.h"

/* One bounded, asynchronous webcam lookup for the profile currently being edited. */
bool camera_discovery_start(const char *host, int port, const char *api_key);
bool camera_discovery_busy(void);
/* count is the number of cameras imported into the profile catalog. */
bool camera_discovery_take_result(moonraker_webcam_t *webcam, bool *found,
                                  size_t *count);
