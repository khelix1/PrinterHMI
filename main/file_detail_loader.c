#include "file_detail_loader.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bsp/esp-bsp.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "moonraker.h"
#include "printer_files.h"
#include "thumbnail_manager.h"

#define DETAIL_METADATA_BODY_SIZE 8192

typedef struct {
    char host[128];
    char api_key[128];
    char file[160];
    int port;
    uint32_t generation;
    file_detail_loader_ready_cb_t ready_cb;
} detail_job_t;

static portMUX_TYPE s_generation_lock = portMUX_INITIALIZER_UNLOCKED;
static uint32_t s_generation;

static uint32_t next_generation(void)
{
    taskENTER_CRITICAL(&s_generation_lock);
    s_generation++;
    if (s_generation == 0) s_generation = 1;
    uint32_t value = s_generation;
    taskEXIT_CRITICAL(&s_generation_lock);
    return value;
}

static bool generation_current(uint32_t generation)
{
    taskENTER_CRITICAL(&s_generation_lock);
    bool current = generation == s_generation;
    taskEXIT_CRITICAL(&s_generation_lock);
    return current;
}

void file_detail_loader_cancel(void)
{
    (void)next_generation();
}

static void detail_task(void *argument)
{
    detail_job_t *job = (detail_job_t *)argument;
    char *body = NULL;
    char metadata[1024] = "Metadata unavailable.";
    char thumbnail[192] = "";
    bool ok = false;

    if (job) {
        body = heap_caps_calloc(
            1,
            DETAIL_METADATA_BODY_SIZE,
            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!body) {
            body = heap_caps_calloc(
                1,
                DETAIL_METADATA_BODY_SIZE,
                MALLOC_CAP_8BIT);
        }
    }

    if (job && body) {
        char encoded[256];
        thumbnail_manager_url_encode(job->file, encoded, sizeof(encoded));
        int http_code = 0;
        esp_err_t error = ESP_FAIL;

        if (moonraker_fetch_file_metadata(
                job->host,
                job->port,
                job->api_key,
                encoded,
                body,
                DETAIL_METADATA_BODY_SIZE,
                &http_code,
                &error)) {
            ok = printer_files_build_metadata_text(
                job->file,
                body,
                thumbnail,
                sizeof(thumbnail),
                metadata,
                sizeof(metadata));
        } else {
            snprintf(metadata,
                     sizeof(metadata),
                     "Metadata request failed.\nHTTP %d\n%s",
                     http_code,
                     esp_err_to_name(error));
        }
    }

    if (body) heap_caps_free(body);

    if (job && job->ready_cb && generation_current(job->generation) &&
        bsp_display_lock(1500)) {
        if (generation_current(job->generation)) {
            job->ready_cb(job->file, ok, metadata, thumbnail);
        }
        bsp_display_unlock();
    }

    if (job) heap_caps_free(job);
    vTaskDelete(NULL);
}

bool file_detail_loader_start(
    const char *host,
    int port,
    const char *api_key,
    const char *file,
    file_detail_loader_ready_cb_t ready_cb)
{
    if (!host || !host[0] || port <= 0 || !file || !file[0] || !ready_cb) {
        return false;
    }

    detail_job_t *job = heap_caps_calloc(
        1, sizeof(*job), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!job) {
        job = heap_caps_calloc(1, sizeof(*job), MALLOC_CAP_8BIT);
    }
    if (!job) return false;

    strlcpy(job->host, host, sizeof(job->host));
    strlcpy(job->api_key, api_key ? api_key : "", sizeof(job->api_key));
    strlcpy(job->file, file, sizeof(job->file));
    job->port = port;
    job->ready_cb = ready_cb;
    job->generation = next_generation();

    if (xTaskCreatePinnedToCore(
            detail_task,
            "file_detail",
            8192,
            job,
            4,
            NULL,
            0) != pdPASS) {
        heap_caps_free(job);
        return false;
    }

    return true;
}
