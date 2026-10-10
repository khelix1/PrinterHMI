#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ui_value_update.h"
#include "telemetry_history.h"
#include "ui_dashboard_status.c"
#include "ui_printer_info_cards.c"

bool custom_theme_color_override(uint32_t a,uint32_t b,uint32_t c,uint32_t *d) { (void)a;(void)b;(void)c;(void)d;return false; }
bool custom_theme_accent_override(uint8_t a,uint32_t *b) { (void)a;(void)b;return false; }

void *heap_caps_malloc(size_t n, uint32_t caps) { (void)caps; return malloc(n); }
void *heap_caps_calloc(size_t n, size_t size, uint32_t caps) { (void)caps; return calloc(n,size); }
void heap_caps_free(void *p) { free(p); }
static unsigned invalidations;
static void invalidated(lv_event_t *e) { (void)e; ++invalidations; }
static void flush(lv_display_t *d, const lv_area_t *a, uint8_t *p)
{ (void)a; (void)p; lv_display_flush_ready(d); }
int main(void)
{
    lv_init(); lv_display_t *d=lv_display_create(1024,600);
    static uint8_t buffer[1024*40*2];
    lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(d,buffer,NULL,sizeof(buffer),LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(d,flush);
    lv_display_add_event_cb(d,invalidated,LV_EVENT_INVALIDATE_AREA,NULL);
    lv_obj_t *label=lv_label_create(lv_screen_active());
    assert(ui_value_set_text(label,"205.0 C"));
    lv_refr_now(d); invalidations=0;
    const char *text=lv_label_get_text(label);
    for(int i=0;i<100;i++) assert(!ui_value_set_text(label,"205.0 C"));
    assert(text==lv_label_get_text(label) && invalidations==0);
    assert(ui_value_set_text(label,"205.1 C")); assert(invalidations>0);
    assert(ui_value_set_text(label,NULL)); assert(!strcmp(lv_label_get_text(label),""));
    assert(!ui_value_set_text(NULL,"value"));
    lv_obj_t *parent=lv_obj_create(lv_screen_active());
    lv_obj_t *child=lv_label_create(parent);
    lv_color_t red=lv_color_hex(0xff0000),blue=lv_color_hex(0x0000ff);
    lv_obj_set_style_text_color(parent,red,0); ui_value_set_color(child,red,0);
    lv_obj_set_style_text_color(parent,blue,0);
    assert(lv_color_eq(lv_obj_get_style_text_color(child,0),red));
    lv_refr_now(d);invalidations=0;ui_value_set_color(child,red,0);assert(!invalidations);
    ui_value_set_color(child,blue,0);assert(lv_color_eq(lv_obj_get_style_text_color(child,0),blue));

    s_dash_status=(ui_dashboard_status_t){
        .state=lv_label_create(parent),.progress=lv_label_create(parent),
        .elapsed=lv_label_create(parent),.remaining=lv_label_create(parent),
        .eta=lv_label_create(parent),.progress_bar=lv_bar_create(parent)};
    ui_dashboard_status_set_print_state("printing");
    ui_dashboard_status_set_progress("72 %",72,red);
    ui_dashboard_status_set_times("02:30","00:58","22:15");
    lv_refr_now(d);invalidations=0;
    for(int i=0;i<10;i++) {
        ui_dashboard_status_set_print_state("printing");
        ui_dashboard_status_set_progress("72 %",72,red);
        ui_dashboard_status_set_times("02:30","00:58","22:15");
    }
    assert(!invalidations && lv_bar_get_value(s_dash_status.progress_bar)==72);
    ui_dashboard_status_set_progress("73 %",73,blue);
    assert(invalidations && lv_bar_get_value(s_dash_status.progress_bar)==73);
    ui_dashboard_status_set_progress("100 %",120,blue);
    assert(lv_bar_get_value(s_dash_status.progress_bar)==100);
    ui_printer_info_cards_t cards={.progress=lv_label_create(parent),
        .nozzle=lv_label_create(parent),.bed=lv_label_create(parent),
        .part_fan=lv_label_create(parent),.elapsed=lv_label_create(parent),
        .remaining=lv_label_create(parent),.eta=lv_label_create(parent)};
    lv_obj_t *card_labels[]={cards.progress,cards.nozzle,cards.bed,cards.part_fan,cards.elapsed,cards.remaining,cards.eta};
    for(unsigned i=0;i<7;i++)lv_obj_set_width(card_labels[i],220);
    ui_printer_info_cards_refresh(parent,&cards,.72,205,205,60,60,50,9000,"00:58",true);
    lv_refr_now(d);invalidations=0;
    for(int i=0;i<10;i++)ui_printer_info_cards_refresh(parent,&cards,.72,205,205,60,60,50,9000,"00:58",true);
    assert(!invalidations);
    ui_printer_info_cards_refresh(parent,&cards,.73,205.1,205,60,60,75,9060,NULL,false);
    assert(invalidations && strstr(lv_label_get_text(cards.nozzle),"205.1"));
    assert(strstr(lv_label_get_text(cards.eta),"OFFLINE"));

    lv_obj_t *batched=lv_chart_create(parent),*reference=lv_chart_create(parent);
    lv_chart_set_point_count(batched,300);lv_chart_set_point_count(reference,300);
    lv_chart_set_update_mode(batched,LV_CHART_UPDATE_MODE_SHIFT);
    lv_chart_set_update_mode(reference,LV_CHART_UPDATE_MODE_SHIFT);
    lv_chart_series_t *bs=lv_chart_add_series(batched,red,LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_series_t *rs=lv_chart_add_series(reference,red,LV_CHART_AXIS_PRIMARY_Y);
    lv_refr_now(d);
    for(int i=0;i<625;i++) {
        int32_t value=i%17==0 ? LV_CHART_POINT_NONE : i<310 ? 2050 : i;
        invalidations=0;ui_value_chart_append(batched,bs,value);assert(!invalidations);
        lv_chart_set_next_value(reference,rs,value);
        assert(lv_chart_get_x_start_point(batched,bs)==lv_chart_get_x_start_point(reference,rs));
        assert(!memcmp(lv_chart_get_series_y_array(batched,bs),lv_chart_get_series_y_array(reference,rs),300*sizeof(int32_t)));
    }

    telemetry_history_reset();
    moonraker_state_t state={.live_data_ok=true,.nozzle_temp=205,.nozzle_target=205,.bed_temp=60,.bed_target=60,.air_temp=30,.humidity=45};
    assert(telemetry_history_sample(&state,1000000));
    for(int i=0;i<9;i++)assert(!telemetry_history_sample(&state,1100000+i*100000));
    assert(telemetry_history_sample(&state,2000000));
    assert(!telemetry_history_sample(&state,3000000));
    for(int i=0;i<310;i++)assert(telemetry_history_sample(&state,5000000LL+i*2000000LL));
    assert(telemetry_history_count()==300);
    telemetry_sample_t last;assert(telemetry_history_get(299,&last));assert(last.time_us==623000000LL && last.nozzle_temp==205);
    telemetry_history_reset();assert(!telemetry_history_count());
    lv_obj_delete(parent);
    lv_display_delete(d);lv_deinit();
    puts("PASS: Dashboard/Printer repeated refresh and live transitions, unchanged text/color produces no invalidation, inherited style safety, exact shift/wrap/gap equivalence, 2-second flat samples, bounded timestamped history and teardown");
}
