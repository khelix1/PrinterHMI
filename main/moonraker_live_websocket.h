#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Active-profile Moonraker WebSocket lifecycle.
 *
 * Phase 1 observes and logs a real subscription while HTTP polling remains
 * authoritative. Call only from the existing application runtime task.
 */
void moonraker_live_websocket_tasklet(
    bool wifi_ready,
    const char *host,
    int port,
    const char *api_key,
    uint32_t configuration_generation);

bool moonraker_live_websocket_connected(void);
bool moonraker_live_websocket_subscribed(void);
bool moonraker_live_websocket_fresh(int64_t maximum_age_us);

/* A file-list notification is coalesced until the LVGL owner consumes it. */
bool moonraker_live_websocket_file_change_pending(void);
bool moonraker_live_websocket_take_file_change(void);

void moonraker_live_websocket_stop(void);

#ifdef __cplusplus
}
#endif
