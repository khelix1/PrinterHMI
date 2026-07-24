#pragma once

#include <stdbool.h>

#include "esp_netif_ip_addr.h"

typedef void (*moonraker_discovery_close_cb_t)(void);
typedef void (*moonraker_discovery_select_cb_t)(const char *host);

/*
 * Stage 1 ownership:
 *
 * - popup object
 * - popup status label
 * - popup rendering
 * - candidate buttons
 * - candidate selection routing
 * - close/cancel state
 * - thread-safe status updates
 *
 * Probe and scan-task ownership remains temporarily in main.c.
 */

void moonraker_discovery_show(
    const char *status_text,
    moonraker_discovery_close_cb_t close_cb,
    moonraker_discovery_select_cb_t select_cb);

void moonraker_discovery_set_status(const char *status_text);

void moonraker_discovery_add_candidate(const char *host, int y);

bool moonraker_discovery_start(const esp_ip4_addr_t *ip);
bool moonraker_discovery_is_running(void);

bool moonraker_discovery_is_cancelled(void);

bool moonraker_discovery_is_open(void);

void moonraker_discovery_close(void);
