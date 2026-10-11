#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "moonraker_probe.h"

/* One bounded, asynchronous webcam lookup for the profile currently being edited. */
bool camera_discovery_start(const char *host, int port, const char *api_key);
/* Editor lookups capture the selected profile, including inactive profiles. */
bool camera_discovery_start_for_profile(int profile, const char *host, int port,
                                        const char *api_key);
bool camera_discovery_busy(void);
/* Consume once; stale configuration results are discarded. Current results
 * are imported here, never by the network worker. count is the returned count. */
bool camera_discovery_take_result(moonraker_webcam_t *webcam, bool *found,
                                  size_t *count);
