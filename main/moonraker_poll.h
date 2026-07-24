#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef bool (*moonraker_poll_fetch_cb_t)(void);

typedef enum {
    MOONRAKER_POLL_NOT_DUE = 0,
    MOONRAKER_POLL_BUSY,
    MOONRAKER_POLL_NO_WIFI,
    MOONRAKER_POLL_OK,
    MOONRAKER_POLL_FAILED,
} moonraker_poll_result_t;

void moonraker_poll_reset(void);

moonraker_poll_result_t moonraker_poll_run(
    int64_t now_us,
    bool wifi_ready,
    bool transport_busy,
    moonraker_poll_fetch_cb_t fetch_cb);
