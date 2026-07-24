#include "thumbnail_session_v32.h"

#include <stdio.h>
#include <string.h>

#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "nvs.h"

#include "moonraker.h"
#include "printer_files.h"
#include "thumbnail_manager_v32.h"

static const char *TAG = "thumbnail_session";

static char s_selected_file[160] = "";
static char s_selected_thumbnail_path[192] = "";
#define METADATA_BODY_SIZE 8192
static char *s_metadata_body = NULL;
static char s_metadata_info[1024] = "";

static bool ensure_metadata_body(void)
{
    if (s_metadata_body) return true;

    s_metadata_body = heap_caps_calloc(
        1,
        METADATA_BODY_SIZE,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (!s_metadata_body) {
        s_metadata_body = heap_caps_calloc(
            1,
            METADATA_BODY_SIZE,
            MALLOC_CAP_8BIT);
    }

    if (!s_metadata_body) {
        ESP_LOGE(TAG, "Metadata buffer allocation failed");
        return false;
    }

    return true;
}


static void copy_text(char *destination,
                      size_t destination_size,
                      const char *source)
{
    if (!destination || destination_size == 0) {
        return;
    }

    snprintf(destination,
             destination_size,
             "%s",
             source ? source : "");
}

static void url_encode_filename(const char *input,
                                char *output,
                                size_t output_size)
{
    static const char hex[] = "0123456789ABCDEF";

    if (!output || output_size == 0) {
        return;
    }

    size_t j = 0;

    for (size_t i = 0;
         input && input[i] && j + 4 < output_size;
         i++) {
        unsigned char c = (unsigned char)input[i];

        if ((c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') ||
            c == '-' ||
            c == '_' ||
            c == '.' ||
            c == '~' ||
            c == '/') {
            output[j++] = (char)c;
        } else {
            output[j++] = '%';
            output[j++] = hex[(c >> 4) & 0x0f];
            output[j++] = hex[c & 0x0f];
        }
    }

    output[j] = '\0';
}

char *thumbnail_session_v32_selected_file(void)
{
    return s_selected_file;
}

size_t thumbnail_session_v32_selected_file_size(void)
{
    return sizeof(s_selected_file);
}

char *thumbnail_session_v32_selected_thumbnail_path(void)
{
    return s_selected_thumbnail_path;
}

size_t thumbnail_session_v32_selected_thumbnail_path_size(void)
{
    return sizeof(s_selected_thumbnail_path);
}

char *thumbnail_session_v32_metadata_info(void)
{
    return s_metadata_info;
}

size_t thumbnail_session_v32_metadata_info_size(void)
{
    return sizeof(s_metadata_info);
}

void thumbnail_session_v32_set_selected_file(const char *file)
{
    copy_text(s_selected_file,
              sizeof(s_selected_file),
              file);
}

void thumbnail_session_v32_clear_selected_file(void)
{
    s_selected_file[0] = '\0';
}

void thumbnail_session_v32_clear_thumbnail_path(void)
{
    s_selected_thumbnail_path[0] = '\0';
}

bool thumbnail_session_v32_build_metadata(
    const char *moonraker_host,
    int moonraker_port,
    const char *api_key,
    const char *file,
    char *out,
    size_t out_size)
{
    if (!file || !file[0] || !out || out_size == 0) {
        return false;
    }

    char encoded[220];
    url_encode_filename(file, encoded, sizeof(encoded));

    if (!ensure_metadata_body()) {
        snprintf(out, out_size, "Metadata buffer unavailable");
        return false;
    }

    memset(s_metadata_body, 0, METADATA_BODY_SIZE);
    s_selected_thumbnail_path[0] = '\0';

    int http_code = 0;
    esp_err_t error = ESP_FAIL;

    if (!moonraker_fetch_file_metadata(
            moonraker_host,
            moonraker_port,
            api_key,
            encoded,
            s_metadata_body,
            METADATA_BODY_SIZE,
            &http_code,
            &error)) {
        snprintf(out,
                 out_size,
                 "File:\\n%.150s\\n\\nMetadata failed\\nHTTP %d\\n%s",
                 file,
                 http_code,
                 esp_err_to_name(error));
        return false;
    }

    if (!printer_files_build_metadata_text(
            file,
            s_metadata_body,
            s_selected_thumbnail_path,
            sizeof(s_selected_thumbnail_path),
            out,
            out_size)) {
        snprintf(out,
                 out_size,
                 "File:\\n%.150s\\n\\nMetadata parse failed",
                 file);
        return false;
    }

    return true;
}

void thumbnail_session_v32_clear_png_buffer(void)
{
    thumbnail_manager_v32_clear_png();

    memset(thumbnail_manager_v32_image_dsc(),
           0,
           sizeof(*thumbnail_manager_v32_image_dsc()));
}

void thumbnail_session_v32_free_thumbnail(void)
{
    thumbnail_session_v32_clear_png_buffer();
}

void thumbnail_session_v32_install_png_buffer(
    uint8_t *buffer,
    size_t length)
{
    thumbnail_session_v32_free_thumbnail();

    thumbnail_manager_v32_take_png(buffer, length);
    thumbnail_manager_v32_prepare_raw_image();
    thumbnail_manager_v32_mark_ready();
}

void thumbnail_session_v32_save_last_selected_file(void)
{
    if (!s_selected_file[0]) {
        return;
    }

    nvs_handle_t handle;
    esp_err_t error =
        nvs_open("hmi", NVS_READWRITE, &handle);

    if (error != ESP_OK) {
        ESP_LOGW(TAG,
                 "NVS last_file open failed: %s",
                 esp_err_to_name(error));
        return;
    }

    error = nvs_set_str(handle,
                        "last_file",
                        s_selected_file);

    if (error == ESP_OK) {
        error = nvs_commit(handle);
    }

    nvs_close(handle);

    if (error != ESP_OK) {
        ESP_LOGW(TAG,
                 "NVS last_file save failed: %s",
                 esp_err_to_name(error));
    }
}

bool thumbnail_session_v32_load_selected_cache(
    bool sd_card_available)
{
    if (!sd_card_available || !s_selected_file[0]) {
        return false;
    }

    return thumbnail_manager_v32_load_selected_cache(
        s_selected_file);
}

void thumbnail_session_v32_store_selected_cache(
    bool sd_card_available)
{
    if (!sd_card_available || !s_selected_file[0]) {
        return;
    }

    if (!thumbnail_manager_v32_store_selected_cache(
            s_selected_file)) {
        ESP_LOGW(TAG, "SD_THUMB cache write failed");
    }
}

thumbnail_session_v32_restore_result_t
thumbnail_session_v32_restore_last_selected_file(
    bool sd_card_available)
{
    if (!sd_card_available) {
        return THUMBNAIL_SESSION_V32_RESTORE_NONE;
    }

    nvs_handle_t handle;
    esp_err_t error =
        nvs_open("hmi", NVS_READONLY, &handle);

    if (error != ESP_OK) {
        ESP_LOGI(TAG, "NVS last_file: none");
        return THUMBNAIL_SESSION_V32_RESTORE_NONE;
    }

    char last_file[160] = "";
    size_t length = sizeof(last_file);

    error = nvs_get_str(handle,
                        "last_file",
                        last_file,
                        &length);

    nvs_close(handle);

    if (error != ESP_OK || !last_file[0]) {
        ESP_LOGI(TAG, "NVS last_file: none");
        return THUMBNAIL_SESSION_V32_RESTORE_NONE;
    }

    thumbnail_session_v32_set_selected_file(last_file);

    if (thumbnail_session_v32_load_selected_cache(
            sd_card_available)) {
        ESP_LOGI(TAG,
                 "Restored last selected file from SD cache: %s",
                 s_selected_file);

        return THUMBNAIL_SESSION_V32_RESTORE_CACHE_READY;
    }

    ESP_LOGI(TAG,
             "Last selected file has no SD thumbnail cache yet: %s",
             s_selected_file);

    return THUMBNAIL_SESSION_V32_RESTORE_NO_CACHE;
}
