#include "ui_preview_lightbox.h"
#include "misc/cache/instance/lv_image_cache.h"

#include <stdio.h>
#include <string.h>

#include "bsp/esp-bsp.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "moonraker.h"
#include "moonraker_config_controller.h"
#include "thumbnail_manager.h"
#include "thumbnail_render.h"
#include "ui_theme.h"
#include "ui_widgets.h"

#define FULLSCREEN_WIDTH 900
#define FULLSCREEN_HEIGHT 520
#define FULLSCREEN_METADATA_SIZE 8192

static lv_obj_t *s_preview_lightbox = NULL;
static lv_obj_t *s_preview_image;
static lv_obj_t *s_preview_hint;
static uint16_t *s_fullscreen_pixels;
static uint16_t *s_fallback_pixels;
static lv_image_dsc_t s_fallback_image;
static lv_image_dsc_t s_fullscreen_image;
static uint32_t s_request_id;
static QueueHandle_t s_preview_queue;
static bool s_preview_worker_started;
static bool s_preview_retiring;

typedef struct {
    char file[160];
    char host[MOONRAKER_CONFIG_HOST_LENGTH];
    char api_key[MOONRAKER_CONFIG_API_KEY_LENGTH];
    int port;
    uint32_t request_id;
    uint32_t profile_generation;
} fullscreen_job_t;

/* Retire the old owner before LVGL deletion can open a replacement. */
static void preview_lightbox_retire(void)
{
    s_preview_retiring = true;
    lv_obj_t *image = s_preview_image;
    uint16_t *fallback = s_fallback_pixels, *fullscreen = s_fullscreen_pixels;
    s_preview_lightbox = s_preview_image = s_preview_hint = NULL;
    s_fallback_pixels = s_fullscreen_pixels = NULL;
    ++s_request_id;
    if (image) lv_image_set_src(image, NULL);
    lv_image_cache_drop(&s_fallback_image);
    lv_image_cache_drop(&s_fullscreen_image);
    memset(&s_fallback_image, 0, sizeof(s_fallback_image));
    memset(&s_fullscreen_image, 0, sizeof(s_fullscreen_image));
    heap_caps_free(fallback);
    heap_caps_free(fullscreen);
    s_preview_retiring = false;
}

static void preview_lightbox_delete_cb(lv_event_t *event)
{
    if (lv_event_get_target_obj(event) != s_preview_lightbox ||
        lv_event_get_current_target_obj(event) != s_preview_lightbox) return;
    preview_lightbox_retire();
}

static void preview_field_delete_cb(lv_event_t *event)
{
    lv_obj_t *target = lv_event_get_target_obj(event);
    if (target == s_preview_image) {s_preview_image = NULL; ++s_request_id;}
    if (target == s_preview_hint) s_preview_hint = NULL;
}

static void preview_lightbox_close_cb(lv_event_t *event)
{
    lv_obj_t *target = lv_event_get_target_obj(event);
    if (s_preview_lightbox && lv_event_get_code(event) == LV_EVENT_CLICKED &&
        target == lv_event_get_current_target_obj(event) &&
        (target == s_preview_lightbox || target == s_preview_image || target == s_preview_hint) &&
        !lv_obj_has_state(target, LV_STATE_DISABLED)) ui_preview_lightbox_close();
}

bool ui_preview_lightbox_is_open(void)
{
    return s_preview_lightbox != NULL;
}

void ui_preview_lightbox_close(void)
{
    if (!s_preview_lightbox || s_preview_retiring) {
        return;
    }

    lv_obj_t *lightbox = s_preview_lightbox;
    preview_lightbox_retire();
    lv_obj_delete(lightbox);
}

static bool preview_lightbox_show_image(const lv_image_dsc_t *image)
{
    if (s_preview_retiring || !image || lv_image_src_get_type(image) != LV_IMAGE_SRC_VARIABLE) return false;
    lv_image_header_t header = image->header;
    if ((header.w == 0 || header.h == 0) &&
        lv_image_decoder_get_info(image, &header) != LV_RESULT_OK) return false;
    if (header.w == 0 || header.h == 0) return false;

    /* Snapshot before closing: image may be this overlay's owned descriptor. */
    uint32_t request = s_request_id;
    lv_obj_t *owner = s_preview_lightbox;
    int width = (int)header.w, height = (int)header.h;
    if ((size_t)width * height > THUMBNAIL_PREVIEW_WIDTH * THUMBNAIL_PREVIEW_HEIGHT) {
        width = THUMBNAIL_PREVIEW_WIDTH;
        height = (int)((int64_t)header.h * width / header.w);
        if (height > THUMBNAIL_PREVIEW_HEIGHT) {
            height = THUMBNAIL_PREVIEW_HEIGHT;
            width = (int)((int64_t)header.w * height / header.h);
        }
    }
    if (width < 1) width = 1;
    if (height < 1) height = 1;
    uint16_t *pixels = heap_caps_malloc((size_t)width * height * sizeof(uint16_t),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (pixels && !thumbnail_render_to_rgb565_fit(image, pixels, width, height)) {
        heap_caps_free(pixels); pixels = NULL;
    }
    if (owner != s_preview_lightbox || request != s_request_id) {
        heap_caps_free(pixels); return false;
    }
    ui_preview_lightbox_close();
    if (s_preview_lightbox || s_request_id != request + (owner ? 1U : 0U)) {
        heap_caps_free(pixels); return false;
    }

    /* File detail popups live on the top layer. The lightbox must be a
     * foreground sibling so it covers the popup and its touch blocker.
     */
    lv_obj_t *layer = lv_layer_top();
    if (!layer) {heap_caps_free(pixels); return false;}

    s_preview_lightbox = lv_obj_create(layer);
    if (!s_preview_lightbox) {
        heap_caps_free(pixels); return false;
    }
    ++s_request_id;

    lv_obj_set_size(s_preview_lightbox, 1024, 600);
    lv_obj_set_pos(s_preview_lightbox, 0, 0);
    lv_obj_clear_flag(s_preview_lightbox, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_preview_lightbox, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_set_style_bg_color(
        s_preview_lightbox,
        lv_color_black(),
        0);
    lv_obj_set_style_bg_opa(
        s_preview_lightbox,
        LV_OPA_90,
        0);
    lv_obj_set_style_border_width(s_preview_lightbox, 0, 0);
    lv_obj_set_style_radius(s_preview_lightbox, 0, 0);
    lv_obj_set_style_pad_all(s_preview_lightbox, 0, 0);

    lv_obj_add_event_cb(
        s_preview_lightbox,
        preview_lightbox_close_cb,
        LV_EVENT_CLICKED,
        NULL);
    lv_obj_add_event_cb(
        s_preview_lightbox,
        preview_lightbox_delete_cb,
        LV_EVENT_DELETE,
        NULL);

    lv_obj_t *preview = lv_image_create(s_preview_lightbox);
    if (!preview) {
        ui_preview_lightbox_close();
        heap_caps_free(pixels); return false;
    }

    s_preview_image = preview;
    lv_obj_add_event_cb(preview, preview_field_delete_cb, LV_EVENT_DELETE, NULL);
    s_fallback_pixels = pixels;
    if (s_fallback_pixels) {
        memset(&s_fallback_image, 0, sizeof(s_fallback_image));
        s_fallback_image.header.magic = LV_IMAGE_HEADER_MAGIC;
        s_fallback_image.header.cf = LV_COLOR_FORMAT_RGB565;
        s_fallback_image.header.w = width;
        s_fallback_image.header.h = height;
        s_fallback_image.header.stride = width * sizeof(uint16_t);
        s_fallback_image.data_size = (size_t)width * height * sizeof(uint16_t);
        s_fallback_image.data = (const uint8_t *)s_fallback_pixels;
        lv_image_set_src(preview, &s_fallback_image);
    }

    lv_obj_set_size(preview, FULLSCREEN_WIDTH, FULLSCREEN_HEIGHT);
    lv_image_set_inner_align(preview, LV_IMAGE_ALIGN_CONTAIN);
    lv_obj_center(preview);
    lv_obj_add_flag(preview, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(preview, preview_lightbox_close_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *hint =
        ui_create_card_subtitle(
            s_preview_lightbox,
            s_fallback_pixels ? "TAP ANYWHERE TO CLOSE" : "PREVIEW UNAVAILABLE - TAP TO CLOSE");

    s_preview_hint = hint;
    if (hint) {
        lv_obj_add_event_cb(hint, preview_field_delete_cb, LV_EVENT_DELETE, NULL);
        ui_apply_label_bright(hint);
        lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -12);
        lv_obj_add_flag(hint, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(hint, preview_lightbox_close_cb, LV_EVENT_CLICKED, NULL);
    }

    lv_obj_move_foreground(s_preview_lightbox);
    return true;
}

void ui_preview_lightbox_show(const lv_image_dsc_t *image)
{
    (void)preview_lightbox_show_image(image);
}

static bool preview_lightbox_show_image_object(lv_obj_t *image_object)
{
    if (!image_object || !lv_obj_has_class(image_object, &lv_image_class)) return false;
    const void *source = lv_image_get_src(image_object);
    if (!source || lv_image_src_get_type(source) != LV_IMAGE_SRC_VARIABLE) return false;
    return preview_lightbox_show_image(source);
}

void ui_preview_lightbox_show_object(lv_obj_t *image_object)
{
    (void)preview_lightbox_show_image_object(image_object);
}

static bool fullscreen_job_is_current(const fullscreen_job_t *job)
{
    return s_preview_lightbox && s_preview_image &&
           job->request_id == s_request_id &&
           job->profile_generation == moonraker_config_generation();
}

/* Transfer only into the current overlay; each LVGL update may retire it. */
static bool fullscreen_publish(const fullscreen_job_t *job, uint16_t **pixels)
{
    if (!*pixels || !fullscreen_job_is_current(job)) return false;
    lv_obj_t *image = s_preview_image;
    lv_image_set_src(image, NULL);
    if (!fullscreen_job_is_current(job)) return false;
    lv_image_cache_drop(&s_fullscreen_image);
    heap_caps_free(s_fullscreen_pixels);
    memset(&s_fullscreen_image, 0, sizeof(s_fullscreen_image));
    s_fullscreen_image.header.magic = LV_IMAGE_HEADER_MAGIC;
    s_fullscreen_image.header.cf = LV_COLOR_FORMAT_RGB565;
    s_fullscreen_image.header.w = FULLSCREEN_WIDTH;
    s_fullscreen_image.header.h = FULLSCREEN_HEIGHT;
    s_fullscreen_image.header.stride = FULLSCREEN_WIDTH * sizeof(uint16_t);
    s_fullscreen_image.data_size = FULLSCREEN_WIDTH * FULLSCREEN_HEIGHT * sizeof(uint16_t);
    s_fullscreen_image.data = (const uint8_t *)*pixels;
    s_fullscreen_pixels = *pixels;
    *pixels = NULL;
    lv_image_set_src(image, &s_fullscreen_image);
    if (!fullscreen_job_is_current(job)) return false;
    lv_obj_set_size(image, FULLSCREEN_WIDTH, FULLSCREEN_HEIGHT);
    if (!fullscreen_job_is_current(job)) return false;
    lv_image_set_inner_align(image, LV_IMAGE_ALIGN_CONTAIN);
    if (!fullscreen_job_is_current(job)) return false;
    lv_obj_center(image);
    if (!fullscreen_job_is_current(job)) return false;
    lv_image_cache_drop(&s_fallback_image);
    heap_caps_free(s_fallback_pixels);
    s_fallback_pixels = NULL;
    memset(&s_fallback_image, 0, sizeof(s_fallback_image));
    return true;
}

static void fullscreen_preview_worker(void *arg)
{
    (void)arg;
    fullscreen_job_t job;
    while (true) {
        if (xQueueReceive(s_preview_queue, &job, portMAX_DELAY) != pdTRUE) continue;
        if (!bsp_display_lock(1000)) {memset(job.api_key, 0, sizeof(job.api_key)); continue;}
        bool current = fullscreen_job_is_current(&job);
        bsp_display_unlock();
        if (!current) {memset(job.api_key, 0, sizeof(job.api_key)); continue;}

        for (unsigned attempt = 0; attempt < 3; ++attempt) {
            char *metadata = heap_caps_calloc(1, FULLSCREEN_METADATA_SIZE,
                                             MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            char encoded_file[512];
            char thumbnail_path[256] = "";
            uint8_t *png = NULL;
            size_t png_size = 0;
            int http_code = 0;
            esp_err_t error = ESP_FAIL;
            thumbnail_manager_url_encode(job.file, encoded_file, sizeof(encoded_file));
            bool fetched = metadata && moonraker_fetch_file_metadata(
                job.host, job.port, job.api_key, encoded_file,
                metadata, FULLSCREEN_METADATA_SIZE, &http_code, &error) &&
                json_find_best_thumbnail_path(metadata, thumbnail_path, sizeof(thumbnail_path));
            heap_caps_free(metadata);
            if (fetched) {
                char encoded_thumbnail[768];
                thumbnail_manager_url_encode(thumbnail_path, encoded_thumbnail, sizeof(encoded_thumbnail));
                fetched = moonraker_fetch_thumbnail_encoded(
                    job.host, job.port, encoded_thumbnail, &png, &png_size);
            }

            /* Only the open lightbox gets a full-size canvas. List/cache slots
             * retain their small sources; never fall back to internal RAM here. */
            uint16_t *pixels = fetched ? heap_caps_malloc(
                FULLSCREEN_WIDTH * FULLSCREEN_HEIGHT * sizeof(uint16_t),
                MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) : NULL;
            bool rendered = false;
            if (bsp_display_lock(2500)) {
                current = fullscreen_job_is_current(&job);
                if (current && pixels) {
                    lv_image_dsc_t raw_png;
                    memset(&raw_png, 0, sizeof(raw_png));
    #if defined(LV_IMAGE_HEADER_MAGIC)
                    raw_png.header.magic = LV_IMAGE_HEADER_MAGIC;
    #endif
                    raw_png.header.cf = LV_COLOR_FORMAT_RAW;
                    raw_png.data = png;
                    raw_png.data_size = png_size;
                    rendered = thumbnail_render_to_rgb565_fit(
                        &raw_png, pixels, FULLSCREEN_WIDTH, FULLSCREEN_HEIGHT);
                }
                current = fullscreen_job_is_current(&job);
                if (!current) rendered = false;
                if (rendered) rendered = fullscreen_publish(&job, &pixels);
                current = fullscreen_job_is_current(&job);
                if (current && s_preview_hint) {
                    lv_label_set_text(s_preview_hint, rendered ? "TAP ANYWHERE TO CLOSE" :
                        attempt + 1 < 3 ? "RETRYING HIGH-RES PREVIEW - TAP TO CLOSE" :
                        s_fallback_pixels ? "SHOWING ORIGINAL PREVIEW - TAP TO CLOSE" :
                                            "PREVIEW UNAVAILABLE - TAP TO CLOSE");
                }
                bsp_display_unlock();
            }
            heap_caps_free(pixels);
            heap_caps_free(png);
            if (rendered || !current) break;
            if (attempt + 1 < 3) {
                vTaskDelay(pdMS_TO_TICKS(500));
                if (!bsp_display_lock(1000)) break;
                current = fullscreen_job_is_current(&job);
                bsp_display_unlock();
                if (!current) break;
            }
        }
        memset(job.api_key, 0, sizeof(job.api_key));
    }
}

void ui_preview_lightbox_show_file_object(lv_obj_t *image_object, const char *file)
{
    /* The caller's filename may belong to the owner that Show retires. */
    fullscreen_job_t job = {0};
    bool valid_file = file && file[0] && strlen(file) < sizeof(job.file);
    if (valid_file) snprintf(job.file, sizeof(job.file), "%s", file);
    if (!preview_lightbox_show_image_object(image_object) || !valid_file) return;

    job.request_id = s_request_id;
    job.profile_generation = moonraker_config_generation();
    const char *host = moonraker_config_host();
    snprintf(job.host, sizeof(job.host), "%s", host ? host : "");
    if (!fullscreen_job_is_current(&job)) return;
    const char *key = moonraker_config_api_key();
    snprintf(job.api_key, sizeof(job.api_key), "%s", key ? key : "");
    job.port = moonraker_config_port();
    if (!fullscreen_job_is_current(&job) || !job.host[0] || job.port < 1 || job.port > 65535) goto finished;

    if (!s_preview_queue) s_preview_queue = xQueueCreate(1, sizeof(fullscreen_job_t));
    if (!s_preview_queue || !fullscreen_job_is_current(&job)) goto finished;
    if (!s_preview_worker_started) {
        s_preview_worker_started = xTaskCreatePinnedToCore(
            fullscreen_preview_worker, "preview_full", 8192, NULL, 3, NULL, 0) == pdPASS;
    }
    if (!s_preview_worker_started || !fullscreen_job_is_current(&job)) goto finished;
    if (s_preview_hint) lv_label_set_text(s_preview_hint, "LOADING HIGH-RES PREVIEW - TAP TO CLOSE");
    if (fullscreen_job_is_current(&job)) xQueueOverwrite(s_preview_queue, &job);
finished:
    memset(job.api_key, 0, sizeof(job.api_key));
}
