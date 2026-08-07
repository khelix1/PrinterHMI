#pragma once

#include <stdbool.h>

#include "moonraker_probe.h"

/*
 * One bounded asynchronous verification for a manually entered endpoint.
 * It never edits or selects a printer profile.
 */
bool moonraker_endpoint_test_start(const char *host, int port);
bool moonraker_endpoint_test_busy(void);
bool moonraker_endpoint_test_take_result(
    moonraker_probe_result_t *result,
    bool *verified);
