#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
static long long now_us;
static unsigned clock_calls;
static unsigned buffer_logs, window_logs;
static char last_log[1024];
long long esp_timer_get_time(void) { ++clock_calls; return now_us; }
void audit_log(const char *tag, const char *fmt, ...)
{
    assert(!strcmp(tag, "graphics_diag"));
    va_list args; va_start(args, fmt);
    vsnprintf(last_log, sizeof(last_log), fmt, args); va_end(args);
    if (!strncmp(last_log, "buffer=", 7)) ++buffer_logs;
    if (!strncmp(last_log, "cycles=", 7)) ++window_logs;
}
#include "ui_graphics_diagnostics.c"
static unsigned char buffer1[8192] __attribute__((aligned(64)));
static unsigned char buffer2[8192] __attribute__((aligned(64)));
static void send(lv_display_t *d, lv_event_code_t code, long long time)
{
    now_us = time; lv_display_send_event(d, code, NULL);
}
int main(void)
{
    lv_init();
    lv_display_t *d = lv_display_create(64, 64);
    lv_display_set_color_format(d, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(d, buffer1, buffer2, sizeof(buffer1), LV_DISPLAY_RENDER_MODE_DIRECT);
    assert(!ui_graphics_diagnostics_start(NULL));
    assert(ui_graphics_diagnostics_start(d));
    assert(buffer_logs == 1 && strstr(last_log, "addr_mod64=0 size_mod64=0"));
    uint32_t events = lv_display_get_event_count(d);
    lv_timer_t *timer = s_diag.timer;
    assert((timer != NULL) == GRAPHICS_PERIODIC_REPORTS);
    assert(ui_graphics_diagnostics_start(d));
    assert(lv_display_get_event_count(d) == events && s_diag.timer == timer);
    lv_display_t *other = lv_display_create(64,64);
    assert(!ui_graphics_diagnostics_start(other)); lv_display_delete(other);
#if GRAPHICS_PERIODIC_REPORTS
    send(d, LV_EVENT_REFR_START, 0); send(d, LV_EVENT_REFR_READY, 10);
    report_cb(timer); assert(!window_logs && !s_diag.cycles);
    send(d, LV_EVENT_RENDER_READY, 20); assert(!s_diag.cycles);
    send(d, LV_EVENT_RENDER_START, 0);
    send(d, LV_EVENT_FLUSH_START, 50); send(d, LV_EVENT_FLUSH_FINISH, 110);
    send(d, LV_EVENT_RENDER_READY, 200);
    assert(s_diag.cycles == 1 && s_diag.render_us == 200 && s_diag.flush_us == 60);
    send(d, LV_EVENT_FLUSH_FINISH, 500); assert(s_diag.flush_us == 60);
    send(d, LV_EVENT_RENDER_START, 1000); send(d, LV_EVENT_RENDER_READY, 1300);
    assert(s_diag.cycles == 2 && s_diag.render_us == 500 && s_diag.render_max_us == 300);
    report_cb(timer);
    assert(window_logs == 1 && strstr(last_log, "cycles=2 cycle_avg_us=250 cycle_max_us=300 flush_callback_us=60"));
    assert(!s_diag.cycles && !s_diag.render_us && !s_diag.flush_us && !s_diag.render_max_us);
    report_cb(timer); assert(window_logs == 1);
#else
    for(unsigned i=0;i<20;++i){
        send(d, LV_EVENT_REFR_START, i*1000);
        send(d, LV_EVENT_RENDER_START, i*1000);
        send(d, LV_EVENT_FLUSH_START, i*1000+100);
        send(d, LV_EVENT_FLUSH_FINISH, i*1000+200);
        send(d, LV_EVENT_RENDER_READY, i*1000+300);
        send(d, LV_EVENT_REFR_READY, i*1000+400);
    }
    assert(!s_diag.cycles && !s_diag.render_us && !s_diag.flush_us && !window_logs && !clock_calls);
#endif
    /* Public buffer replacement models the other active buffer without private display access. */
    lv_display_set_buffers(d, buffer2, buffer1, sizeof(buffer2), LV_DISPLAY_RENDER_MODE_DIRECT);
    observe_buffer(); observe_buffer(); assert(buffer_logs == 2);
    /* Odd size is reported, never changed or passed to a cache operation. */
    lv_display_set_buffers(d, buffer1 + 4, NULL, 8188, LV_DISPLAY_RENDER_MODE_PARTIAL);
    observe_buffer(); assert(buffer_logs == 3 && strstr(last_log, "addr_mod64=4 size_mod64=60"));
    lv_display_delete(d); assert(!s_diag.display && !s_diag.timer && !s_diag.seen_count);
    d = lv_display_create(64,64);
    assert(ui_graphics_diagnostics_start(d));
    lv_display_delete(d); assert(!s_diag.display && !s_diag.timer);
    lv_deinit();
    puts(GRAPHICS_PERIODIC_REPORTS ?
         "PASS: opt-in actual LVGL timing, idle exclusion, exact accounting, buffer dedup and display/timer cleanup" :
         "PASS: default observer has no timing timer, clock reads or periodic reports; startup buffers, dedup, attach and deletion remain correct");
    return 0;
}
