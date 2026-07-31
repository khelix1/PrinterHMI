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

/*
 * Immediately retires the current profile's logical ownership, then lets the
 * runtime task perform a quiet, two-phase transport rebind. This function is
 * safe to call from the active-profile selection bridge.
 */
void moonraker_live_websocket_prepare_profile_change(
    uint32_t configuration_generation);

/* True while Moonraker network workers must remain quiet for a safe rebind. */
bool moonraker_live_websocket_rebinding(void);

bool moonraker_live_websocket_connected(void);
bool moonraker_live_websocket_subscribed(void);
bool moonraker_live_websocket_fresh(int64_t maximum_age_us);

/*
 * Queues a G-code script on the existing live Moonraker WebSocket.
 * Success means the JSON-RPC request was transmitted without blocking for
 * long-running Klipper motion to finish.
 */
bool moonraker_live_websocket_send_gcode(
    const char *script);

/* A file-list notification is coalesced until the LVGL owner consumes it. */
bool moonraker_live_websocket_file_change_pending(void);
bool moonraker_live_websocket_take_file_change(void);

void moonraker_live_websocket_stop(void);

#ifdef __cplusplus
}
#endif
