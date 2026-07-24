#pragma once

#include "lvgl.h"

typedef void (*ui_ota_start_cb_t)(const char *url);
typedef void (*ui_ota_remote_cb_t)(void);
typedef void (*ui_ota_cancel_cb_t)(void);

void ui_ota_popup_show(const char *current_url,
                       const char *firmware_info,
                       size_t max_url_len,
                       ui_ota_start_cb_t start_cb,
                       ui_ota_remote_cb_t remote_cb);

void ui_ota_popup_close(void);

void ui_ota_progress_show(ui_ota_cancel_cb_t cancel_cb);
void ui_ota_progress_close(void);
void ui_ota_progress_pump(const char *status_text,
                          int percent,
                          int bytes_read,
                          int content_length,
                          bool cancel_enabled);
void ui_ota_remote_builds_placeholder_show(void);
