#include "moonraker_tls_trust_store.h"
#include "moonraker_config_controller.h"
#include "moonraker_transport_security_controller.h"

#include <stdio.h>
#include <string.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "nvs.h"

#define TAG "moon_tls_store"
#define TLS_NVS_NAMESPACE "moon_tls"

static char *s_pem[MOONRAKER_CONFIG_MAX_PROFILES];
static bool s_loaded[MOONRAKER_CONFIG_MAX_PROFILES];

static bool valid_index(int profile_index)
{
    return profile_index >= 0 && profile_index < MOONRAKER_CONFIG_MAX_PROFILES;
}

static bool key_for_profile(int profile_index, char *key, size_t key_size)
{
    return snprintf(key, key_size, "ca_pem_%d", profile_index) > 0;
}

static bool ensure_profile_buffer(int profile_index)
{
    if (!valid_index(profile_index)) return false;
    if (s_pem[profile_index]) return true;
    s_pem[profile_index] = heap_caps_calloc(1, MOONRAKER_TLS_CA_PEM_MAX,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (s_pem[profile_index]) {
        ESP_LOGI(TAG, "Profile %d CA allocated in PSRAM", profile_index);
        return true;
    }
    s_pem[profile_index] = heap_caps_calloc(1, MOONRAKER_TLS_CA_PEM_MAX,
        MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!s_pem[profile_index]) return false;
    ESP_LOGW(TAG, "Profile %d CA using internal-RAM fallback", profile_index);
    return true;
}

static bool load_profile(int profile_index)
{
    if (!ensure_profile_buffer(profile_index)) return false;
    if (s_loaded[profile_index]) return true;
    s_loaded[profile_index] = true;
    char key[16];
    if (!key_for_profile(profile_index, key, sizeof(key))) return false;
    nvs_handle_t handle;
    if (nvs_open(TLS_NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) return true;
    size_t length = MOONRAKER_TLS_CA_PEM_MAX;
    if (nvs_get_str(handle, key, s_pem[profile_index], &length) != ESP_OK) {
        s_pem[profile_index][0] = '\0';
    }
    nvs_close(handle);
    return true;
}

bool moonraker_tls_trust_store_init(void)
{
    return true; /* Certificates load lazily for the profile that needs one. */
}

bool moonraker_tls_trust_store_set_pem_for_profile(int profile_index, const char *pem)
{
    if (!valid_index(profile_index) || !pem ||
        !strstr(pem, "-----BEGIN CERTIFICATE-----") ||
        !strstr(pem, "-----END CERTIFICATE-----") ||
        strlen(pem) >= MOONRAKER_TLS_CA_PEM_MAX ||
        !ensure_profile_buffer(profile_index)) return false;
    if (pem != s_pem[profile_index]) {
        strlcpy(s_pem[profile_index], pem, MOONRAKER_TLS_CA_PEM_MAX);
    }
    s_loaded[profile_index] = true;
    char key[16];
    if (!key_for_profile(profile_index, key, sizeof(key))) return false;
    nvs_handle_t handle;
    if (nvs_open(TLS_NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) return false;
    esp_err_t err = nvs_set_str(handle, key, s_pem[profile_index]);
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    return err == ESP_OK;
}

bool moonraker_tls_trust_store_import_file_for_profile(int profile_index, const char *path)
{
    if (!valid_index(profile_index) || !path || !path[0] || !ensure_profile_buffer(profile_index)) return false;
    FILE *file = fopen(path, "rb");
    if (!file) return false;
    size_t count = fread(s_pem[profile_index], 1, MOONRAKER_TLS_CA_PEM_MAX - 1, file);
    int extra = fgetc(file);
    fclose(file);
    if (extra != EOF) { s_pem[profile_index][0] = '\0'; return false; }
    s_pem[profile_index][count] = '\0';
    return moonraker_tls_trust_store_set_pem_for_profile(profile_index, s_pem[profile_index]);
}

const char *moonraker_tls_trust_store_pem_for_profile(int profile_index)
{
    if (!load_profile(profile_index)) return NULL;
    return s_pem[profile_index][0] ? s_pem[profile_index] : NULL;
}
