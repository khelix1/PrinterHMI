#include "network_status_controller.h"

#include <stdio.h>

#include "ui_network_v32.h"

void network_status_controller_refresh(
    const char *banner_text,
    bool got_ip,
    const char *connected_ssid,
    const char *selected_ssid,
    const char *ip_text,
    bool moonraker_ok,
    const char *moonraker_host,
    int moonraker_http_code,
    const char *scan_status)
{
    char wifi_buf[64];

    const char *wifi_text = "OFFLINE";

    if (got_ip && connected_ssid && connected_ssid[0]) {
        wifi_text = connected_ssid;
    } else if (got_ip) {
        wifi_text = "CONNECTED";
    } else if (selected_ssid && selected_ssid[0]) {
        snprintf(
            wifi_buf,
            sizeof(wifi_buf),
            "TRYING %.24s",
            selected_ssid);

        wifi_text = wifi_buf;
    }

    ui_network_v32_refresh_objects(
        banner_text ? banner_text : "NETWORK OFFLINE",
        wifi_text,
        ip_text ? ip_text : "--",
        moonraker_ok,
        moonraker_host ? moonraker_host : "",
        moonraker_http_code,
        scan_status ? scan_status : "");
}
