#pragma once

#include <stdbool.h>

#include "esp_netif_ip_addr.h"

typedef void (*moonraker_discovery_close_cb_t)(void);
typedef void (*moonraker_discovery_select_cb_t)(
    const char *host,
    int port);

/*
 * Moonraker discovery owns the complete discovery workflow:
 *
 * - modal popup and status presentation
 * - scrollable candidate-list rendering
 * - candidate data lifetime and selection routing
 * - scan task, cancellation and close state
 * - thread-safe LVGL updates
 *
 * Shared popup, list, row, footer-action, button, typography and theme
 * primitives remain owned by their reusable UI modules.
 */

void moonraker_discovery_show(
    const char *status_text,
    moonraker_discovery_close_cb_t close_cb,
    moonraker_discovery_select_cb_t select_cb);

void moonraker_discovery_set_status(const char *status_text);

bool moonraker_discovery_start(const esp_ip4_addr_t *ip);
bool moonraker_discovery_is_running(void);
bool moonraker_discovery_is_cancelled(void);
bool moonraker_discovery_is_open(void);

void moonraker_discovery_close(void);
