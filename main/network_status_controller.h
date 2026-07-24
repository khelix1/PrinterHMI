#pragma once

#include <stdbool.h>

/*
 * Push the current application network snapshot into the Network page.
 *
 * This controller owns presentation formatting only. WiFi, IP address,
 * Moonraker transport, and persistence remain application-owned.
 */
void network_status_controller_refresh(
    const char *banner_text,
    bool got_ip,
    const char *connected_ssid,
    const char *selected_ssid,
    const char *ip_text,
    bool moonraker_ok,
    const char *moonraker_host,
    int moonraker_http_code,
    const char *scan_status);
