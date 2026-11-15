#include "moonraker_http_objects.h"
#include "printer_profile_preview_worker.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "moonraker.h"
#include "moonraker_live_websocket.h"
#include "moonraker_config_controller.h"
#include "printer_profile_health.h"
#include "printer_preview_cache.h"
#include "printer_preview_store.h"
#include "thumbnail_manager.h"

#define PROFILE_PREVIEW_FILENAME_LENGTH 160
#define PROFILE_PREVIEW_WIDTH  286
#define PROFILE_PREVIEW_HEIGHT 215

#define TAG "profile_preview_worker"


#define PROFILE_PREVIEW_WORKER_STACK 12288
#define PROFILE_PREVIEW_WORKER_INTERVAL_MS 1000
/* A failed inactive endpoint is not useful work every scheduler pass. */
#define PROFILE_PREVIEW_OFFLINE_RETRY_US (20LL * 1000 * 1000)
/* One missed request receives a quick confirmation without returning to
 * the old repeated-probe behavior for confirmed offline profiles. */
#define PROFILE_PREVIEW_CONFIRM_RETRY_US (3LL * 1000 * 1000)
#define PROFILE_PREVIEW_STATUS_AGE_US (2500LL * 1000)
#define PROFILE_PREVIEW_IMAGE_RETRY_US (15LL * 1000 * 1000)

typedef struct {
    int index;
    char file[PROFILE_PREVIEW_FILENAME_LENGTH];
} preview_candidate_t;

static TaskHandle_t s_preview_worker_task = NULL;
static int64_t s_preview_retry_after_us[MOONRAKER_CONFIG_MAX_PROFILES];
static int64_t s_image_retry_after_us[MOONRAKER_CONFIG_MAX_PROFILES];
/* Odd generations have an address. Every link transition retires old I/O. */
static uint32_t s_network_epoch;
static uint32_t s_retry_revision[MOONRAKER_CONFIG_MAX_PROFILES];
static uint32_t s_last_polled_network_epoch; /* worker-owned */


static bool state_has_current_print(const char *state)
{
    return state &&
           (strcmp(state, "printing") == 0 ||
            strcmp(state, "paused") == 0);
}


void printer_profile_preview_worker_reset(void)
{
    /* Cursor and generation are owned by printer_profile_health. */
    for (int i = 0; i < MOONRAKER_CONFIG_MAX_PROFILES; ++i) {
        __atomic_store_n(&s_preview_retry_after_us[i], 0, __ATOMIC_RELAXED);
        __atomic_store_n(&s_image_retry_after_us[i], 0, __ATOMIC_RELAXED);
    }
}


void printer_profile_preview_worker_set_network_ready(bool ready)
{
    uint32_t epoch = __atomic_load_n(&s_network_epoch, __ATOMIC_RELAXED);
    while ((bool)(epoch & 1u) != ready &&
        !__atomic_compare_exchange_n(&s_network_epoch, &epoch, epoch + 1u,
            false, __ATOMIC_RELEASE, __ATOMIC_RELAXED)) {
    }
}

static void printer_profile_preview_worker_task(void *arg)
{
    (void)arg;

    while (true) {
        printer_profile_preview_worker_poll_cycle();
        vTaskDelay(
            pdMS_TO_TICKS(PROFILE_PREVIEW_WORKER_INTERVAL_MS));
    }
}


void printer_profile_preview_worker_start(void)
{
    if (s_preview_worker_task) {
        return;
    }

    BaseType_t created = xTaskCreatePinnedToCore(
        printer_profile_preview_worker_task,
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


static bool profile_request_current(int index, const moonraker_profile_t *captured,
                                    uint32_t generation, uint32_t network_epoch,
                                    uint32_t connection_revision)
{
    const moonraker_profile_t *current = moonraker_config_profile(index);
    return network_epoch == __atomic_load_n(&s_network_epoch, __ATOMIC_ACQUIRE) &&
        generation == moonraker_config_generation() &&
        connection_revision == moonraker_config_connection_revision(index) &&
        index != moonraker_config_active_profile_index() && current &&
        current->configured && captured->port == current->port &&
        captured->secure_transport == current->secure_transport &&
        !strcmp(captured->host, current->host) &&
        !strcmp(captured->api_key, current->api_key);
}

static void poll_profile(int index, const moonraker_profile_t *profile,
                         uint32_t generation_before, uint32_t network_epoch, uint32_t connection_revision,
                         preview_candidate_t *candidate)
{
    int64_t now = esp_timer_get_time();
    const char *api_key = profile->api_key;
    char host[MOONRAKER_CONFIG_HOST_LENGTH];
    strlcpy(host, profile->host, sizeof(host));
    int port = profile->port;

    char stats[2048] = {0};
    int http_code = 0;
    esp_err_t error = ESP_FAIL;

    if (!profile_request_current(index, profile, generation_before, network_epoch, connection_revision)) return;

    bool online = moonraker_fetch_print_stats(
        host,
        port,
        api_key,
        stats,
        sizeof(stats),
        &http_code,
        &error);

    if (!profile_request_current(index, profile, generation_before, network_epoch, connection_revision)) return;

    if (!online) {
        /* An HTTP response proves the endpoint answered. An auth rejection,
         * upstream error, oversized payload or busy local lane cannot confirm
         * that the printer disconnected. Keep the last confirmed health. */
        if (http_code > 0 || error == ESP_ERR_INVALID_STATE) {
            __atomic_store_n(&s_preview_retry_after_us[index],
                esp_timer_get_time() + PROFILE_PREVIEW_CONFIRM_RETRY_US,
                __ATOMIC_RELAXED);
            ESP_LOGD(TAG, "Profile %d status unavailable (HTTP %d, error %d)",
                index + 1, http_code, (int)error);
            return;
        }

        printer_profile_health_report_live_state(index, false, NULL);

        /* Keep a previously-online profile in VERIFYING until a second
         * failure confirms it. A confirmed offline profile returns to
         * the low-pressure retry cadence. */
        bool still_online = printer_profile_health_get(index, NULL);
        __atomic_store_n(&s_preview_retry_after_us[index], esp_timer_get_time() +
            (still_online
                ? PROFILE_PREVIEW_CONFIRM_RETRY_US
                : PROFILE_PREVIEW_OFFLINE_RETRY_US), __ATOMIC_RELAXED);
        ESP_LOGD(
            TAG,
            "Profile %d %s; retry deferred",
            index + 1,
            still_online ? "verification pending" : "offline");
        return;
    }

    __atomic_store_n(&s_preview_retry_after_us[index], 0, __ATOMIC_RELAXED);

    char state[32] = "";
    char file[PROFILE_PREVIEW_FILENAME_LENGTH] = "";

    const cJSON *status;
    cJSON *root = moonraker_http_objects_parse(stats, strnlen(stats, sizeof(stats)),
        sizeof(stats) - 1, &status);
    const cJSON *hooks = cJSON_GetObjectItemCaseSensitive(status, "webhooks");
    const char *klipper_state = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(hooks, "state"));
    if (!klipper_state || !*klipper_state) {
        /* Incomplete responses are not a Klipper state transition. */
        cJSON_Delete(root);
        return;
    }
    const bool ready = strcmp(klipper_state, "ready") == 0;
    const cJSON *print_stats = cJSON_GetObjectItemCaseSensitive(status, "print_stats");
    const char *parsed_state = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(print_stats, "state"));
    const char *parsed_file = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(print_stats, "filename"));
    if (parsed_state && strlen(parsed_state) < sizeof(state)) strlcpy(state, parsed_state, sizeof(state));
    if (parsed_file && strlen(parsed_file) < sizeof(file)) strlcpy(file, parsed_file, sizeof(file));

    if (!profile_request_current(index, profile, generation_before, network_epoch, connection_revision)) {
        cJSON_Delete(root);
        return;
    }

    /* Moonraker can answer even when Klipper has no MCU, is starting,
     * or is shut down. That printer is not available to the operator. */
    if (!ready) {
        bool fault = (strcmp(klipper_state, "shutdown") == 0 ||
            strcmp(klipper_state, "error") == 0);

        if (fault) {
            printer_profile_health_set_live_state(
                index, true, false, klipper_state);
        } else {
            printer_profile_health_set_verifying(index);
        }
        __atomic_store_n(&s_preview_retry_after_us[index],
            esp_timer_get_time() + PROFILE_PREVIEW_CONFIRM_RETRY_US, __ATOMIC_RELAXED);

        ESP_LOGI(
            TAG,
            "Profile %d Klipper is %s (%s)",
            index + 1,
            klipper_state,
            fault ? "fault" : "waiting");
        cJSON_Delete(root);
        return;
    }

    cJSON_Delete(root);

    printer_profile_health_report_live_state(index, true, state);

    if (candidate->index < 0 && state_has_current_print(state) && file[0] &&
        now >= __atomic_load_n(&s_image_retry_after_us[index], __ATOMIC_RELAXED) &&
        !printer_preview_cache_matches(index, file)) {
        candidate->index = index;
        strlcpy(candidate->file, file, sizeof(candidate->file));
    }
}

static void fetch_preview(int index, const moonraker_profile_t *profile,
                          uint32_t generation_before, uint32_t network_epoch, uint32_t connection_revision,
                          const char *file)
{
    const char *host = profile->host;
    const char *api_key = profile->api_key;
    int port = profile->port;
    int http_code = 0;
    esp_err_t error = ESP_FAIL;
    __atomic_store_n(&s_image_retry_after_us[index],
        esp_timer_get_time() + PROFILE_PREVIEW_IMAGE_RETRY_US, __ATOMIC_RELAXED);

    char encoded_file[PROFILE_PREVIEW_FILENAME_LENGTH * 3];
    thumbnail_manager_url_encode(file, encoded_file, sizeof(encoded_file));

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

    if (!profile_request_current(index, profile, generation_before, network_epoch, connection_revision)) {
        heap_caps_free(metadata);
        return;
    }

    char thumbnail_path[256] = "";

    bool path_ok = metadata_ok &&
        moonraker_find_thumbnail_path(
            metadata, file, thumbnail_path, sizeof(thumbnail_path));

    heap_caps_free(metadata);

    if (!path_ok) {
        ESP_LOGW(TAG, "Profile %d has no thumbnail for %s", index + 1, file);
        return;
    }

    char encoded_thumbnail[sizeof(thumbnail_path) * 3];
    thumbnail_manager_url_encode(
        thumbnail_path,
        encoded_thumbnail,
        sizeof(encoded_thumbnail));

    uint8_t *png = NULL;
    size_t png_size = 0;

    if (!moonraker_fetch_thumbnail_encoded_with_api_key(
            host,
            port,
            api_key,
            encoded_thumbnail,
            &png,
            &png_size)) {
        ESP_LOGW(TAG, "Profile %d thumbnail download failed", index + 1);
        return;
    }

    if (!profile_request_current(index, profile, generation_before, network_epoch, connection_revision)) {
        heap_caps_free(png);
        return;
    }

    bool installed = false;

    if (bsp_display_lock(1000)) {
        if (!profile_request_current(index, profile, generation_before, network_epoch, connection_revision)) {
            bsp_display_unlock();
            heap_caps_free(png);
            return;
        }
        installed = printer_preview_cache_publish_png(
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

    if (installed && profile_request_current(index, profile, generation_before, network_epoch, connection_revision)) {
        printer_preview_store_store_png(
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

void printer_profile_preview_worker_poll_cycle(void)
{
    const uint32_t network_epoch = __atomic_load_n(&s_network_epoch, __ATOMIC_ACQUIRE);
    if (!(network_epoch & 1u)) return;
    if (s_last_polled_network_epoch != network_epoch) {
        printer_profile_preview_worker_reset();
        s_last_polled_network_epoch = network_epoch;
    }

    /* Publish status for each eligible row before starting any image work. */
    preview_candidate_t candidate = {.index = -1};
    moonraker_profile_t preview_profile = {0};
    uint32_t preview_revision = 0;
    const uint32_t generation = moonraker_config_generation();
    bool visited[MOONRAKER_CONFIG_MAX_PROFILES] = {false};
    for (int turn = 0; turn < MOONRAKER_CONFIG_MAX_PROFILES; ++turn) {
        if (network_epoch != __atomic_load_n(&s_network_epoch, __ATOMIC_ACQUIRE) ||
            generation != moonraker_config_generation()) break;
        int index = printer_profile_health_take_next_index();
        if (index < 0 || visited[index]) break;
        visited[index] = true;
        if (index == moonraker_config_active_profile_index()) continue;

        const moonraker_profile_t *profile = moonraker_config_profile(index);
        if (!profile || !profile->configured) continue;
        uint32_t revision = moonraker_config_connection_revision(index);
        if (s_retry_revision[index] != revision) {
            s_retry_revision[index] = revision;
            __atomic_store_n(&s_preview_retry_after_us[index], 0, __ATOMIC_RELAXED);
            __atomic_store_n(&s_image_retry_after_us[index], 0, __ATOMIC_RELAXED);
        }
        bool known = false;
        (void)printer_profile_health_get(index, &known);
        if (known && esp_timer_get_time() <
            __atomic_load_n(&s_preview_retry_after_us[index], __ATOMIC_RELAXED)) continue;
        if (printer_profile_health_live_state_fresh(index, PROFILE_PREVIEW_STATUS_AGE_US)) continue;

        moonraker_profile_t captured = *profile;
        int previous_candidate = candidate.index;
        poll_profile(index, &captured, generation, network_epoch, revision, &candidate);
        if (previous_candidate < 0 && candidate.index == index) {
            preview_profile = captured;
            preview_revision = revision;
        }
        for (size_t i = 0; i < sizeof(captured.api_key); ++i)
            ((volatile char *)captured.api_key)[i] = 0;
    }

    bool active_recovering = moonraker_live_websocket_running() &&
        !moonraker_live_websocket_connected();
    if (candidate.index >= 0 && !active_recovering &&
        profile_request_current(candidate.index, &preview_profile, generation, network_epoch, preview_revision)) {
        fetch_preview(candidate.index, &preview_profile, generation, network_epoch, preview_revision, candidate.file);
    }
    for (size_t i = 0; i < sizeof(preview_profile.api_key); ++i)
        ((volatile char *)preview_profile.api_key)[i] = 0;
}
