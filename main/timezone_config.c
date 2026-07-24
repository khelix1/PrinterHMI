#include "timezone_config.h"

#include "esp_err.h"
#include "esp_log.h"
#include "nvs.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#define TIMEZONE_NVS_NAMESPACE "time_cfg"
#define TIMEZONE_NVS_ZONE_KEY "zone_id"
#define TIMEZONE_ID_MAX 24

static const char *TAG = "timezone_config";

static const timezone_config_entry_t s_timezones[] = {
    {"utc", "UTC", "UTC", "UTC0"},
    {"us_atlantic", "Atlantic Time (US/Canada)", "AST / ADT",
     "AST4ADT,M3.2.0/2,M11.1.0/2"},
    {"us_eastern", "Eastern Time (US/Canada)", "EST / EDT",
     "EST5EDT,M3.2.0/2,M11.1.0/2"},
    {"us_central", "Central Time (US/Canada)", "CST / CDT",
     "CST6CDT,M3.2.0/2,M11.1.0/2"},
    {"us_mountain", "Mountain Time (US/Canada)", "MST / MDT",
     "MST7MDT,M3.2.0/2,M11.1.0/2"},
    {"us_arizona", "Arizona", "MST", "MST7"},
    {"us_pacific", "Pacific Time (US/Canada)", "PST / PDT",
     "PST8PDT,M3.2.0/2,M11.1.0/2"},
    {"us_alaska", "Alaska", "AKST / AKDT",
     "AKST9AKDT,M3.2.0/2,M11.1.0/2"},
    {"us_hawaii", "Hawaii", "HST", "HST10"},
    {"uk", "United Kingdom", "GMT / BST",
     "GMT0BST,M3.5.0/1,M10.5.0/2"},
    {"europe_central", "Central Europe", "CET / CEST",
     "CET-1CEST,M3.5.0/2,M10.5.0/3"},
    {"india", "India", "IST", "IST-5:30"},
    {"china", "China", "CST", "CST-8"},
    {"japan", "Japan", "JST", "JST-9"},
    {"australia_eastern", "Australia Eastern", "AEST / AEDT",
     "AEST-10AEDT,M10.1.0/2,M4.1.0/3"},
    {"new_zealand", "New Zealand", "NZST / NZDT",
     "NZST-12NZDT,M9.5.0/2,M4.1.0/3"},
};

static const size_t s_default_index = 3;
static size_t s_selected_index = 3;

static size_t timezone_count_internal(void)
{
    return sizeof(s_timezones) / sizeof(s_timezones[0]);
}

static bool timezone_apply(size_t index)
{
    if (index >= timezone_count_internal()) {
        return false;
    }

    if (setenv("TZ", s_timezones[index].posix_tz, 1) != 0) {
        ESP_LOGE(TAG, "Could not set TZ environment");
        return false;
    }

    tzset();
    s_selected_index = index;

    ESP_LOGI(TAG,
             "Applied timezone: %s (%s)",
             s_timezones[index].label,
             s_timezones[index].posix_tz);
    return true;
}

static size_t timezone_find_id(const char *id)
{
    if (!id || !id[0]) {
        return s_default_index;
    }

    for (size_t index = 0;
         index < timezone_count_internal();
         ++index) {
        if (strcmp(id, s_timezones[index].id) == 0) {
            return index;
        }
    }

    return s_default_index;
}

void timezone_config_init(void)
{
    char saved_id[TIMEZONE_ID_MAX] = "";
    nvs_handle_t handle;

    esp_err_t error = nvs_open(
        TIMEZONE_NVS_NAMESPACE,
        NVS_READONLY,
        &handle);

    if (error == ESP_OK) {
        size_t length = sizeof(saved_id);
        error = nvs_get_str(
            handle,
            TIMEZONE_NVS_ZONE_KEY,
            saved_id,
            &length);
        nvs_close(handle);
    }

    size_t index = error == ESP_OK
        ? timezone_find_id(saved_id)
        : s_default_index;

    if (error != ESP_OK && error != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG,
                 "Using default timezone: %s",
                 esp_err_to_name(error));
    }

    (void)timezone_apply(index);
}

size_t timezone_config_count(void)
{
    return timezone_count_internal();
}

const timezone_config_entry_t *timezone_config_entry(size_t index)
{
    if (index >= timezone_count_internal()) {
        return NULL;
    }

    return &s_timezones[index];
}

size_t timezone_config_selected_index(void)
{
    return s_selected_index;
}

const char *timezone_config_selected_label(void)
{
    return s_timezones[s_selected_index].label;
}

bool timezone_config_select(size_t index)
{
    if (index >= timezone_count_internal()) {
        return false;
    }

    nvs_handle_t handle;
    esp_err_t error = nvs_open(
        TIMEZONE_NVS_NAMESPACE,
        NVS_READWRITE,
        &handle);

    if (error != ESP_OK) {
        ESP_LOGE(TAG,
                 "Could not open timezone storage: %s",
                 esp_err_to_name(error));
        return false;
    }

    error = nvs_set_str(
        handle,
        TIMEZONE_NVS_ZONE_KEY,
        s_timezones[index].id);

    if (error == ESP_OK) {
        error = nvs_commit(handle);
    }

    nvs_close(handle);

    if (error != ESP_OK) {
        ESP_LOGE(TAG,
                 "Could not save timezone: %s",
                 esp_err_to_name(error));
        return false;
    }

    return timezone_apply(index);
}
