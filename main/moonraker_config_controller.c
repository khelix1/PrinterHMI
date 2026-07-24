#include "moonraker_config_controller.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_err.h"
#include "esp_log.h"
#include "nvs.h"

#define DEFAULT_MOONRAKER_HOST "moonraker.local"
#define DEFAULT_MOONRAKER_PORT 7125

#define MOONRAKER_NVS_NAMESPACE "netcfg"
#define MOONRAKER_SCHEMA_KEY    "mp_schema"
#define MOONRAKER_ACTIVE_KEY    "mp_active"
#define MOONRAKER_SCHEMA_VALUE  1

static const char *TAG = "moonraker_config";

static moonraker_profile_t
    s_profiles[MOONRAKER_CONFIG_MAX_PROFILES];

static int s_active_profile = 0;

/*
 * A 32-bit read/write is atomic on the ESP32-P4. Volatile ensures the
 * polling task observes profile changes made by the LVGL task.
 */
static volatile uint32_t s_configuration_generation = 1;


static void advance_configuration_generation(void)
{
    ++s_configuration_generation;

    if (s_configuration_generation == 0) {
        s_configuration_generation = 1;
    }
}


static void copy_text(
    char *destination,
    size_t destination_size,
    const char *source)
{
    if (!destination || destination_size == 0) {
        return;
    }

    if (!source) {
        source = "";
    }

    strlcpy(
        destination,
        source,
        destination_size);
}


static bool valid_port(int port)
{
    return port > 0 && port < 65536;
}


static bool valid_profile_index(int profile_index)
{
    return profile_index >= 0 &&
           profile_index < MOONRAKER_CONFIG_MAX_PROFILES;
}


static void profile_key(
    char *output,
    size_t output_size,
    int profile_index,
    const char *field)
{
    snprintf(
        output,
        output_size,
        "p%d_%s",
        profile_index,
        field);
}


static void default_profile_name(
    char *output,
    size_t output_size,
    int profile_index)
{
    snprintf(
        output,
        output_size,
        "Printer %d",
        profile_index + 1);
}


static void clear_profile(
    moonraker_profile_t *profile)
{
    if (!profile) {
        return;
    }

    memset(
        profile,
        0,
        sizeof(*profile));

    profile->port =
        DEFAULT_MOONRAKER_PORT;
}


static void install_default_profile(void)
{
    for (int index = 0;
         index < MOONRAKER_CONFIG_MAX_PROFILES;
         ++index) {
        clear_profile(
            &s_profiles[index]);
    }

    s_profiles[0].configured = true;

    default_profile_name(
        s_profiles[0].name,
        sizeof(s_profiles[0].name),
        0);

    copy_text(
        s_profiles[0].host,
        sizeof(s_profiles[0].host),
        DEFAULT_MOONRAKER_HOST);

    s_profiles[0].port =
        DEFAULT_MOONRAKER_PORT;

    s_active_profile = 0;
}


static int first_configured_profile(void)
{
    for (int index = 0;
         index < MOONRAKER_CONFIG_MAX_PROFILES;
         ++index) {
        if (s_profiles[index].configured) {
            return index;
        }
    }

    return -1;
}


static bool profile_valid(
    const moonraker_profile_t *profile)
{
    return profile &&
           profile->configured &&
           profile->name[0] &&
           profile->host[0] &&
           valid_port(profile->port);
}


static esp_err_t persist_profile(
    nvs_handle_t handle,
    int profile_index,
    const moonraker_profile_t *profile)
{
    char key[16];

    profile_key(
        key,
        sizeof(key),
        profile_index,
        "used");

    esp_err_t error =
        nvs_set_u8(
            handle,
            key,
            profile && profile->configured
                ? 1
                : 0);

    if (error != ESP_OK ||
        !profile ||
        !profile->configured) {
        return error;
    }

    profile_key(
        key,
        sizeof(key),
        profile_index,
        "name");

    error =
        nvs_set_str(
            handle,
            key,
            profile->name);

    if (error != ESP_OK) {
        return error;
    }

    profile_key(
        key,
        sizeof(key),
        profile_index,
        "host");

    error =
        nvs_set_str(
            handle,
            key,
            profile->host);

    if (error != ESP_OK) {
        return error;
    }

    profile_key(
        key,
        sizeof(key),
        profile_index,
        "port");

    return nvs_set_i32(
        handle,
        key,
        profile->port);
}


static bool persist_collection(void)
{
    if (!valid_profile_index(s_active_profile) ||
        !profile_valid(
            &s_profiles[s_active_profile])) {
        return false;
    }

    nvs_handle_t handle;

    esp_err_t error =
        nvs_open(
            MOONRAKER_NVS_NAMESPACE,
            NVS_READWRITE,
            &handle);

    if (error != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Profile NVS open failed: %s",
            esp_err_to_name(error));

        return false;
    }

    error =
        nvs_set_u8(
            handle,
            MOONRAKER_SCHEMA_KEY,
            MOONRAKER_SCHEMA_VALUE);

    if (error == ESP_OK) {
        error =
            nvs_set_u8(
                handle,
                MOONRAKER_ACTIVE_KEY,
                (uint8_t)s_active_profile);
    }

    for (int index = 0;
         error == ESP_OK &&
         index < MOONRAKER_CONFIG_MAX_PROFILES;
         ++index) {
        error =
            persist_profile(
                handle,
                index,
                &s_profiles[index]);
    }

    /*
     * Mirror the active endpoint into the legacy keys. This preserves
     * rollback compatibility with firmware released before profiles.
     */
    if (error == ESP_OK) {
        error =
            nvs_set_str(
                handle,
                "moon_host",
                s_profiles[s_active_profile].host);
    }

    if (error == ESP_OK) {
        error =
            nvs_set_i32(
                handle,
                "moon_port",
                s_profiles[s_active_profile].port);
    }

    if (error == ESP_OK) {
        error =
            nvs_commit(handle);
    }

    nvs_close(handle);

    if (error != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Profile NVS save failed: %s",
            esp_err_to_name(error));

        return false;
    }

    return true;
}


static bool load_profile(
    nvs_handle_t handle,
    int profile_index)
{
    char key[16];
    uint8_t configured = 0;

    profile_key(
        key,
        sizeof(key),
        profile_index,
        "used");

    if (nvs_get_u8(
            handle,
            key,
            &configured) != ESP_OK ||
        !configured) {
        clear_profile(
            &s_profiles[profile_index]);

        return false;
    }

    moonraker_profile_t loaded = {
        .configured = true,
        .port = DEFAULT_MOONRAKER_PORT,
    };

    size_t name_length =
        sizeof(loaded.name);

    profile_key(
        key,
        sizeof(key),
        profile_index,
        "name");

    if (nvs_get_str(
            handle,
            key,
            loaded.name,
            &name_length) != ESP_OK ||
        !loaded.name[0]) {
        default_profile_name(
            loaded.name,
            sizeof(loaded.name),
            profile_index);
    }

    size_t host_length =
        sizeof(loaded.host);

    profile_key(
        key,
        sizeof(key),
        profile_index,
        "host");

    if (nvs_get_str(
            handle,
            key,
            loaded.host,
            &host_length) != ESP_OK ||
        !loaded.host[0]) {
        clear_profile(
            &s_profiles[profile_index]);

        return false;
    }

    int32_t loaded_port =
        DEFAULT_MOONRAKER_PORT;

    profile_key(
        key,
        sizeof(key),
        profile_index,
        "port");

    if (nvs_get_i32(
            handle,
            key,
            &loaded_port) == ESP_OK &&
        valid_port((int)loaded_port)) {
        loaded.port =
            (int)loaded_port;
    }

    if (!profile_valid(&loaded)) {
        clear_profile(
            &s_profiles[profile_index]);

        return false;
    }

    s_profiles[profile_index] =
        loaded;

    return true;
}


static bool migrate_legacy_config(
    nvs_handle_t handle)
{
    install_default_profile();

    char legacy_host[
        MOONRAKER_CONFIG_HOST_LENGTH] = "";

    size_t host_length =
        sizeof(legacy_host);

    esp_err_t host_error =
        nvs_get_str(
            handle,
            "moon_host",
            legacy_host,
            &host_length);

    int32_t legacy_port =
        DEFAULT_MOONRAKER_PORT;

    esp_err_t port_error =
        nvs_get_i32(
            handle,
            "moon_port",
            &legacy_port);

    if (host_error == ESP_OK &&
        legacy_host[0]) {
        copy_text(
            s_profiles[0].host,
            sizeof(s_profiles[0].host),
            legacy_host);
    }

    if (port_error == ESP_OK &&
        valid_port((int)legacy_port)) {
        s_profiles[0].port =
            (int)legacy_port;
    }

    ESP_LOGI(
        TAG,
        "Migrating legacy Moonraker endpoint to "
        "profile 0: %s:%d",
        s_profiles[0].host,
        s_profiles[0].port);

    return persist_collection();
}


bool moonraker_config_load(void)
{
    install_default_profile();

    nvs_handle_t handle;

    esp_err_t error =
        nvs_open(
            MOONRAKER_NVS_NAMESPACE,
            NVS_READWRITE,
            &handle);

    if (error != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Profile NVS load open failed: %s",
            esp_err_to_name(error));

        return false;
    }

    uint8_t schema = 0;

    error =
        nvs_get_u8(
            handle,
            MOONRAKER_SCHEMA_KEY,
            &schema);

    if (error != ESP_OK ||
        schema != MOONRAKER_SCHEMA_VALUE) {
        bool migrated =
            migrate_legacy_config(handle);

        nvs_close(handle);
        return migrated;
    }

    for (int index = 0;
         index < MOONRAKER_CONFIG_MAX_PROFILES;
         ++index) {
        load_profile(
            handle,
            index);
    }

    uint8_t loaded_active = 0;

    if (nvs_get_u8(
            handle,
            MOONRAKER_ACTIVE_KEY,
            &loaded_active) == ESP_OK &&
        valid_profile_index((int)loaded_active) &&
        profile_valid(
            &s_profiles[loaded_active])) {
        s_active_profile =
            (int)loaded_active;
    } else {
        int first =
            first_configured_profile();

        if (first >= 0) {
            s_active_profile = first;
        }
    }

    nvs_close(handle);

    if (first_configured_profile() < 0) {
        ESP_LOGW(
            TAG,
            "No valid profiles found; restoring default");

        install_default_profile();
        return persist_collection();
    }

    ESP_LOGI(
        TAG,
        "Loaded %u printer profile(s); active=%d %s:%d",
        (unsigned)moonraker_config_profile_count(),
        s_active_profile,
        moonraker_config_host(),
        moonraker_config_port());

    return true;
}


const char *moonraker_config_host(void)
{
    return
        s_profiles[s_active_profile].host;
}


int moonraker_config_port(void)
{
    return
        s_profiles[s_active_profile].port;
}


size_t moonraker_config_profile_capacity(void)
{
    return
        MOONRAKER_CONFIG_MAX_PROFILES;
}


size_t moonraker_config_profile_count(void)
{
    size_t count = 0;

    for (int index = 0;
         index < MOONRAKER_CONFIG_MAX_PROFILES;
         ++index) {
        if (profile_valid(
                &s_profiles[index])) {
            ++count;
        }
    }

    return count;
}


int moonraker_config_active_profile_index(void)
{
    return s_active_profile;
}


const char *moonraker_config_active_profile_name(void)
{
    return
        s_profiles[s_active_profile].name;
}


uint32_t moonraker_config_generation(void)
{
    return s_configuration_generation;
}


const moonraker_profile_t *moonraker_config_profile(
    int profile_index)
{
    if (!valid_profile_index(profile_index)) {
        return NULL;
    }

    return &s_profiles[profile_index];
}


bool moonraker_config_select_profile(
    int profile_index)
{
    if (!valid_profile_index(profile_index) ||
        !profile_valid(
            &s_profiles[profile_index])) {
        return false;
    }

    if (profile_index == s_active_profile) {
        return true;
    }

    int previous_active =
        s_active_profile;

    s_active_profile =
        profile_index;

    if (!persist_collection()) {
        s_active_profile =
            previous_active;

        return false;
    }

    advance_configuration_generation();

    ESP_LOGI(
        TAG,
        "Selected profile %d: %s (%s:%d)",
        s_active_profile,
        moonraker_config_active_profile_name(),
        moonraker_config_host(),
        moonraker_config_port());

    return true;
}


bool moonraker_config_save_profile(
    int profile_index,
    const char *name,
    const char *host,
    int port)
{
    if (!valid_profile_index(profile_index) ||
        !host ||
        !host[0] ||
        !valid_port(port)) {
        return false;
    }

    moonraker_profile_t previous =
        s_profiles[profile_index];

    moonraker_profile_t updated = {
        .configured = true,
        .port = port,
    };

    if (name && name[0]) {
        copy_text(
            updated.name,
            sizeof(updated.name),
            name);
    } else {
        default_profile_name(
            updated.name,
            sizeof(updated.name),
            profile_index);
    }

    copy_text(
        updated.host,
        sizeof(updated.host),
        host);

    s_profiles[profile_index] =
        updated;

    if (!persist_collection()) {
        s_profiles[profile_index] =
            previous;

        return false;
    }

    if (profile_index == s_active_profile) {
        advance_configuration_generation();
    }

    ESP_LOGI(
        TAG,
        "Saved profile %d: %s (%s:%d)",
        profile_index,
        updated.name,
        updated.host,
        updated.port);

    return true;
}


bool moonraker_config_delete_profile(
    int profile_index)
{
    if (!valid_profile_index(profile_index) ||
        !s_profiles[profile_index].configured) {
        return false;
    }

    moonraker_profile_t previous_profiles[
        MOONRAKER_CONFIG_MAX_PROFILES];

    memcpy(
        previous_profiles,
        s_profiles,
        sizeof(previous_profiles));

    int previous_active =
        s_active_profile;

    bool active_profile_affected =
        profile_index == previous_active;

    clear_profile(
        &s_profiles[profile_index]);

    int next_active =
        first_configured_profile();

    if (next_active < 0) {
        install_default_profile();
    } else if (profile_index ==
               s_active_profile) {
        s_active_profile =
            next_active;
    }

    if (!persist_collection()) {
        memcpy(
            s_profiles,
            previous_profiles,
            sizeof(previous_profiles));

        s_active_profile =
            previous_active;

        return false;
    }

    if (active_profile_affected) {
        advance_configuration_generation();
    }

    return true;
}


bool moonraker_config_select_host(
    const char *host,
    int port)
{
    return moonraker_config_save_profile(
        s_active_profile,
        moonraker_config_active_profile_name(),
        host,
        port);
}


bool moonraker_config_select_port(int port)
{
    if (!valid_port(port)) {
        return false;
    }

    return moonraker_config_save_profile(
        s_active_profile,
        moonraker_config_active_profile_name(),
        moonraker_config_host(),
        port);
}
