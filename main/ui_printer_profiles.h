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

/* Nonzero identity for the current editor's Discover request. Zero means the
 * originating editor/draft is no longer current. UI-thread calls only. */
uint32_t ui_printer_profiles_discovery_request(void);
void ui_printer_profiles_cancel_discovery_request(uint32_t request);
bool ui_printer_profiles_apply_discovered_endpoint(uint32_t request,
    const char *host,int port,const char *identity);

/* Synchronous edit helper for a caller already owning the current editor.
 * Discovery callbacks must use the request-bound API above. */
void ui_printer_profiles_set_discovered_endpoint(
    const char *host,
    int port,
    const char *identity);

/* Opens Camera setup directly for the saved profile; closes prior profile
 * dialogs. Cancel/Use camera returns to the caller without an editor stack.
 * Callback parameters are retained for source compatibility. */
void ui_printer_profiles_open_camera_setup(
    int profile_index,
    ui_printer_profiles_active_changed_cb_t active_changed_cb,
    ui_printer_profiles_discover_cb_t discover_cb);

void ui_printer_profiles_close_all(void);
