#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef void (*printer_file_controller_popup_cb_t)(void);

void printer_file_controller_select_file(
    const char *path,
    printer_file_controller_popup_cb_t show_detail_popup);

bool printer_file_controller_start_file(
    bool network_ready,
    const char *host,
    int port,
    const char *api_key,
    const char *filename,
    char *status_text,
    size_t status_text_size);

bool printer_file_controller_start_selected_file(
    bool network_ready,
    const char *host,
    int port,
    const char *api_key,
    char *status_text,
    size_t status_text_size,
    printer_file_controller_popup_cb_t close_detail_popup);
