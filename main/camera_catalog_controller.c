#include "camera_catalog_controller.h"

#include "moonraker_config_controller.h"

#include "esp_err.h"
#include "nvs.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define CAMERA_NVS_NAMESPACE "netcfg"

static bool valid_profile(int profile_index)
{
    return profile_index >= 0 &&
           profile_index < (int)moonraker_config_profile_capacity();
}

static bool valid_url(const char *url)
{
    return !url || !url[0] ||
           strncmp(url, "http://", 7) == 0 ||
           strncmp(url, "https://", 8) == 0;
}

static void slot_key(char *key, size_t size, int profile, size_t camera)
{
    snprintf(key, size, "p%d_c%u", profile, (unsigned)camera);
}

static void camera_name_key(char *key, size_t size, int profile, size_t camera)
{
    snprintf(key, size, "p%d_n%u", profile, (unsigned)camera);
}

static void camera_default_key(char *key, size_t size, int profile)
{
    /* Profiles are bounded by moonraker_config_profile_capacity().  Keeping
     * the formatted value in the uint8_t storage domain also lets GCC prove
     * that every supported NVS key fits in the existing key buffer. */
    unsigned profile_id = (unsigned)(uint8_t)profile;
    snprintf(key, size, "p%u_cdef", profile_id);
}

static void load_camera_name(int profile, size_t camera, char *name, size_t size)
{
    snprintf(name, size, "Camera %u", (unsigned)(camera + 1));
    nvs_handle_t handle;
    if (nvs_open(CAMERA_NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) return;
    char key[16];
    camera_name_key(key, sizeof(key), profile, camera);
    size_t length = size;
    if (nvs_get_str(handle, key, name, &length) != ESP_OK || !name[0]) {
        snprintf(name, size, "Camera %u", (unsigned)(camera + 1));
    }
    nvs_close(handle);
}

static void view_key(char *key, size_t size, int profile, size_t camera)
{
    snprintf(key, size, "p%d_v%u", profile, (unsigned)camera);
}

static void load_view(int profile_index, size_t camera_index,
                      camera_catalog_entry_t *entry)
{
    if (!entry) return;
    entry->rotation = 0;
    entry->mirror_horizontal = false;
    entry->mirror_vertical = false;
    nvs_handle_t handle;
    if (nvs_open(CAMERA_NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) return;
    char key[16];
    uint8_t packed = 0;
    view_key(key, sizeof(key), profile_index, camera_index);
    if (nvs_get_u8(handle, key, &packed) == ESP_OK) {
        entry->rotation = (unsigned)(packed & 0x03U) * 90U;
        entry->mirror_horizontal = (packed & 0x04U) != 0;
        entry->mirror_vertical = (packed & 0x08U) != 0;
    }
    nvs_close(handle);
}

size_t camera_catalog_count(int profile_index)
{
    size_t count = 0;
    camera_catalog_entry_t entry;
    for (size_t index = 0; index < CAMERA_CATALOG_MAX_CAMERAS; ++index) {
        if (camera_catalog_get(profile_index, index, &entry) &&
            entry.configured) {
            ++count;
        }
    }
    return count;
}

bool camera_catalog_get(int profile_index, size_t camera_index,
                        camera_catalog_entry_t *entry)
{
    if (!entry || !valid_profile(profile_index) ||
        camera_index >= CAMERA_CATALOG_MAX_CAMERAS) {
        return false;
    }
    memset(entry, 0, sizeof(*entry));
    if (camera_index == 0) {
        const char *url = moonraker_config_camera_stream_url(profile_index);
        if (!url || !url[0]) return true;
        load_camera_name(profile_index, camera_index, entry->name, sizeof(entry->name));
        strlcpy(entry->stream_url, url, sizeof(entry->stream_url));
        entry->configured = true;
        load_view(profile_index, camera_index, entry);
        return true;
    }
    nvs_handle_t handle;
    if (nvs_open(CAMERA_NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
        return true;
    }
    char key[16];
    slot_key(key, sizeof(key), profile_index, camera_index);
    size_t length = sizeof(entry->stream_url);
    esp_err_t error = nvs_get_str(handle, key, entry->stream_url, &length);
    nvs_close(handle);
    if (error != ESP_OK || !valid_url(entry->stream_url) ||
        !entry->stream_url[0]) {
        entry->stream_url[0] = '\0';
        return true;
    }
    load_camera_name(profile_index, camera_index,
                     entry->name, sizeof(entry->name));
    entry->configured = true;
    load_view(profile_index, camera_index, entry);
    return true;
}

bool camera_catalog_set(int profile_index, size_t camera_index,
                        const char *name, const char *stream_url)
{
    if (!valid_profile(profile_index) ||
        camera_index >= CAMERA_CATALOG_MAX_CAMERAS ||
        !valid_url(stream_url)) {
        return false;
    }
    if (camera_index == 0 &&
        !moonraker_config_set_camera_stream_url(profile_index,
                                                 stream_url ? stream_url : "")) {
        return false;
    }

    nvs_handle_t handle;
    if (nvs_open(CAMERA_NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) {
        return false;
    }

    char key[16];
    esp_err_t error = ESP_OK;
    if (camera_index != 0) {
        slot_key(key, sizeof(key), profile_index, camera_index);
        error = stream_url && stream_url[0]
            ? nvs_set_str(handle, key, stream_url)
            : nvs_erase_key(handle, key);
        if (error == ESP_ERR_NVS_NOT_FOUND) error = ESP_OK;
    }
    if (error == ESP_OK) {
        camera_name_key(key, sizeof(key), profile_index, camera_index);
        error = stream_url && stream_url[0]
            ? nvs_set_str(handle, key, name && name[0] ? name : "Camera")
            : nvs_erase_key(handle, key);
        if (error == ESP_ERR_NVS_NOT_FOUND) error = ESP_OK;
    }
    if (error == ESP_OK) error = nvs_commit(handle);
    nvs_close(handle);
    return error == ESP_OK;
}

bool camera_catalog_clear(int profile_index, size_t camera_index)
{
    return camera_catalog_set(profile_index, camera_index, "", "");
}


size_t camera_catalog_default(int profile_index)
{
    if (!valid_profile(profile_index)) return 0;

    uint8_t saved = 0;
    nvs_handle_t handle;
    if (nvs_open(CAMERA_NVS_NAMESPACE, NVS_READONLY, &handle) == ESP_OK) {
        char key[16];
        camera_default_key(key, sizeof(key), profile_index);
        (void)nvs_get_u8(handle, key, &saved);
        nvs_close(handle);
    }

    camera_catalog_entry_t entry = {0};
    if (saved < CAMERA_CATALOG_MAX_CAMERAS &&
        camera_catalog_get(profile_index, saved, &entry) && entry.configured) {
        return saved;
    }
    for (size_t index = 0; index < CAMERA_CATALOG_MAX_CAMERAS; ++index) {
        if (camera_catalog_get(profile_index, index, &entry) && entry.configured) {
            return index;
        }
    }
    return 0;
}

bool camera_catalog_set_default(int profile_index, size_t camera_index)
{
    camera_catalog_entry_t entry = {0};
    if (!valid_profile(profile_index) ||
        camera_index >= CAMERA_CATALOG_MAX_CAMERAS ||
        !camera_catalog_get(profile_index, camera_index, &entry) ||
        !entry.configured) {
        return false;
    }

    nvs_handle_t handle;
    if (nvs_open(CAMERA_NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) {
        return false;
    }
    /* GCC cannot infer valid_profile() for formatted NVS keys. */
    char key[24];
    camera_default_key(key, sizeof(key), profile_index);
    esp_err_t error = nvs_set_u8(handle, key, (uint8_t)camera_index);
    if (error == ESP_OK) error = nvs_commit(handle);
    nvs_close(handle);
    return error == ESP_OK;
}

bool camera_catalog_set_view(int profile_index, size_t camera_index,
                             unsigned rotation,
                             bool mirror_horizontal,
                             bool mirror_vertical)
{
    if (!valid_profile(profile_index) ||
        camera_index >= CAMERA_CATALOG_MAX_CAMERAS ||
        rotation % 90U != 0 || rotation > 270U) return false;
    uint8_t packed = (uint8_t)(rotation / 90U);
    if (mirror_horizontal) packed |= 0x04U;
    if (mirror_vertical) packed |= 0x08U;

    nvs_handle_t handle;
    if (nvs_open(CAMERA_NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) return false;
    char key[16];
    view_key(key, sizeof(key), profile_index, camera_index);
    esp_err_t error = packed ? nvs_set_u8(handle, key, packed) : nvs_erase_key(handle, key);
    if (error == ESP_ERR_NVS_NOT_FOUND) error = ESP_OK;
    if (error == ESP_OK) error = nvs_commit(handle);
    nvs_close(handle);
    return error == ESP_OK;
}
