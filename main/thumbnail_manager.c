#include "thumbnail_manager.h"
#include "thumbnail_cache_io.h"

#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "esp_heap_caps.h"

#include "moonraker.h"
#include "moonraker_config_controller.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"


static const char *TAG = "thumbnail_manager";

static SemaphoreHandle_t s_cache_mutex;
static portMUX_TYPE s_cache_mutex_guard = portMUX_INITIALIZER_UNLOCKED;

static bool cache_lock(void)
{
    portENTER_CRITICAL(&s_cache_mutex_guard);
    SemaphoreHandle_t mutex = s_cache_mutex;
    portEXIT_CRITICAL(&s_cache_mutex_guard);
    if (!mutex) {
        SemaphoreHandle_t candidate = xSemaphoreCreateMutex();
        if (!candidate) return false;
        portENTER_CRITICAL(&s_cache_mutex_guard);
        if (!s_cache_mutex) s_cache_mutex = candidate;
        mutex = s_cache_mutex;
        portEXIT_CRITICAL(&s_cache_mutex_guard);
        if (candidate != mutex) vSemaphoreDelete(candidate);
    }
    return xSemaphoreTake(mutex, pdMS_TO_TICKS(5000)) == pdTRUE;
}

static void cache_unlock(void)
{
    xSemaphoreGive(s_cache_mutex);
}


static portMUX_TYPE s_download_lock = portMUX_INITIALIZER_UNLOCKED;
static bool s_thumb_task_running, s_thumb_ready, s_thumb_failed, s_thumb_force_refresh;
static uint32_t s_request_revision, s_completed_generation;
static uint8_t *s_completed_png;
static size_t s_completed_len;
static char s_completed_file[160];

static void invalidate_download(bool failed)
{
    portENTER_CRITICAL(&s_download_lock);
    ++s_request_revision;
    uint8_t *discarded = s_completed_png;
    s_completed_png = NULL;
    s_thumb_ready = false;
    s_thumb_failed = failed;
    s_completed_generation = moonraker_config_generation();
    portEXIT_CRITICAL(&s_download_lock);
    if (discarded) heap_caps_free(discarded);
}

void thumbnail_manager_mark_pending(void) { invalidate_download(false); }
void thumbnail_manager_mark_failed(void) { invalidate_download(true); }

bool thumbnail_manager_task_running(void)
{
    portENTER_CRITICAL(&s_download_lock);
    bool running = s_thumb_task_running;
    portEXIT_CRITICAL(&s_download_lock);
    return running;
}

thumbnail_manager_result_t thumbnail_manager_result(void)
{
    uint32_t generation = moonraker_config_generation();
    portENTER_CRITICAL(&s_download_lock);
    thumbnail_manager_result_t result = THUMBNAIL_MANAGER_RESULT_IDLE;
    if (s_thumb_task_running) result = THUMBNAIL_MANAGER_RESULT_LOADING;
    else if (s_completed_generation == generation) {
        if (s_thumb_failed) result = THUMBNAIL_MANAGER_RESULT_FAILED;
        else if (s_thumb_ready) result = THUMBNAIL_MANAGER_RESULT_READY;
    }
    portEXIT_CRITICAL(&s_download_lock);
    return result;
}

bool thumbnail_manager_publish_download(const char *file)
{
    uint32_t generation = moonraker_config_generation();
    portENTER_CRITICAL(&s_download_lock);
    uint8_t *png = s_completed_png;
    size_t size = s_completed_len;
    bool matches = png && s_completed_generation == generation &&
                   file && !strcmp(file, s_completed_file);
    s_completed_png = NULL;
    if (!matches) s_thumb_ready = false;
    portEXIT_CRITICAL(&s_download_lock);
    if (!matches) {
        if (png) heap_caps_free(png);
        return false;
    }
    thumbnail_manager_take_png(png, size);
    thumbnail_manager_prepare_raw_image();
    return true;
}

bool thumbnail_manager_force_refresh(void)
{
    portENTER_CRITICAL(&s_download_lock);
    bool refresh = s_thumb_force_refresh;
    portEXIT_CRITICAL(&s_download_lock);
    return refresh;
}

void thumbnail_manager_set_force_refresh(bool force_refresh)
{
    portENTER_CRITICAL(&s_download_lock);
    s_thumb_force_refresh = force_refresh;
    portEXIT_CRITICAL(&s_download_lock);
}

void thumbnail_manager_url_encode(const char *in, char *out, size_t out_sz)
{
    if (!out || !out_sz) return;
    size_t oi = 0;
    static const char hex[] = "0123456789ABCDEF";

    for (size_t i = 0; in && in[i]; i++) {
        unsigned char c = (unsigned char)in[i];

        if ((c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') ||
            c == '/' || c == '.' || c == '_' || c == '-') {
            if (out_sz - oi <= 1) { out[0] = 0; return; }
            out[oi++] = c;
        } else {
            if (out_sz - oi <= 3) { out[0] = 0; return; }
            out[oi++] = '%';
            out[oi++] = hex[(c >> 4) & 0x0F];
            out[oi++] = hex[c & 0x0F];
        }
    }

    out[oi] = 0;
}

static uint64_t cache_hash_field(uint64_t hash, const char *field)
{
    /* Include each terminating NUL to keep adjacent fields unambiguous. */
    do {
        hash ^= (unsigned char)*field;
        hash *= UINT64_C(1099511628211);
    } while (*field++);
    return hash;
}

static bool cache_path_for_thumbnail(const char *host, int port,
                                     const char *file, const char *thumbnail,
                                     char *out, size_t capacity)
{
    if (!out || !capacity) return false;
    out[0] = 0;
    if (!host || !*host || port < 1 || port > 65535 ||
        !file || !*file || !thumbnail || !*thumbnail) return false;

    char port_text[6];
    snprintf(port_text, sizeof(port_text), "%u", (unsigned)(uint16_t)port);
    uint64_t hash = cache_hash_field(UINT64_C(14695981039346656037), host);
    hash = cache_hash_field(hash, port_text);
    hash = cache_hash_field(hash, file);
    hash = cache_hash_field(hash, thumbnail);
    int written = snprintf(out, capacity, "/sdcard/hmi/thumbs/v2-%016" PRIx64 ".png", hash);
    if (written >= 0 && (size_t)written < capacity) return true;
    out[0] = 0;
    return false;
}


static bool load_cache_file_locked(const char *path,
                                           uint8_t **out_buf,
                                           size_t *out_len)
{
    if (!path || !path[0] || !out_buf || !out_len) return false;

    *out_buf = NULL;
    *out_len = 0;

    FILE *f = fopen(path, "rb");
    if (!f) return false;

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return false;
    }

    long len = ftell(f);
    if (len <= 0 || len > (long)THUMBNAIL_CACHE_MAX_PNG_BYTES) {
        fclose(f);
        return false;
    }

    rewind(f);

    uint8_t *buf = heap_caps_malloc((size_t)len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf) buf = heap_caps_malloc((size_t)len, MALLOC_CAP_8BIT);
    if (!buf) {
        fclose(f);
        return false;
    }

    size_t rd = fread(buf, 1, (size_t)len, f);
    fclose(f);

    if (rd != (size_t)len) {
        heap_caps_free(buf);
        return false;
    }

    *out_buf = buf;
    *out_len = (size_t)len;
    return true;
}


bool thumbnail_manager_load_cache_file(const char *path,
                                       uint8_t **out_buf, size_t *out_len)
{
    if (!out_buf || !out_len) return false;
    *out_buf = NULL;
    *out_len = 0;
    if (!cache_lock()) return false;
    bool ok = load_cache_file_locked(path, out_buf, out_len);
    cache_unlock();
    return ok;
}

bool thumbnail_manager_store_cache_file(const char *path,
                                        const uint8_t *buf, size_t len)
{
    if (!cache_lock()) return false;
    mkdir("/sdcard/hmi", 0775);
    mkdir("/sdcard/hmi/thumbs", 0775);
    mkdir("/sdcard/hmi/thumbs32", 0775);
    bool ok = thumbnail_cache_write(path, buf, len);
    cache_unlock();
    return ok;
}

static bool download_thumbnail(const char *host,
                                       int port,
                                       const char *selected_file,
                                       const char *thumb_path,
                                       bool force_refresh,
                                       bool sd_ok, uint8_t **out, size_t *out_len)
{
    char cache_path[64];
    bool use_cache = sd_ok && cache_path_for_thumbnail(host, port, selected_file,
        thumb_path, cache_path, sizeof(cache_path));
    if (use_cache && !force_refresh) {
        uint8_t *cached = NULL;
        size_t cached_len = 0;
        if (thumbnail_manager_load_cache_file(cache_path, &cached, &cached_len)) {
            *out = cached;
            *out_len = cached_len;
            return true;
        }
    }

    char enc[768];
    thumbnail_manager_url_encode(thumb_path, enc, sizeof(enc));

    uint8_t *buf = NULL;
    size_t len = 0;

    if (!moonraker_fetch_thumbnail_encoded_quiet(
            host,
            port,
            enc,
            &buf,
            &len)) {
        ESP_LOGW(TAG, "THUMB fetch failed");
        return false;
    }

    *out = buf;
    *out_len = len;

    if (use_cache && !thumbnail_manager_store_cache_file(cache_path, buf, len)) {
        ESP_LOGW(TAG, "SD_THUMB cache write failed");
    }

    return true;
}


static uint8_t *s_thumb_png = NULL;
static size_t s_thumb_png_len = 0;
static lv_image_dsc_t s_thumb_img_dsc;


lv_image_dsc_t *thumbnail_manager_image_dsc(void)
{
    return &s_thumb_img_dsc;
}

void thumbnail_manager_prepare_raw_image(void)
{
    memset(&s_thumb_img_dsc, 0, sizeof(s_thumb_img_dsc));
#if defined(LV_IMAGE_HEADER_MAGIC)
    s_thumb_img_dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
#endif
    s_thumb_img_dsc.header.cf = LV_COLOR_FORMAT_RAW;
    s_thumb_img_dsc.data = thumbnail_manager_png_data();
    s_thumb_img_dsc.data_size = thumbnail_manager_png_size();
}

uint8_t *thumbnail_manager_png_data(void)
{
    return s_thumb_png;
}

size_t thumbnail_manager_png_size(void)
{
    return s_thumb_png_len;
}

bool thumbnail_manager_has_png(void)
{
    return s_thumb_png && s_thumb_png_len > 0;
}

void thumbnail_manager_clear_png(void)
{
    if(s_thumb_png)heap_caps_free(s_thumb_png);
    s_thumb_png=NULL;s_thumb_png_len=0;
}

void thumbnail_manager_take_png(uint8_t *buf, size_t len)
{
    thumbnail_manager_clear_png();
    s_thumb_png=buf;s_thumb_png_len=len;
}

typedef struct {
    char host[128];
    char selected_file[160];
    char thumb_path[192];

    int port;
    uint32_t revision, generation;
    bool force_refresh;
    bool sd_ok;
} thumbnail_download_job_t;

/* Called with the download lock held. */
static bool download_is_current(const thumbnail_download_job_t *job)
{
    return job->revision == s_request_revision &&
           job->generation == moonraker_config_generation();
}

static void thumbnail_manager_download_task(void *arg)
{
    thumbnail_download_job_t *job = arg;
    uint8_t *png = NULL;
    size_t size = 0;
    bool ok = false;
    for (unsigned attempt = 0; attempt < 3; ++attempt) {
        portENTER_CRITICAL(&s_download_lock);
        bool current = download_is_current(job);
        portEXIT_CRITICAL(&s_download_lock);
        if (!current) break;
        ok = download_thumbnail(job->host, job->port, job->selected_file,
            job->thumb_path, job->force_refresh, job->sd_ok, &png, &size);
        if (ok) break;
        portENTER_CRITICAL(&s_download_lock);
        current = download_is_current(job);
        portEXIT_CRITICAL(&s_download_lock);
        if (current && attempt + 1 < 3) vTaskDelay(pdMS_TO_TICKS(500));
    }

    portENTER_CRITICAL(&s_download_lock);
    if (download_is_current(job)) {
        s_thumb_force_refresh = false;
        s_completed_generation = job->generation;
        s_thumb_ready = ok;
        s_thumb_failed = !ok;
        if (ok) {
            s_completed_png = png;
            s_completed_len = size;
            memcpy(s_completed_file, job->selected_file, sizeof(s_completed_file));
            png = NULL;
        }
    }
    s_thumb_task_running = false;
    portEXIT_CRITICAL(&s_download_lock);
    if (png) heap_caps_free(png);
    heap_caps_free(job);
    vTaskDelete(NULL);
}

bool thumbnail_manager_start_download_task(
    const char *host,
    int port,
    const char *selected_file,
    const char *thumb_path,
    bool force_refresh,
    bool sd_ok)
{
    if (!host || !host[0] || strlen(host) >= sizeof(((thumbnail_download_job_t *)0)->host) ||
        port < 1 || port > 65535 ||
        !selected_file || !selected_file[0] || strlen(selected_file) >= sizeof(((thumbnail_download_job_t *)0)->selected_file) ||
        !thumb_path || !thumb_path[0] || strlen(thumb_path) >= sizeof(((thumbnail_download_job_t *)0)->thumb_path)) {
        return false;
    }

    if (thumbnail_manager_task_running()) {
        return false;
    }

    thumbnail_download_job_t *job =
        heap_caps_calloc(
            1,
            sizeof(*job),
            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (!job) {
        job = heap_caps_calloc(
            1,
            sizeof(*job),
            MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }

    if (!job) {
        return false;
    }

    strlcpy(job->host,
            host,
            sizeof(job->host));

    strlcpy(job->selected_file,
            selected_file,
            sizeof(job->selected_file));

    strlcpy(job->thumb_path,
            thumb_path,
            sizeof(job->thumb_path));

    job->port = port;
    job->force_refresh = force_refresh;
    job->sd_ok = sd_ok;

    portENTER_CRITICAL(&s_download_lock);
    if (s_thumb_task_running) {
        portEXIT_CRITICAL(&s_download_lock);
        heap_caps_free(job);
        return false;
    }
    uint8_t *discarded = s_completed_png;
    s_completed_png = NULL;
    s_thumb_ready = s_thumb_failed = false;
    job->revision = ++s_request_revision;
    job->generation = moonraker_config_generation();
    s_thumb_task_running = true;
    portEXIT_CRITICAL(&s_download_lock);
    if (discarded) heap_caps_free(discarded);

    BaseType_t rc = xTaskCreatePinnedToCore(
        thumbnail_manager_download_task,
        "thumb_dl",
        8192,
        job,
        4,
        NULL,
        0);

    if (rc != pdPASS) {
        portENTER_CRITICAL(&s_download_lock);
        s_thumb_task_running = false;
        portEXIT_CRITICAL(&s_download_lock);
        heap_caps_free(job);
        return false;
    }

    return true;
}
