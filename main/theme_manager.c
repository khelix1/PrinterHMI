#include "theme_manager.h"

#include "esp_err.h"
#include "esp_log.h"
#include "nvs.h"

#include <stdint.h>

#define THEME_NVS_NAMESPACE "ui_theme"
#define THEME_NVS_ACTIVE_KEY "active"
#define THEME_NVS_ACCENT_KEY "accent"
#define THEME_NVS_DENSITY_KEY "density"
#define THEME_NVS_LARGE_TEXT_KEY "large_text"
#define THEME_NVS_CONTRAST_KEY "contrast"
#define THEME_NVS_SOLID_KEY "solid_glass"
#define THEME_NVS_MOTION_KEY "reduce_motion"

static const char *TAG = "theme_manager";

static bool save_u8(const char *key, uint8_t value)
{
    nvs_handle_t handle;
    esp_err_t error = nvs_open(
        THEME_NVS_NAMESPACE,
        NVS_READWRITE,
        &handle);

    if (error != ESP_OK) return false;

    error = nvs_set_u8(handle, key, value);
    if (error == ESP_OK) error = nvs_commit(handle);
    nvs_close(handle);

    if (error != ESP_OK) {
        ESP_LOGE(TAG, "Could not save %s: %s", key, esp_err_to_name(error));
        return false;
    }

    return true;
}

static uint8_t load_u8(nvs_handle_t handle,
                       const char *key,
                       uint8_t fallback)
{
    uint8_t value = fallback;
    if (nvs_get_u8(handle, key, &value) != ESP_OK) return fallback;
    return value;
}

static bool theme_valid(ui_theme_id_t theme)
{
    return theme == UI_THEME_CLASSIC ||
           theme == UI_THEME_OPERATOR ||
           theme == UI_THEME_GLASS;
}

static const char *theme_label(ui_theme_id_t theme)
{
    switch (theme) {
        case UI_THEME_CLASSIC:
            return "FOUNDRY (THEME A)";
        case UI_THEME_GLASS:
            return "DARK GLASS (THEME C)";
        case UI_THEME_OPERATOR:
        default:
            return "OPERATOR (THEME B)";
    }
}

void theme_manager_init(void)
{
    uint8_t saved_theme = (uint8_t)UI_THEME_OPERATOR;
    nvs_handle_t handle;

    esp_err_t error = nvs_open(
        THEME_NVS_NAMESPACE,
        NVS_READONLY,
        &handle);

    ui_accent_id_t accent = UI_ACCENT_DEFAULT;
    ui_density_id_t density = UI_DENSITY_COMFORTABLE;
    ui_accessibility_t accessibility = {0};

    if (error == ESP_OK) {
        error = nvs_get_u8(
            handle,
            THEME_NVS_ACTIVE_KEY,
            &saved_theme);
        accent = (ui_accent_id_t)load_u8(
            handle, THEME_NVS_ACCENT_KEY, UI_ACCENT_DEFAULT);
        density = (ui_density_id_t)load_u8(
            handle, THEME_NVS_DENSITY_KEY, UI_DENSITY_COMFORTABLE);
        accessibility.large_text = load_u8(
            handle, THEME_NVS_LARGE_TEXT_KEY, 0) != 0;
        accessibility.high_contrast = load_u8(
            handle, THEME_NVS_CONTRAST_KEY, 0) != 0;
        accessibility.reduced_transparency = load_u8(
            handle, THEME_NVS_SOLID_KEY, 0) != 0;
        accessibility.reduced_motion = load_u8(
            handle, THEME_NVS_MOTION_KEY, 0) != 0;
        nvs_close(handle);
    }

    ui_theme_id_t theme = (ui_theme_id_t)saved_theme;

    if (error != ESP_OK || !theme_valid(theme)) {
        theme = UI_THEME_OPERATOR;

        if (error != ESP_OK && error != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(TAG,
                     "Using default theme: %s",
                     esp_err_to_name(error));
        }
    }

    ui_theme_set_active(theme);
    ui_theme_set_accent(accent);
    ui_theme_set_density(density);
    ui_theme_set_accessibility(accessibility);

    ESP_LOGI(TAG,
             "Applied theme: %s",
             theme_label(theme));
}

ui_theme_id_t theme_manager_active(void)
{
    return ui_theme_get_active();
}

const char *theme_manager_active_label(void)
{
    return theme_label(ui_theme_get_active());
}

bool theme_manager_select(ui_theme_id_t theme)
{
    if (!theme_valid(theme)) {
        return false;
    }

    nvs_handle_t handle;
    esp_err_t error = nvs_open(
        THEME_NVS_NAMESPACE,
        NVS_READWRITE,
        &handle);

    if (error != ESP_OK) {
        ESP_LOGE(TAG,
                 "Could not open theme storage: %s",
                 esp_err_to_name(error));
        return false;
    }

    error = nvs_set_u8(
        handle,
        THEME_NVS_ACTIVE_KEY,
        (uint8_t)theme);

    if (error == ESP_OK) {
        error = nvs_commit(handle);
    }

    nvs_close(handle);

    if (error != ESP_OK) {
        ESP_LOGE(TAG,
                 "Could not save theme: %s",
                 esp_err_to_name(error));
        return false;
    }

    ui_theme_set_active(theme);

    ESP_LOGI(TAG,
             "Selected theme: %s",
             theme_label(theme));
    return true;
}

ui_accent_id_t theme_manager_accent(void)
{
    return ui_theme_get_accent();
}

const char *theme_manager_accent_name(ui_accent_id_t accent)
{
    static const char *names[] = {
        "THEME DEFAULT", "CYAN", "BLUE", "GREEN", "AMBER", "VIOLET"
    };
    return accent < UI_ACCENT_COUNT ? names[accent] : names[0];
}

const char *theme_manager_accent_label(void)
{
    return theme_manager_accent_name(theme_manager_accent());
}

bool theme_manager_select_accent(ui_accent_id_t accent)
{
    if (accent >= UI_ACCENT_COUNT ||
        !save_u8(THEME_NVS_ACCENT_KEY, (uint8_t)accent)) return false;
    ui_theme_set_accent(accent);
    return true;
}

ui_density_id_t theme_manager_density(void)
{
    return ui_theme_get_density();
}

const char *theme_manager_density_name(ui_density_id_t density)
{
    static const char *names[] = {"COMPACT", "COMFORTABLE", "SPACIOUS"};
    return density < UI_DENSITY_COUNT ? names[density] : names[1];
}

const char *theme_manager_density_label(void)
{
    return theme_manager_density_name(theme_manager_density());
}

bool theme_manager_select_density(ui_density_id_t density)
{
    if (density >= UI_DENSITY_COUNT ||
        !save_u8(THEME_NVS_DENSITY_KEY, (uint8_t)density)) return false;
    ui_theme_set_density(density);
    return true;
}

ui_accessibility_t theme_manager_accessibility(void)
{
    return ui_theme_get_accessibility();
}

const char *theme_manager_accessibility_label(void)
{
    ui_accessibility_t value = theme_manager_accessibility();
    unsigned count = value.large_text + value.high_contrast +
        value.reduced_transparency + value.reduced_motion;
    static const char *labels[] = {
        "STANDARD", "1 OPTION", "2 OPTIONS", "3 OPTIONS", "4 OPTIONS"
    };
    return labels[count <= 4 ? count : 4];
}

bool theme_manager_set_accessibility(ui_accessibility_t options)
{
    if (!save_u8(THEME_NVS_LARGE_TEXT_KEY, options.large_text) ||
        !save_u8(THEME_NVS_CONTRAST_KEY, options.high_contrast) ||
        !save_u8(THEME_NVS_SOLID_KEY, options.reduced_transparency) ||
        !save_u8(THEME_NVS_MOTION_KEY, options.reduced_motion)) return false;
    ui_theme_set_accessibility(options);
    return true;
}
