#include "theme_manager.h"

#include "custom_theme.h"
#include "esp_err.h"
#include "esp_log.h"
#include "nvs.h"

#include <stdint.h>
#include <string.h>

#define THEME_NVS_NAMESPACE "ui_theme"
#define THEME_NVS_ACTIVE_KEY "active"
#define THEME_NVS_ACCENT_KEY "accent"
#define THEME_NVS_DENSITY_KEY "density"
#define THEME_NVS_LARGE_TEXT_KEY "large_text"
#define THEME_NVS_CONTRAST_KEY "contrast"
#define THEME_NVS_SOLID_KEY "solid_glass"
#define THEME_NVS_MOTION_KEY "reduce_motion"
#define THEME_NVS_CUSTOM_KEY "custom_id"

static const char *TAG = "theme_manager";
static char s_pending_custom_id[CUSTOM_THEME_ID_MAX + 1] = "";

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

static void load_custom_id(nvs_handle_t handle)
{
    size_t length = sizeof(s_pending_custom_id);
    s_pending_custom_id[0] = 0;
    if (nvs_get_str(
            handle,
            THEME_NVS_CUSTOM_KEY,
            s_pending_custom_id,
            &length) != ESP_OK) {
        s_pending_custom_id[0] = 0;
    }
}

static bool theme_valid(ui_theme_id_t theme)
{
    return (unsigned)theme >= (unsigned)UI_THEME_CLASSIC &&
           (unsigned)theme <= (unsigned)UI_THEME_OPERATOR_SHELL;
}

static const char *theme_label(ui_theme_id_t theme)
{
    switch (theme) {
        case UI_THEME_CLASSIC:
            return "FOUNDRY (THEME A)";
        case UI_THEME_GLASS:
            return "DARK GLASS (THEME C)";
        case UI_THEME_OPERATOR_SHELL:
            return "OPERATOR SHELL (LAYOUT)";
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
        load_custom_id(handle);
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
    if (custom_theme_is_active()) {
        return custom_theme_active_name();
    }
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
        error = nvs_erase_key(
            handle,
            THEME_NVS_CUSTOM_KEY);
        if (error == ESP_ERR_NVS_NOT_FOUND) {
            error = ESP_OK;
        }
    }

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

    custom_theme_deactivate();
    s_pending_custom_id[0] = 0;
    ui_theme_set_active(theme);

    ESP_LOGI(TAG,
             "Selected theme: %s",
             theme_label(theme));
    return true;
}

size_t theme_manager_scan_custom_themes(void)
{
    size_t count = custom_theme_scan_sd();

    if (s_pending_custom_id[0] &&
        custom_theme_activate_id(
            s_pending_custom_id)) {
        ESP_LOGI(TAG,
                 "Restored custom theme: %s",
                 custom_theme_active_name());
    } else if (s_pending_custom_id[0]) {
        ESP_LOGW(TAG,
                 "Saved custom theme unavailable: %s",
                 s_pending_custom_id);
    }

    return count;
}

size_t theme_manager_custom_count(void)
{
    return custom_theme_count();
}

const custom_theme_summary_t *
theme_manager_custom_summary(size_t index)
{
    return custom_theme_summary(index);
}

bool theme_manager_select_custom(size_t index)
{
    const custom_theme_summary_t *summary =
        custom_theme_summary(index);
    if (!summary) return false;

    nvs_handle_t handle;
    esp_err_t error = nvs_open(
        THEME_NVS_NAMESPACE,
        NVS_READWRITE,
        &handle);
    if (error != ESP_OK) return false;

    error = nvs_set_u8(
        handle,
        THEME_NVS_ACTIVE_KEY,
        (uint8_t)summary->base_theme);
    if (error == ESP_OK) {
        error = nvs_set_str(
            handle,
            THEME_NVS_CUSTOM_KEY,
            summary->id);
    }
    if (error == ESP_OK) error = nvs_commit(handle);
    nvs_close(handle);
    if (error != ESP_OK) {
        ESP_LOGE(TAG,
                 "Could not save custom theme: %s",
                 esp_err_to_name(error));
        return false;
    }

    if (!custom_theme_activate(index)) {
        return false;
    }
    strlcpy(
        s_pending_custom_id,
        summary->id,
        sizeof(s_pending_custom_id));
    return true;
}

bool theme_manager_select_custom_id(const char *id)
{
    if (!id || !id[0]) return false;
    for (size_t index = 0;
         index < custom_theme_count();
         ++index) {
        const custom_theme_summary_t *summary =
            custom_theme_summary(index);
        if (summary &&
            strcmp(summary->id, id) == 0) {
            return theme_manager_select_custom(index);
        }
    }
    return false;
}

bool theme_manager_remove_custom(size_t index)
{
    const custom_theme_summary_t *summary =
        custom_theme_summary(index);
    if (!summary) return false;

    bool removing_active =
        custom_theme_is_active() &&
        strcmp(custom_theme_active_id(),
               summary->id) == 0;

    if (removing_active &&
        !theme_manager_select(UI_THEME_OPERATOR)) {
        return false;
    }

    return custom_theme_remove(index);
}

bool theme_manager_custom_active(void)
{
    return custom_theme_is_active();
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
