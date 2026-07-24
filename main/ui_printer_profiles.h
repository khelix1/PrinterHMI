#pragma once

#include "lvgl.h"

/*
 * Called only when the active printer endpoint changes.
 * The application owns clearing runtime state and restarting polling.
 */
typedef void (*ui_printer_profiles_active_changed_cb_t)(void);
typedef void (*ui_printer_profiles_discover_cb_t)(void);

void ui_printer_profiles_show(
    ui_printer_profiles_active_changed_cb_t active_changed_cb,
    ui_printer_profiles_discover_cb_t discover_cb);

void ui_printer_profiles_set_discovered_endpoint(
    const char *host,
    int port);

void ui_printer_profiles_close_all(void);
