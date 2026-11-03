#include "wifi_credentials_store.h"

#include <string.h>

#include "nvs.h"
#include "esp_err.h"
#include "esp_log.h"

static const char *TAG = "PrinterHMI";

bool wifi_credentials_store_load(char *ssid, size_t ssid_size,
                                 char *password, size_t password_size)
{
    /* Never leave a previous printer network or password in caller buffers. */
    if (ssid && ssid_size) memset(ssid, 0, ssid_size);
    if (password && password_size) memset(password, 0, password_size);
    if (!ssid || ssid_size == 0 || !password || password_size == 0) {
        return false;
    }

    nvs_handle_t h;
    esp_err_t err = nvs_open("netcfg", NVS_READONLY, &h);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS WiFi load: no netcfg namespace");
        return false;
    }

    size_t ssid_len = ssid_size;
    size_t pass_len = password_size;
    err = nvs_get_str(h, "ssid", ssid, &ssid_len);
    if (err != ESP_OK || ssid[0] == 0) {
        memset(ssid, 0, ssid_size);
        nvs_close(h);
        ESP_LOGW(TAG, "NVS WiFi load: no saved SSID");
        return false;
    }

    err = nvs_get_str(h, "pass", password, &pass_len);
    nvs_close(h);
    if (err != ESP_OK) {
        memset(ssid, 0, ssid_size);
        memset(password, 0, password_size);
        ESP_LOGW(TAG, "NVS WiFi load: saved password unavailable");
        return false;
    }
    ESP_LOGI(TAG, "NVS WiFi load: saved credentials available");
    return true;
}

bool wifi_credentials_store_save(const char *ssid, const char *password)
{
    if (!ssid || !ssid[0] ||
        strnlen(ssid, 33) > 32 ||
        (password && strnlen(password, 65) > 64)) return false;
    if (!password) password = "";

    nvs_handle_t h;
    esp_err_t err = nvs_open("netcfg", NVS_READWRITE, &h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS WiFi save open failed: %s", esp_err_to_name(err));
        return false;
    }
    err = nvs_set_str(h, "ssid", ssid);
    if (err == ESP_OK) err = nvs_set_str(h, "pass", password);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS WiFi save failed: %s", esp_err_to_name(err));
        return false;
    }
    ESP_LOGI(TAG, "NVS WiFi saved");
    return true;
}
