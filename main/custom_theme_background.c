#include "custom_theme_background.h"
#include "custom_theme.h"
#include "theme_background_asset.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "misc/cache/instance/lv_image_cache.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Pixels are shared by the screen and cropped page descriptors. Each object
 * owns a reference, so rescan/theme changes cannot invalidate live styles. */
typedef struct background_cache {
    struct background_cache *next;
    char path[192];
    uint8_t *pixels;
    unsigned references;
} background_cache_t;
typedef struct {
    background_cache_t *cache;
    lv_image_dsc_t image;
} background_view_t;
static background_cache_t *s_cache;

static void release_cache(background_cache_t *cache)
{
    if (!cache || --cache->references) return;
    background_cache_t **item = &s_cache;
    while (*item && *item != cache) item = &(*item)->next;
    if (*item) *item = cache->next;
    heap_caps_free(cache->pixels);
    free(cache);
}
static void view_deleted(lv_event_t *event)
{
    if (lv_event_get_target_obj(event) != lv_event_get_current_target_obj(event)) return;
    background_view_t *view = lv_event_get_user_data(event);
    lv_image_cache_drop(&view->image);
    release_cache(view->cache);
    free(view);
}
static void detach(lv_obj_t *object)
{
    for (uint32_t i = 0; i < lv_obj_get_event_count(object); ++i) {
        lv_event_dsc_t *event = lv_obj_get_event_dsc(object, i);
        if (lv_event_dsc_get_cb(event) != view_deleted) continue;
        background_view_t *view = lv_event_dsc_get_user_data(event);
        lv_obj_set_style_bg_image_src(object, NULL, 0);
        lv_obj_remove_event_dsc(object, event);
        lv_image_cache_drop(&view->image);
        release_cache(view->cache);
        free(view);
        return;
    }
}
static background_cache_t *acquire(const char *path)
{
    unsigned count = 0;
    for (background_cache_t *item = s_cache; item; item = item->next) {
        if (!strcmp(item->path, path)) { ++item->references; return item; }
        ++count;
    }
    /* Keep a bounded allowance for a previous theme still owned by a page. */
    if (count >= 2 || !theme_background_asset_header_valid(path)) return NULL;
    background_cache_t *cache = calloc(1, sizeof(*cache));
    if (!cache) return NULL;
    cache->pixels = heap_caps_malloc(THEME_BACKGROUND_BYTES + THEME_BACKGROUND_WIDTH * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!cache->pixels || !theme_background_asset_read(path, cache->pixels, THEME_BACKGROUND_BYTES)) {
        heap_caps_free(cache->pixels); free(cache); return NULL;
    }
    /* Cropped descriptors keep the full-screen stride. Reserve a padded row
     * so the final scanline has the size LVGL requires even at x > 0. */
    memset(cache->pixels + THEME_BACKGROUND_BYTES, 0, THEME_BACKGROUND_WIDTH * 2);
    snprintf(cache->path, sizeof(cache->path), "%s", path);
    cache->references = 1; cache->next = s_cache; s_cache = cache;
    return cache;
}
void custom_theme_background_apply(lv_obj_t *object)
{
    if (!object) return;
    detach(object);
    const char *active_path = custom_theme_background_path();
    if (!active_path || ui_theme_get_accessibility().high_contrast) return;
    char path[192];
    snprintf(path, sizeof(path), "%s", active_path);
    lv_obj_t *screen = lv_screen_active();
    int32_t x = 0, y = 0, width = THEME_BACKGROUND_WIDTH, height = THEME_BACKGROUND_HEIGHT;
    if (object != screen) {
        if (lv_obj_get_parent(object) != screen) return;
        x = lv_obj_get_style_x(object, 0); y = lv_obj_get_style_y(object, 0);
        width = lv_obj_get_style_width(object, 0); height = lv_obj_get_style_height(object, 0);
        /* Only the known page frames receive artwork; dialogs remain opaque. */
        if (!((width == 854 && height == 528) || (width == 976 && height == 424))) return;
    }
    if (x < 0 || y < 0 || width <= 0 || height <= 0 || x + width > THEME_BACKGROUND_WIDTH || y + height > THEME_BACKGROUND_HEIGHT) return;
    background_cache_t *cache = acquire(path);
    if (!cache) { ESP_LOGW("theme_background", "Artwork unavailable; using solid theme background"); return; }
    background_view_t *view = calloc(1, sizeof(*view));
    if (!view) { release_cache(cache); return; }
    view->cache = cache;
    view->image.header.magic = LV_IMAGE_HEADER_MAGIC;
    view->image.header.cf = LV_COLOR_FORMAT_RGB565;
    view->image.header.w = width; view->image.header.h = height;
    view->image.header.stride = THEME_BACKGROUND_WIDTH * 2;
    view->image.data = cache->pixels + (y * THEME_BACKGROUND_WIDTH + x) * 2;
    view->image.data_size = height * THEME_BACKGROUND_WIDTH * 2;
    if (!lv_obj_add_event_cb(object, view_deleted, LV_EVENT_DELETE, view)) { release_cache(cache); free(view); return; }
    lv_obj_set_style_bg_image_src(object, &view->image, 0);
    lv_obj_set_style_bg_image_opa(object, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_image_recolor_opa(object, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_image_tiled(object, false, 0);
}
