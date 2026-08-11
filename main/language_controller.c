#include "language_controller.h"

#include "ui_i18n_registry.h"

#include "esp_log.h"
#include "nvs.h"

#define LANGUAGE_NVS_NAMESPACE "ui_language"
#define LANGUAGE_NVS_KEY "active"

static const char *TAG = "language";
static ui_language_id_t s_active = UI_LANGUAGE_ENGLISH;

bool language_controller_init(void)
{
    nvs_handle_t handle;
    if (nvs_open(LANGUAGE_NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
        return true;
    }
    uint8_t saved = UI_LANGUAGE_ENGLISH;
    esp_err_t err = nvs_get_u8(handle, LANGUAGE_NVS_KEY, &saved);
    nvs_close(handle);
    if (err == ESP_OK && saved < UI_LANGUAGE_COUNT &&
        ui_i18n_language_is_selectable((ui_language_id_t)saved)) {
        s_active = (ui_language_id_t)saved;
    }
    return true;
}

ui_language_id_t language_controller_active(void)
{
    return s_active;
}

bool language_controller_select(ui_language_id_t language)
{
    if (language >= UI_LANGUAGE_COUNT ||
        !ui_i18n_language_is_selectable(language)) return false;
    nvs_handle_t handle;
    if (nvs_open(LANGUAGE_NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) {
        ESP_LOGE(TAG, "Could not open language preferences");
        return false;
    }
    esp_err_t err = nvs_set_u8(handle, LANGUAGE_NVS_KEY, (uint8_t)language);
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    if (err != ESP_OK) return false;
    s_active = language;
    return true;
}

const char *language_controller_native_name(ui_language_id_t language)
{
    switch (language) {
        case UI_LANGUAGE_ENGLISH: return "English";
        case UI_LANGUAGE_SPANISH: return "Español";
        default: return "English";
    }
}
