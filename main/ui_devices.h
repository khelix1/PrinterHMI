#pragma once

typedef void (*ui_devices_open_telemetry_cb_t)(void);

void ui_devices_show(
    ui_devices_open_telemetry_cb_t open_telemetry_cb);

void ui_devices_hide(void);
