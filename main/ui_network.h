#pragma once

#include <stdbool.h>
#include "lvgl.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"

void ui_network_show(void);
void ui_network_hide(void);

void ui_network_create_objects(
    const char *banner_text,
    int moonraker_port,
    lv_event_cb_t wifi_clicked_cb,
    lv_event_cb_t host_clicked_cb);

void ui_network_refresh_objects(
    const char *banner_text,
    const char *wifi_text,
    const char *ip_text,
    bool moonraker_connected,
    const char *host_text,
    int http_code,
    const char *scan_status_text);

void ui_network_destroy_objects(void);

void ui_network_set_port(int port);

void ui_network_set_scan_status(
    const char *status_text);

void ui_network_render_scan_results(
    const wifi_ap_record_t *aps,
    uint16_t count,
    unsigned total_count,
    lv_event_cb_t selected_cb);
