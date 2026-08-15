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

/*
 * Shows profile management.  An empty requested slot opens directly in the
 * Add Printer editor; a negative index selects the active profile.
 */
void ui_printer_profiles_show_for_slot(
    int profile_index,
    ui_printer_profiles_active_changed_cb_t active_changed_cb,
    ui_printer_profiles_discover_cb_t discover_cb);

void ui_printer_profiles_set_discovered_endpoint(
    const char *host,
    int port,
    const char *identity);

/* Opens the selected profile editor directly at its Camera Setup panel. */
void ui_printer_profiles_open_camera_setup(
    int profile_index,
    ui_printer_profiles_active_changed_cb_t active_changed_cb,
    ui_printer_profiles_discover_cb_t discover_cb);

void ui_printer_profiles_close_all(void);
