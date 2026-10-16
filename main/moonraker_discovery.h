#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_netif_ip_addr.h"

/* Forward declaration keeps the discovery API lightweight for callers. */
typedef struct _lv_obj_t lv_obj_t;

typedef void (*moonraker_discovery_close_cb_t)(void);
typedef void (*moonraker_discovery_select_cb_t)(
    const char *host,
    int port,
    const char *identity);

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

/* Same workflow, rendered inside a caller-owned card instead of a modal layer. */
void moonraker_discovery_show_in_parent(
    lv_obj_t *parent,
    const char *status_text,
    moonraker_discovery_close_cb_t close_cb,
    moonraker_discovery_select_cb_t select_cb);

void moonraker_discovery_set_status(const char *status_text);

bool moonraker_discovery_start(const esp_ip4_addr_t *ip);
bool moonraker_discovery_is_running(void);
bool moonraker_discovery_is_cancelled(void);
bool moonraker_discovery_is_open(void);

void moonraker_discovery_close(void);

/* Request-bound selection for profile drafts. Repeated Show for the same
 * request preserves its callback; a different request replaces the old scan.
 * A Close callback that opens another dialog retains that replacement. */
typedef void (*moonraker_discovery_close_request_cb_t)(uint32_t request);
typedef void (*moonraker_discovery_select_request_cb_t)(
    const char *host,int port,const char *identity,uint32_t request);
bool moonraker_discovery_show_for_request(const char *status_text,
    moonraker_discovery_close_request_cb_t close_cb,
    moonraker_discovery_select_request_cb_t select_cb,uint32_t request);
bool moonraker_discovery_show_in_parent_for_request(lv_obj_t *parent,const char *status_text,
    moonraker_discovery_close_request_cb_t close_cb,
    moonraker_discovery_select_request_cb_t select_cb,uint32_t request);
/* Only closes the current request's popup. A stale callback cannot close a
 * replacement request or a legacy/unrelated discovery popup. */
void moonraker_discovery_close_request(uint32_t request);
