#include "printer_profile_preview_worker_v32.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "moonraker.h"
#include "moonraker_config_controller.h"
#include "printer_profile_health.h"
#include "printer_preview_cache_v32.h"
#include "printer_preview_store_v32.h"

#define PROFILE_PREVIEW_WIDTH  286
#define PROFILE_PREVIEW_HEIGHT 215

#define TAG "profile_preview_worker"


#define PROFILE_PREVIEW_WORKER_STACK 6144
#define PROFILE_PREVIEW_WORKER_INTERVAL_MS 1000
#define PROFILE_PREVIEW_API_KEY_MAX 160

static TaskHandle_t s_preview_worker_task = NULL;
static char s_preview_worker_api_key[PROFILE_PREVIEW_API_KEY_MAX];


static void url_encode(
    const char *input,
    char *output,
    size_t output_size)
{
    static const char hex[] = "0123456789ABCDEF";

    if (!output || output_size == 0) return;

    size_t out = 0;

    for (size_t in = 0;
         input && input[in] && out + 4 < output_size;
         ++in) {
        unsigned char c = (unsigned char)input[in];

        if ((c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' ||
            c == '~' || c == '/') {
            output[out++] = (char)c;
        } else {
            output[out++] = '%';
            output[out++] = hex[(c >> 4) & 0x0f];
            output[out++] = hex[c & 0x0f];
        }
    }

    output[out] = '\0';
}


static bool state_has_current_print(const char *state)
{
    return state &&
           (strcmp(state, "printing") == 0 ||
            strcmp(state, "paused") == 0);
}


void printer_profile_preview_worker_v32_reset(void)
{
    /* Cursor and generation are owned by printer_profile_health. */
}


static void printer_profile_preview_worker_v32_task(void *arg)
{
    (void)arg;

    while (true) {
        printer_profile_preview_worker_v32_poll_one(
            s_preview_worker_api_key);
        vTaskDelay(
            pdMS_TO_TICKS(PROFILE_PREVIEW_WORKER_INTERVAL_MS));
    }
}


void printer_profile_preview_worker_v32_start(const char *api_key)
{
    if (s_preview_worker_task) {
        return;
    }

    strlcpy(
        s_preview_worker_api_key,
        api_key ? api_key : "",
        sizeof(s_preview_worker_api_key));

    BaseType_t created = xTaskCreatePinnedToCore(
        printer_profile_preview_worker_v32_task,
        "profile_preview",
        PROFILE_PREVIEW_WORKER_STACK,
        NULL,
        3,
        &s_preview_worker_task,
        0);

    if (created != pdPASS) {
        s_preview_worker_task = NULL;
        ESP_LOGE(TAG, "Unable to start profile preview worker");
        return;
    }

    ESP_LOGI(TAG, "Inactive profile preview worker started");
}


void printer_profile_preview_worker_v32_poll_one(const char *api_key)
{
    int index = printer_profile_health_take_next_index();
    uint32_t generation_before = moonraker_config_generation();

    if (index == moonraker_config_active_profile_index()) {
        return;
    }

    const moonraker_profile_t *profile =
        moonraker_config_profile(index);

    if (!profile || !profile->configured) {
        printer_profile_health_set(index, true, false);
        printer_preview_cache_v32_invalidate(index);
        return;
    }

    char host[MOONRAKER_CONFIG_HOST_LENGTH];
    strlcpy(host, profile->host, sizeof(host));
    int port = profile->port;

    char stats[2048] = {0};
    int http_code = 0;
    esp_err_t error = ESP_FAIL;

    bool online = moonraker_fetch_print_stats(
        host,
        port,
        api_key,
        stats,
        sizeof(stats),
        &http_code,
        &error);

    if (generation_before != moonraker_config_generation()) return;

    printer_profile_health_set(index, true, online);

    if (!online) {
        return;
    }

    char state[32] = "";
    char file[160] = "";

    const char *print_stats = strstr(stats, "\"print_stats\"");
    if (!print_stats) print_stats = stats;

    json_find_string(print_stats, "state", state, sizeof(state));
    json_find_string(print_stats, "filename", file, sizeof(file));

    if (!state_has_current_print(state) || !file[0]) {
        return;
    }

    if (printer_preview_cache_v32_matches(index, file)) {
        return;
    }

    char encoded_file[384];
    url_encode(file, encoded_file, sizeof(encoded_file));

    char *metadata = heap_caps_calloc(
        1,
        8192,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (!metadata) {
        metadata = heap_caps_calloc(1, 8192, MALLOC_CAP_8BIT);
    }

    if (!metadata) return;

    bool metadata_ok = moonraker_fetch_file_metadata(
        host,
        port,
        api_key,
        encoded_file,
        metadata,
        8192,
        &http_code,
        &error);

    if (generation_before != moonraker_config_generation()) {
        heap_caps_free(metadata);
        return;
    }

    char thumbnail_path[256] = "";

    bool path_ok = metadata_ok &&
        json_find_best_thumbnail_path(
            metadata,
            thumbnail_path,
            sizeof(thumbnail_path));

    heap_caps_free(metadata);

    if (!path_ok) {
        ESP_LOGW(TAG, "Profile %d has no thumbnail for %s", index + 1, file);
        return;
    }

    char encoded_thumbnail[512];
    url_encode(
        thumbnail_path,
        encoded_thumbnail,
        sizeof(encoded_thumbnail));

    uint8_t *png = NULL;
    size_t png_size = 0;

    if (!moonraker_fetch_thumbnail_encoded(
            host,
            port,
            encoded_thumbnail,
            &png,
            &png_size)) {
        ESP_LOGW(TAG, "Profile %d thumbnail download failed", index + 1);
        return;
    }

    if (generation_before != moonraker_config_generation()) {
        heap_caps_free(png);
        return;
    }

    bool installed = false;

    if (bsp_display_lock(1000)) {
        installed = printer_preview_cache_v32_publish_png(
            index,
            host,
            port,
            file,
            png,
            png_size,
            PROFILE_PREVIEW_WIDTH,
            PROFILE_PREVIEW_HEIGHT);

        bsp_display_unlock();
    }

    if (installed) {
        printer_preview_store_v32_store_png(
            index,
            host,
            port,
            file,
            png,
            png_size);
    }

    heap_caps_free(png);

    ESP_LOGI(
        TAG,
        "Profile %d preview refresh %s: %s",
        index + 1,
        installed ? "complete" : "failed",
        file);
}
