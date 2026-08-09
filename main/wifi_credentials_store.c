#include "wifi_credentials_store.h"

#include "nvs.h"
#include "esp_err.h"
#include "esp_log.h"

static const char *TAG = "PrinterHMI";

bool wifi_credentials_store_load(char *ssid, size_t ssid_size,
                                 char *password, size_t password_size)
{
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
        nvs_close(h);
        ESP_LOGW(TAG, "NVS WiFi load: no saved SSID");
        return false;
    }

    err = nvs_get_str(h, "pass", password, &pass_len);
    if (err != ESP_OK) {
        password[0] = 0;
    }
    nvs_close(h);
    ESP_LOGI(TAG, "NVS WiFi load: saved credentials available");
    return true;
}

bool wifi_credentials_store_save(const char *ssid, const char *password)
{
    if (!ssid || !ssid[0]) return false;
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
