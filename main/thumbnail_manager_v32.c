#include "thumbnail_manager_v32.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "esp_heap_caps.h"

#include "moonraker.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define THUMB_CACHE_DIR "/sdcard/cache"
#define THUMB_MAX_FILE 160
#define THUMB_MAX_PATH 240

static const char *TAG = "thumbnail_manager_v32";


static volatile bool s_thumb_task_running = false;
static volatile bool s_thumb_ready = false;
static volatile bool s_thumb_failed = false;
static volatile bool s_thumb_force_refresh = false;

void thumbnail_manager_v32_mark_pending(void)
{
    s_thumb_ready = false;
    s_thumb_failed = false;
}

void thumbnail_manager_v32_mark_ready(void)
{
    s_thumb_ready = true;
    s_thumb_failed = false;
}

void thumbnail_manager_v32_mark_failed(void)
{
    s_thumb_ready = false;
    s_thumb_failed = true;
}

void thumbnail_manager_v32_mark_result(bool ok)
{
    s_thumb_ready = ok;
    s_thumb_failed = !ok;
}

bool thumbnail_manager_v32_is_ready(void)
{
    return s_thumb_ready;
}

bool thumbnail_manager_v32_has_failed(void)
{
    return s_thumb_failed;
}

bool thumbnail_manager_v32_task_running(void)
{
    return s_thumb_task_running;
}

void thumbnail_manager_v32_set_task_running(bool running)
{
    s_thumb_task_running = running;
}


thumbnail_manager_v32_result_t
thumbnail_manager_v32_result(void)
{
    if (s_thumb_task_running) {
        return THUMBNAIL_MANAGER_V32_RESULT_LOADING;
    }

    if (s_thumb_failed) {
        return THUMBNAIL_MANAGER_V32_RESULT_FAILED;
    }

    if (s_thumb_ready && thumbnail_manager_v32_has_png()) {
        return THUMBNAIL_MANAGER_V32_RESULT_READY;
    }

    return THUMBNAIL_MANAGER_V32_RESULT_IDLE;
}

bool thumbnail_manager_v32_force_refresh(void)
{
    return s_thumb_force_refresh;
}

void thumbnail_manager_v32_set_force_refresh(bool force_refresh)
{
    s_thumb_force_refresh = force_refresh;
}


static thumbnail_v32_state_t s_state = THUMBNAIL_V32_STATE_IDLE;
static char s_file[THUMB_MAX_FILE];
static char s_cache_path[THUMB_MAX_PATH];
static char s_status[96];

static void thumb_set_status(const char *text)
{
    snprintf(s_status, sizeof(s_status), "%s", text ? text : "");
}

static void thumb_build_cache_path(const char *gcode_file)
{
    s_cache_path[0] = 0;

    if (!gcode_file || !gcode_file[0]) {
        return;
    }

    const char *base = strrchr(gcode_file, '/');
    base = base ? base + 1 : gcode_file;

    char safe[96];
    size_t j = 0;

    for (size_t i = 0; base[i] && j < sizeof(safe) - 1; i++) {
        char c = base[i];

        if ((c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == '_' || c == '-') {
            safe[j++] = c;
        } else if (c == '.') {
            safe[j++] = '_';
        } else {
            safe[j++] = '_';
        }
    }

    safe[j] = 0;

    if (safe[0] == 0) {
        snprintf(safe, sizeof(safe), "thumbnail");
    }

    snprintf(s_cache_path, sizeof(s_cache_path), "%s/%s.png", THUMB_CACHE_DIR, safe);
}

void thumbnail_manager_v32_init(void)
{
    s_file[0] = 0;
    s_cache_path[0] = 0;
    s_state = THUMBNAIL_V32_STATE_IDLE;
    thumb_set_status("Thumbnail idle");
}

void thumbnail_manager_v32_set_file(const char *gcode_file)
{
    if (!gcode_file || !gcode_file[0]) {
        thumbnail_manager_v32_clear();
        s_state = THUMBNAIL_V32_STATE_NO_FILE;
        thumb_set_status("No file selected");
        return;
    }

    snprintf(s_file, sizeof(s_file), "%s", gcode_file);
    thumb_build_cache_path(s_file);

    /*
     * Phase 1 only:
     * We are not downloading or checking SD yet.
     * This proves the manager owns file/cache state safely.
     */
    s_state = THUMBNAIL_V32_STATE_PENDING;
    thumb_set_status("Thumbnail pending");
}

void thumbnail_manager_v32_clear(void)
{
    s_file[0] = 0;
    s_cache_path[0] = 0;
    s_state = THUMBNAIL_V32_STATE_IDLE;
    thumb_set_status("Thumbnail idle");
}

void thumbnail_manager_v32_set_ready_image(const char *cache_path)
{
    if (!cache_path || !cache_path[0]) {
        s_state = THUMBNAIL_V32_STATE_ERROR;
        thumb_set_status("Thumbnail ready path missing");
        return;
    }

    snprintf(s_cache_path, sizeof(s_cache_path), "%s", cache_path);
    s_state = THUMBNAIL_V32_STATE_READY;
    thumb_set_status("Thumbnail ready");
}

void thumbnail_manager_v32_set_error(const char *status_text)
{
    s_state = THUMBNAIL_V32_STATE_ERROR;
    thumb_set_status(status_text ? status_text : "Thumbnail error");
}

thumbnail_v32_state_t thumbnail_manager_v32_state(void)
{
    return s_state;
}

const char *thumbnail_manager_v32_file(void)
{
    return s_file;
}

const char *thumbnail_manager_v32_cache_path(void)
{
    return s_cache_path;
}

const char *thumbnail_manager_v32_status_text(void)
{
    return s_status;
}

bool thumbnail_manager_v32_has_ready_image(void)
{
    return s_state == THUMBNAIL_V32_STATE_READY && s_cache_path[0] != 0;
}

bool thumbnail_manager_v32_copy_cache_path(char *out, size_t out_len)
{
    if (!out || out_len == 0 || s_cache_path[0] == 0) {
        return false;
    }

    snprintf(out, out_len, "%s", s_cache_path);
    return true;
}

void thumbnail_manager_v32_url_encode(const char *in, char *out, size_t out_sz)
{
    size_t oi = 0;
    static const char hex[] = "0123456789ABCDEF";

    for (size_t i = 0; in && in[i] && oi + 4 < out_sz; i++) {
        unsigned char c = (unsigned char)in[i];

        if ((c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') ||
            c == '/' || c == '.' || c == '_' || c == '-') {
            out[oi++] = c;
        } else {
            out[oi++] = '%';
            out[oi++] = hex[(c >> 4) & 0x0F];
            out[oi++] = hex[c & 0x0F];
        }
    }

    out[oi] = 0;
}

bool thumbnail_manager_v32_cache_path_for_file(const char *gcode_file,
                                               char *out,
                                               size_t out_sz)
{
    if (!out || out_sz == 0 || !gcode_file || !gcode_file[0]) return false;

    char safe[160];
    size_t j = 0;

    for (size_t i = 0; gcode_file && gcode_file[i] && j < sizeof(safe) - 1; i++) {
        char c = gcode_file[i];
        if ((c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == '-' || c == '_') {
            safe[j++] = c;
        } else {
            safe[j++] = '_';
        }
    }

    safe[j] = 0;
    snprintf(out, out_sz, "/sdcard/hmi/thumbs/%s.png", safe);
    return true;
}


bool thumbnail_manager_v32_load_cache_file(const char *path,
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
    if (len <= 0 || len > 64 * 1024) {
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


bool thumbnail_manager_v32_store_cache_file(const char *path,
                                            const uint8_t *buf,
                                            size_t len)
{
    if (!path || !path[0] || !buf || len == 0) return false;

    mkdir("/sdcard/hmi", 0775);
    mkdir("/sdcard/hmi/thumbs", 0775);
    mkdir("/sdcard/hmi/thumbs32", 0775);

    FILE *f = fopen(path, "wb");
    if (!f) {
        return false;
    }

    size_t wr = fwrite(buf, 1, len, f);
    int cr = fclose(f);

    return wr == len && cr == 0;
}










bool thumbnail_manager_v32_run_download_task(void *arg,
                                             const char *host,
                                             int port,
                                             const char *selected_file,
                                             bool force_refresh,
                                             bool sd_ok)
{
    const char *path_arg = (const char *)arg;
    char path[192];

    snprintf(path, sizeof(path), "%s", path_arg ? path_arg : "");

    return thumbnail_manager_v32_download_ram(host,
                                              port,
                                              selected_file,
                                              path,
                                              force_refresh,
                                              sd_ok);
}





bool thumbnail_manager_v32_download_ram(const char *host,
                                       int port,
                                       const char *selected_file,
                                       const char *thumb_path,
                                       bool force_refresh,
                                       bool sd_ok)
{
    if (!thumb_path || !thumb_path[0]) {
        return false;
    }

    if (!force_refresh && sd_ok &&
        thumbnail_manager_v32_load_selected_cache(selected_file)) {
        return true;
    }

    char enc[256];
    thumbnail_manager_v32_url_encode(thumb_path, enc, sizeof(enc));

    uint8_t *buf = NULL;
    size_t len = 0;

    if (!moonraker_fetch_thumbnail_encoded(host,
                                           port,
                                           enc,
                                           &buf,
                                           &len)) {
        ESP_LOGW(TAG, "THUMB fetch failed");
        return false;
    }

    thumbnail_manager_v32_take_png(buf, len);
    thumbnail_manager_v32_prepare_raw_image();

    ESP_LOGI(TAG, "THUMB loaded len=%u", (unsigned)thumbnail_manager_v32_png_size());

    if (sd_ok && !thumbnail_manager_v32_store_selected_cache(selected_file)) {
        ESP_LOGW(TAG, "SD_THUMB cache write failed");
    }

    return true;
}


bool thumbnail_manager_v32_load_selected_cache(const char *selected_file)
{
    char path[220];

    if (!thumbnail_manager_v32_cache_path_for_file(selected_file,
                                                   path,
                                                   sizeof(path))) {
        return false;
    }

    uint8_t *buf = NULL;
    size_t len = 0;

    if (!thumbnail_manager_v32_load_cache_file(path, &buf, &len)) {
        return false;
    }

    thumbnail_manager_v32_take_png(buf, len);
    thumbnail_manager_v32_prepare_raw_image();

    return true;
}

bool thumbnail_manager_v32_store_selected_cache(const char *selected_file)
{
    if (!thumbnail_manager_v32_has_png() || !selected_file || !selected_file[0]) {
        return false;
    }

    char path[220];

    if (!thumbnail_manager_v32_cache_path_for_file(selected_file,
                                                   path,
                                                   sizeof(path))) {
        return false;
    }

    return thumbnail_manager_v32_store_cache_file(path,
                                                  thumbnail_manager_v32_png_data(),
                                                  thumbnail_manager_v32_png_size());
}


static uint8_t *s_thumb_png = NULL;
static size_t s_thumb_png_len = 0;
static lv_image_dsc_t s_thumb_img_dsc;


lv_image_dsc_t *thumbnail_manager_v32_image_dsc(void)
{
    return &s_thumb_img_dsc;
}

void thumbnail_manager_v32_prepare_raw_image(void)
{
    memset(&s_thumb_img_dsc, 0, sizeof(s_thumb_img_dsc));
#if defined(LV_IMAGE_HEADER_MAGIC)
    s_thumb_img_dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
#endif
    s_thumb_img_dsc.header.cf = LV_COLOR_FORMAT_RAW;
    s_thumb_img_dsc.data = thumbnail_manager_v32_png_data();
    s_thumb_img_dsc.data_size = thumbnail_manager_v32_png_size();
}

uint8_t *thumbnail_manager_v32_png_data(void)
{
    return s_thumb_png;
}

size_t thumbnail_manager_v32_png_size(void)
{
    return s_thumb_png_len;
}

bool thumbnail_manager_v32_has_png(void)
{
    return s_thumb_png && s_thumb_png_len > 0;
}

void thumbnail_manager_v32_clear_png(void)
{
    thumbnail_manager_v32_free_png_buffer(&s_thumb_png, &s_thumb_png_len);
}

void thumbnail_manager_v32_take_png(uint8_t *buf, size_t len)
{
    thumbnail_manager_v32_clear_png();
    thumbnail_manager_v32_set_png_buffer(&s_thumb_png, &s_thumb_png_len, buf, len);
}

void thumbnail_manager_v32_free_png_buffer(uint8_t **buf, size_t *len)
{
    if (buf && *buf) {
        heap_caps_free(*buf);
        *buf = NULL;
    }

    if (len) {
        *len = 0;
    }
}


void thumbnail_manager_v32_set_png_buffer(uint8_t **dst_buf,
                                          size_t *dst_len,
                                          uint8_t *src_buf,
                                          size_t src_len)
{
    if (dst_buf) {
        *dst_buf = src_buf;
    }

    if (dst_len) {
        *dst_len = src_len;
    }
}

typedef struct {
    char host[128];
    char selected_file[160];
    char thumb_path[192];

    int port;
    bool force_refresh;
    bool sd_ok;
} thumbnail_download_job_v32_t;

static void thumbnail_manager_v32_download_task(void *arg)
{
    thumbnail_download_job_v32_t *job =
        (thumbnail_download_job_v32_t *)arg;

    bool ok = false;

    if (job) {
        ok = thumbnail_manager_v32_run_download_task(
            job->thumb_path,
            job->host,
            job->port,
            job->selected_file,
            job->force_refresh,
            job->sd_ok);
    }

    thumbnail_manager_v32_set_force_refresh(false);
    thumbnail_manager_v32_mark_result(ok);
    thumbnail_manager_v32_set_task_running(false);

    if (job) {
        heap_caps_free(job);
    }

    vTaskDelete(NULL);
}

bool thumbnail_manager_v32_start_download_task(
    const char *host,
    int port,
    const char *selected_file,
    const char *thumb_path,
    bool force_refresh,
    bool sd_ok)
{
    if (!host || !host[0] ||
        port <= 0 ||
        !selected_file || !selected_file[0] ||
        !thumb_path || !thumb_path[0]) {
        return false;
    }

    if (thumbnail_manager_v32_task_running()) {
        return false;
    }

    thumbnail_download_job_v32_t *job =
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

    thumbnail_manager_v32_set_task_running(true);

    BaseType_t rc = xTaskCreatePinnedToCore(
        thumbnail_manager_v32_download_task,
        "thumb_dl",
        8192,
        job,
        4,
        NULL,
        0);

    if (rc != pdPASS) {
        thumbnail_manager_v32_set_task_running(false);
        heap_caps_free(job);
        return false;
    }

    return true;
}
