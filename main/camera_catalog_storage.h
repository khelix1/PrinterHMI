#pragma once

#include <stdint.h>
#include <stdio.h>
#include "esp_err.h"
#include "nvs.h"
#include "camera_catalog_controller.h"

/* Camera metadata shares the printer profile's netcfg namespace. */
static inline void slot_key(char *key, size_t size, int profile, size_t camera)
{
    snprintf(key, size, "p%d_c%u", profile, (unsigned)camera);
}

static inline void camera_name_key(char *key, size_t size, int profile, size_t camera)
{
    snprintf(key, size, "p%d_n%u", profile, (unsigned)camera);
}

static inline void camera_default_key(char *key, size_t size, int profile)
{
    /* Profiles are bounded by moonraker_config_profile_capacity().  Keeping
     * the formatted value in the uint8_t storage domain also lets GCC prove
     * that every supported NVS key fits in the existing key buffer. */
    unsigned profile_id = (unsigned)(uint8_t)profile;
    snprintf(key, size, "p%u_cdef", profile_id);
}

static inline void view_key(char *key, size_t size, int profile, size_t camera)
{
    snprintf(key, size, "p%d_v%u", profile, (unsigned)camera);
}

static inline esp_err_t camera_catalog_erase_slot_metadata(nvs_handle_t handle, int profile)
{
    char key[24];
    for (size_t i = 0; i < CAMERA_CATALOG_MAX_CAMERAS; ++i) {
        if (i != 0) {
            slot_key(key, sizeof(key), profile, i);
            esp_err_t error = nvs_erase_key(handle, key);
            if (error != ESP_OK && error != ESP_ERR_NVS_NOT_FOUND) return error;
        }
        camera_name_key(key, sizeof(key), profile, i);
        esp_err_t error = nvs_erase_key(handle, key);
        if (error != ESP_OK && error != ESP_ERR_NVS_NOT_FOUND) return error;
        view_key(key, sizeof(key), profile, i);
        error = nvs_erase_key(handle, key);
        if (error != ESP_OK && error != ESP_ERR_NVS_NOT_FOUND) return error;
    }
    camera_default_key(key, sizeof(key), profile);
    esp_err_t error = nvs_erase_key(handle, key);
    return error == ESP_ERR_NVS_NOT_FOUND ? ESP_OK : error;
}
