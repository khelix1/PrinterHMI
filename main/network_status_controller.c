#include "network_status_controller.h"
#include "ui_text.h"

#include "ui_network.h"

void network_status_controller_refresh(
    const char *banner_text,
    bool got_ip,
    const char *connected_ssid,
    const char *configured_ssid,
    const char *ip_text,
    bool moonraker_ok,
    const char *moonraker_host,
    int moonraker_http_code,
    const char *scan_status)
{
    const char *wifi_text = got_ip
        ? (connected_ssid && connected_ssid[0] ? connected_ssid : "--")
        : (configured_ssid && configured_ssid[0] ? configured_ssid : "--");

    ui_network_refresh_objects(
        banner_text ? banner_text : "NETWORK OFFLINE",
        wifi_text,
        ip_text ? ip_text : "--",
        moonraker_ok,
        moonraker_host ? moonraker_host : "",
        moonraker_http_code,
        scan_status ? scan_status : "");
}

void network_status_controller_update_topbar(
    lv_obj_t *wifi_label,
    const char *wifi_status)
{
    if (!wifi_label) {
        return;
    }

    lv_label_set_text(
        wifi_label,
        wifi_status ? wifi_status : ui_text(""));
}
