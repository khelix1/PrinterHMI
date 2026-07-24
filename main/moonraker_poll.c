#include "moonraker_poll.h"

#ifndef MOONRAKER_POLL_INTERVAL_US
#define MOONRAKER_POLL_INTERVAL_US 1500000LL
#endif

static int64_t s_last_poll_us = 0;

void moonraker_poll_reset(void)
{
    s_last_poll_us = 0;
}

moonraker_poll_result_t moonraker_poll_run(
    int64_t now_us,
    bool wifi_ready,
    bool transport_busy,
    moonraker_poll_fetch_cb_t fetch_cb)
{
    if (!wifi_ready) {
        return MOONRAKER_POLL_NO_WIFI;
    }

    if (transport_busy) {
        return MOONRAKER_POLL_BUSY;
    }

    if (s_last_poll_us != 0 &&
        now_us - s_last_poll_us < MOONRAKER_POLL_INTERVAL_US) {
        return MOONRAKER_POLL_NOT_DUE;
    }

    s_last_poll_us = now_us;

    if (!fetch_cb) {
        return MOONRAKER_POLL_FAILED;
    }

    return fetch_cb()
        ? MOONRAKER_POLL_OK
        : MOONRAKER_POLL_FAILED;
}
