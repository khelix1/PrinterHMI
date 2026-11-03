#pragma once

#include <stdbool.h>
#include <string.h>
#include "esp_wifi.h"

/* ESP station fields are fixed-width byte arrays, not C string buffers.
 * Preserve all 32 SSID / 64 password bytes; reject longer inputs. */
static inline bool wifi_station_config_build(
    wifi_config_t *config, const char *ssid, const char *password)
{
    if (!config) return false;
    memset(config, 0, sizeof(*config));
    if (!ssid || !ssid[0]) return false;
    if (!password) password = "";
    size_t ssid_size = strnlen(ssid, sizeof(config->sta.ssid) + 1);
    size_t password_size = strnlen(password, sizeof(config->sta.password) + 1);
    if (ssid_size > sizeof(config->sta.ssid) ||
        password_size > sizeof(config->sta.password)) return false;
    memcpy(config->sta.ssid, ssid, ssid_size);
    memcpy(config->sta.password, password, password_size);
    return true;
}
