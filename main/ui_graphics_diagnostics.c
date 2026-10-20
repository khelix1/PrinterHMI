#include "ui_graphics_diagnostics.h"
#include <inttypes.h>
#include <stdint.h>
#include <string.h>
#include "sdkconfig.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"

#if defined(CONFIG_PRINTERHMI_GRAPHICS_DIAGNOSTICS) && CONFIG_PRINTERHMI_GRAPHICS_DIAGNOSTICS
#define GRAPHICS_PERIODIC_REPORTS 1
#else
#define GRAPHICS_PERIODIC_REPORTS 0
#endif

static const char *TAG = "graphics_diag";
static struct {
    lv_display_t *display;
    lv_timer_t *timer;
    const void *seen_buffers[3];
    unsigned seen_count;
    bool rendering;
    bool flushing;
    int64_t render_start;
    int64_t flush_start;
    uint64_t cycles;
    uint64_t render_us;
    uint64_t render_max_us;
    uint64_t flush_us;
} s_diag;

static void observe_buffer(void)
{
    const lv_draw_buf_t *buf = lv_display_get_buf_active(s_diag.display);
    if (!buf || !buf->data) return;
    for (unsigned i = 0; i < s_diag.seen_count; ++i) {
        if (s_diag.seen_buffers[i] == buf->data) return;
    }
    /* Bound logging even if a future display driver rotates more buffers. */
    if (s_diag.seen_count == 3) return;
    s_diag.seen_buffers[s_diag.seen_count++] = buf->data;
    ESP_LOGI(TAG, "buffer=%u addr=%p bytes=%" PRIu32
             " width=%u height=%u stride=%" PRIu32
             " cf=%u addr_mod64=%u size_mod64=%u",
             s_diag.seen_count, (void *)buf->data, buf->data_size,
             (unsigned)buf->header.w, (unsigned)buf->header.h,
             buf->header.stride, (unsigned)buf->header.cf,
             (unsigned)((uintptr_t)buf->data % 64),
             (unsigned)(buf->data_size % 64));
}

#if GRAPHICS_PERIODIC_REPORTS
static void report_cb(lv_timer_t *timer)
{
    (void)timer;
    if (!s_diag.cycles) return; /* No idle/no-damage refreshes counted. */
    ESP_LOGI(TAG, "cycles=%" PRIu64 " cycle_avg_us=%" PRIu64
             " cycle_max_us=%" PRIu64 " flush_callback_us=%" PRIu64
             " internal_free=%zu internal_largest=%zu spiram_free=%zu",
             s_diag.cycles, s_diag.render_us / s_diag.cycles,
             s_diag.render_max_us, s_diag.flush_us,
             heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    s_diag.cycles = 0;
    s_diag.render_us = 0;
    s_diag.render_max_us = 0;
    s_diag.flush_us = 0;
}

#endif

static void display_event_cb(lv_event_t *event)
{
    switch (lv_event_get_code(event)) {
    case LV_EVENT_RENDER_START:
        observe_buffer();
#if GRAPHICS_PERIODIC_REPORTS
        s_diag.render_start = esp_timer_get_time();
        s_diag.rendering = true;
#endif
        break;
    case LV_EVENT_FLUSH_START:
        observe_buffer();
#if GRAPHICS_PERIODIC_REPORTS
        s_diag.flush_start = esp_timer_get_time();
        s_diag.flushing = true;
#endif
        break;
#if GRAPHICS_PERIODIC_REPORTS
    case LV_EVENT_FLUSH_FINISH:
        if (s_diag.flushing) {
            int64_t elapsed = esp_timer_get_time() - s_diag.flush_start;
            if (elapsed >= 0) s_diag.flush_us += (uint64_t)elapsed;
            s_diag.flushing = false;
        }
        break;
    case LV_EVENT_RENDER_READY:
        if (s_diag.rendering) {
            int64_t elapsed = esp_timer_get_time() - s_diag.render_start;
            if (elapsed >= 0) {
                ++s_diag.cycles;
                s_diag.render_us += (uint64_t)elapsed;
                if ((uint64_t)elapsed > s_diag.render_max_us)
                    s_diag.render_max_us = (uint64_t)elapsed;
            }
            s_diag.rendering = false;
        }
        break;
#endif
    case LV_EVENT_DELETE:
        if (s_diag.timer) lv_timer_delete(s_diag.timer);
        memset(&s_diag, 0, sizeof(s_diag));
        break;
    default:
        break;
    }
}

bool ui_graphics_diagnostics_start(lv_display_t *display)
{
    if (!display) return false;
    if (s_diag.display) return s_diag.display == display;
    lv_timer_t *timer = NULL;
#if GRAPHICS_PERIODIC_REPORTS
    timer = lv_timer_create(report_cb, 10000, NULL);
    if (!timer) return false;
#endif
    uint32_t event_count = lv_display_get_event_count(display);
    lv_display_add_event_cb(display, display_event_cb, LV_EVENT_ALL, NULL);
    if (lv_display_get_event_count(display) != event_count + 1) {
        if (timer) lv_timer_delete(timer);
        return false;
    }
    s_diag.display = display;
    s_diag.timer = timer;
    const char *mode = "partial";
    if (lv_display_get_render_mode(display) == LV_DISPLAY_RENDER_MODE_DIRECT) mode = "direct";
    else if (lv_display_get_render_mode(display) == LV_DISPLAY_RENDER_MODE_FULL) mode = "full";
    ESP_LOGI(TAG, "renderer mode=%s resolution=%ldx%ld cf=%u draw_align=%u l2_line=%u ppa=%u reports=%u",
             mode, (long)lv_display_get_horizontal_resolution(display),
             (long)lv_display_get_vertical_resolution(display),
             (unsigned)lv_display_get_color_format(display),
             (unsigned)CONFIG_LV_DRAW_BUF_ALIGN, (unsigned)CONFIG_CACHE_L2_CACHE_LINE_SIZE,
             (unsigned)LV_USE_PPA, (unsigned)GRAPHICS_PERIODIC_REPORTS);
    observe_buffer();
    return true;
}
