#include "files_row_preview_v32.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bsp/esp-bsp.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "moonraker.h"
#include "thumbnail_manager_v32.h"
#include "thumbnail_render_v32.h"

#define TAG "files_row_preview"
#define ROW_PREVIEW_WIDTH 64
#define ROW_PREVIEW_HEIGHT 64
#define ROW_PREVIEW_SLOT_COUNT 24
#define ROW_PREVIEW_QUEUE_LENGTH 12
#define ROW_PREVIEW_METADATA_SIZE 8192

typedef enum {
    ROW_PREVIEW_EMPTY = 0,
    ROW_PREVIEW_QUEUED,
    ROW_PREVIEW_LOADING,
    ROW_PREVIEW_READY,
    ROW_PREVIEW_FAILED,
} row_preview_state_t;

typedef struct {
    char file[160];
    uint16_t *pixels;
    lv_image_dsc_t image;
    uint32_t generation;
    row_preview_state_t state;
} row_preview_slot_t;

typedef struct {
    int slot;
    uint32_t generation;
} row_preview_job_t;

static row_preview_slot_t *s_slots;
static QueueHandle_t s_queue;
static SemaphoreHandle_t s_lock;
static bool s_worker_started;
static uint32_t s_generation;
static char s_host[128];
static char s_api_key[128];
static int s_port;
static bool s_sd_available;
static files_row_preview_v32_ready_cb_t s_ready_cb;

static bool ensure_runtime(void)
{
    if (!s_slots) {
        s_slots = heap_caps_calloc(
            ROW_PREVIEW_SLOT_COUNT,
            sizeof(*s_slots),
            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

        if (!s_slots) {
            s_slots = heap_caps_calloc(
                ROW_PREVIEW_SLOT_COUNT,
                sizeof(*s_slots),
                MALLOC_CAP_8BIT);
        }
    }

    if (!s_lock) {
        s_lock = xSemaphoreCreateMutex();
    }

    if (!s_queue) {
        s_queue = xQueueCreate(
            ROW_PREVIEW_QUEUE_LENGTH,
            sizeof(row_preview_job_t));
    }

    return s_slots && s_lock && s_queue;
}

static bool fetch_preview_png(
    const char *host,
    int port,
    const char *api_key,
    const char *file,
    bool sd_available,
    uint8_t **png_out,
    size_t *png_size_out)
{
    *png_out = NULL;
    *png_size_out = 0;

    char cache_path[220];
    if (sd_available &&
        thumbnail_manager_v32_cache_path_for_file(
            file,
            cache_path,
            sizeof(cache_path)) &&
        thumbnail_manager_v32_load_cache_file(
            cache_path,
            png_out,
            png_size_out)) {
        return true;
    }

    char *metadata = heap_caps_calloc(
        1,
        ROW_PREVIEW_METADATA_SIZE,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (!metadata) {
        metadata = heap_caps_calloc(
            1,
            ROW_PREVIEW_METADATA_SIZE,
            MALLOC_CAP_8BIT);
    }

    if (!metadata) return false;

    char encoded_file[256];
    thumbnail_manager_v32_url_encode(
        file,
        encoded_file,
        sizeof(encoded_file));

    int http_code = 0;
    esp_err_t error = ESP_FAIL;
    bool metadata_ok = moonraker_fetch_file_metadata(
        host,
        port,
        api_key,
        encoded_file,
        metadata,
        ROW_PREVIEW_METADATA_SIZE,
        &http_code,
        &error);

    char thumbnail_path[192] = "";
    bool has_thumbnail = metadata_ok &&
        json_find_best_thumbnail_path(
            metadata,
            thumbnail_path,
            sizeof(thumbnail_path));

    heap_caps_free(metadata);

    if (!has_thumbnail) return false;

    char encoded_thumbnail[256];
    thumbnail_manager_v32_url_encode(
        thumbnail_path,
        encoded_thumbnail,
        sizeof(encoded_thumbnail));

    if (!moonraker_fetch_thumbnail_encoded(
            host,
            port,
            encoded_thumbnail,
            png_out,
            png_size_out)) {
        return false;
    }

    if (sd_available &&
        thumbnail_manager_v32_cache_path_for_file(
            file,
            cache_path,
            sizeof(cache_path))) {
        (void)thumbnail_manager_v32_store_cache_file(
            cache_path,
            *png_out,
            *png_size_out);
    }

    return true;
}

static void preview_worker(void *arg)
{
    (void)arg;
    row_preview_job_t job;

    while (true) {
        if (xQueueReceive(s_queue, &job, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        char host[sizeof(s_host)];
        char api_key[sizeof(s_api_key)];
        char file[160];
        int port = 0;
        bool sd_available = false;

        xSemaphoreTake(s_lock, portMAX_DELAY);

        bool current =
            job.slot >= 0 &&
            job.slot < ROW_PREVIEW_SLOT_COUNT &&
            job.generation == s_generation &&
            s_slots[job.slot].generation == job.generation &&
            s_slots[job.slot].state == ROW_PREVIEW_QUEUED;

        if (current) {
            s_slots[job.slot].state = ROW_PREVIEW_LOADING;
            strlcpy(host, s_host, sizeof(host));
            strlcpy(api_key, s_api_key, sizeof(api_key));
            strlcpy(file, s_slots[job.slot].file, sizeof(file));
            port = s_port;
            sd_available = s_sd_available;
        }

        xSemaphoreGive(s_lock);

        if (!current) continue;

        uint8_t *png = NULL;
        size_t png_size = 0;
        bool fetched = fetch_preview_png(
            host,
            port,
            api_key,
            file,
            sd_available,
            &png,
            &png_size);

        uint16_t *rendered = NULL;
        bool rendered_ok = false;

        if (fetched) {
            rendered = heap_caps_malloc(
                ROW_PREVIEW_WIDTH * ROW_PREVIEW_HEIGHT * sizeof(uint16_t),
                MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

            if (!rendered) {
                rendered = heap_caps_malloc(
                    ROW_PREVIEW_WIDTH * ROW_PREVIEW_HEIGHT * sizeof(uint16_t),
                    MALLOC_CAP_8BIT);
            }

            lv_image_dsc_t raw_png;
            memset(&raw_png, 0, sizeof(raw_png));
#if defined(LV_IMAGE_HEADER_MAGIC)
            raw_png.header.magic = LV_IMAGE_HEADER_MAGIC;
#endif
            raw_png.header.cf = LV_COLOR_FORMAT_RAW;
            raw_png.data = png;
            raw_png.data_size = png_size;

            if (rendered && bsp_display_lock(2500)) {
                rendered_ok = thumbnail_render_v32_to_rgb565(
                    &raw_png,
                    rendered,
                    ROW_PREVIEW_WIDTH,
                    ROW_PREVIEW_HEIGHT);
                bsp_display_unlock();
            }
        }

        if (png) heap_caps_free(png);

        const lv_image_dsc_t *ready_image = NULL;
        files_row_preview_v32_ready_cb_t ready_cb = NULL;

        xSemaphoreTake(s_lock, portMAX_DELAY);

        current =
            job.generation == s_generation &&
            s_slots[job.slot].generation == job.generation &&
            strcmp(s_slots[job.slot].file, file) == 0;

        if (current && rendered_ok) {
            if (!s_slots[job.slot].pixels) {
                s_slots[job.slot].pixels = heap_caps_malloc(
                    ROW_PREVIEW_WIDTH * ROW_PREVIEW_HEIGHT * sizeof(uint16_t),
                    MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

                if (!s_slots[job.slot].pixels) {
                    s_slots[job.slot].pixels = heap_caps_malloc(
                        ROW_PREVIEW_WIDTH * ROW_PREVIEW_HEIGHT * sizeof(uint16_t),
                        MALLOC_CAP_8BIT);
                }
            }

            if (s_slots[job.slot].pixels) {
                memcpy(
                    s_slots[job.slot].pixels,
                    rendered,
                    ROW_PREVIEW_WIDTH * ROW_PREVIEW_HEIGHT * sizeof(uint16_t));

                lv_image_dsc_t *image = &s_slots[job.slot].image;
                memset(image, 0, sizeof(*image));
#if defined(LV_IMAGE_HEADER_MAGIC)
                image->header.magic = LV_IMAGE_HEADER_MAGIC;
#endif
                image->header.cf = LV_COLOR_FORMAT_RGB565;
                image->header.w = ROW_PREVIEW_WIDTH;
                image->header.h = ROW_PREVIEW_HEIGHT;
                image->header.stride =
                    ROW_PREVIEW_WIDTH * sizeof(uint16_t);
                image->data_size =
                    ROW_PREVIEW_WIDTH * ROW_PREVIEW_HEIGHT * sizeof(uint16_t);
                image->data = (const uint8_t *)s_slots[job.slot].pixels;
                s_slots[job.slot].state = ROW_PREVIEW_READY;
                ready_image = image;
                ready_cb = s_ready_cb;
            } else {
                s_slots[job.slot].state = ROW_PREVIEW_FAILED;
            }
        } else if (current) {
            s_slots[job.slot].state = ROW_PREVIEW_FAILED;
        }

        xSemaphoreGive(s_lock);

        if (rendered) heap_caps_free(rendered);

        if (ready_image && ready_cb && bsp_display_lock(1000)) {
            xSemaphoreTake(s_lock, portMAX_DELAY);
            bool still_current =
                job.generation == s_generation &&
                s_slots[job.slot].state == ROW_PREVIEW_READY &&
                strcmp(s_slots[job.slot].file, file) == 0;
            xSemaphoreGive(s_lock);

            if (still_current) {
                ready_cb(file, ready_image);
            }
            bsp_display_unlock();
        }
    }
}

void files_row_preview_v32_begin(
    const char *host,
    int port,
    const char *api_key,
    bool sd_available,
    files_row_preview_v32_ready_cb_t ready_cb)
{
    if (!ensure_runtime()) {
        ESP_LOGE(TAG, "Runtime allocation failed");
        return;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);

    s_generation++;
    if (s_generation == 0) s_generation = 1;

    strlcpy(s_host, host ? host : "", sizeof(s_host));
    strlcpy(s_api_key, api_key ? api_key : "", sizeof(s_api_key));
    s_port = port;
    s_sd_available = sd_available;
    s_ready_cb = ready_cb;

    for (int index = 0; index < ROW_PREVIEW_SLOT_COUNT; ++index) {
        s_slots[index].file[0] = '\0';
        s_slots[index].generation = s_generation;
        s_slots[index].state = ROW_PREVIEW_EMPTY;
    }

    xQueueReset(s_queue);
    xSemaphoreGive(s_lock);

    if (!s_worker_started) {
        BaseType_t result = xTaskCreatePinnedToCore(
            preview_worker,
            "files_preview",
            8192,
            NULL,
            3,
            NULL,
            0);

        s_worker_started = result == pdPASS;
        if (!s_worker_started) {
            ESP_LOGE(TAG, "Worker task creation failed");
        }
    }
}

void files_row_preview_v32_request(const char *file)
{
    if (!file || !file[0] || !ensure_runtime() || !s_worker_started) {
        return;
    }

    row_preview_job_t job = {
        .slot = -1,
        .generation = 0,
    };
    const lv_image_dsc_t *ready_image = NULL;
    files_row_preview_v32_ready_cb_t ready_cb = NULL;

    xSemaphoreTake(s_lock, portMAX_DELAY);

    for (int index = 0; index < ROW_PREVIEW_SLOT_COUNT; ++index) {
        if (s_slots[index].generation == s_generation &&
            s_slots[index].state != ROW_PREVIEW_EMPTY &&
            strcmp(s_slots[index].file, file) == 0) {
            if (s_slots[index].state == ROW_PREVIEW_READY) {
                ready_image = &s_slots[index].image;
                ready_cb = s_ready_cb;
            }
            xSemaphoreGive(s_lock);

            if (ready_image && ready_cb) {
                ready_cb(file, ready_image);
            }
            return;
        }
    }

    for (int index = 0; index < ROW_PREVIEW_SLOT_COUNT; ++index) {
        if (s_slots[index].state == ROW_PREVIEW_EMPTY) {
            strlcpy(s_slots[index].file, file, sizeof(s_slots[index].file));
            s_slots[index].generation = s_generation;
            s_slots[index].state = ROW_PREVIEW_QUEUED;
            job.slot = index;
            job.generation = s_generation;
            break;
        }
    }

    xSemaphoreGive(s_lock);

    if (job.slot >= 0 &&
        xQueueSend(s_queue, &job, 0) != pdTRUE) {
        xSemaphoreTake(s_lock, portMAX_DELAY);
        if (s_slots[job.slot].generation == job.generation &&
            s_slots[job.slot].state == ROW_PREVIEW_QUEUED) {
            s_slots[job.slot].file[0] = '\0';
            s_slots[job.slot].state = ROW_PREVIEW_EMPTY;
        }
        xSemaphoreGive(s_lock);
    }
}
