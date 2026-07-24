#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Fetch and display the Moonraker G-code file list.
 *
 * This controller owns:
 * - file-list response allocation
 * - Moonraker file-list transport
 * - JSON/path iteration
 * - population of the Files page
 *
 * The caller remains responsible for providing current application
 * connectivity and Moonraker configuration.
 */
void files_page_controller_reload(
    bool wifi_connected,
    bool moonraker_connected,
    bool sd_available,
    const char *host,
    int port,
    const char *api_key);

/* UI bridge: enqueue a row only when it enters the visible viewport. */
void files_page_controller_request_preview(const char *path);
void files_page_controller_set_search(const char *query);
void files_page_controller_cycle_sort(void);
void files_page_controller_open_folder(const char *path);
void files_page_controller_up_folder(void);

#ifdef __cplusplus
}
#endif
