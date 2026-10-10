#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "custom_theme.h"
#include "ui_telemetry_charts.c"
#include "ui_telemetry.c"
static moonraker_state_t state;
static int64_t clock_us;
static unsigned back_page,back_nav,invalidations;
int64_t esp_timer_get_time(void){return clock_us;}
void moonraker_state_snapshot(moonraker_state_t *out){*out=state;}
void ui_shell_page_action(ui_shell_page_t page){back_page=page;}
void ui_shell_set_active_nav(int page){back_nav=page;}
bool custom_theme_color_override(uint32_t a,uint32_t b,uint32_t c,uint32_t *d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_metric_override(int32_t a,int32_t b,int32_t c,int32_t *d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_accent_override(uint8_t a,uint32_t *b){(void)a;(void)b;return false;}
bool custom_theme_surface_opacity(uint8_t *a){(void)a;return false;}
static uint16_t raster[1024*600];static int screen_width=1024;
static void flush(lv_display_t *d,const lv_area_t *a,uint8_t *p){uint16_t *v=(uint16_t*)p;for(int y=a->y1;y<=a->y2;y++)for(int x=a->x1;x<=a->x2;x++)raster[y*screen_width+x]=*v++;lv_display_flush_ready(d);}
static void invalidated(lv_event_t *e){(void)e;invalidations++;}
static void screenshot(unsigned theme,unsigned view){const char *folder=getenv("TELEMETRY_SCREENSHOTS");if(!folder)return;lv_refr_now(NULL);char path[512];snprintf(path,sizeof(path),"%s/telemetry-theme%u-view%u.ppm",folder,theme,view);FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n1024 600\n255\n");for(int i=0;i<1024*600;i++){uint16_t v=raster[i];unsigned char c[3]={(v>>11)*255/31,((v>>5)&63)*255/63,(v&31)*255/31};fwrite(c,1,3,f);}fclose(f);}
static void inside(lv_obj_t *child,lv_obj_t *parent){lv_area_t a,b;lv_obj_get_coords(child,&a);lv_obj_get_coords(parent,&b);if(a.x1<b.x1||a.x2>b.x2||a.y1<b.y1||a.y2>b.y2){fprintf(stderr,"outside %s %d,%d..%d,%d parent %d,%d..%d,%d\n",lv_obj_check_type(child,&lv_label_class)?lv_label_get_text(child):"object",a.x1,a.y1,a.x2,a.y2,b.x1,b.y1,b.x2,b.y2);abort();}}
static void descendants(lv_obj_t *parent){for(uint32_t i=0;i<lv_obj_get_child_count(parent);i++){lv_obj_t *c=lv_obj_get_child(parent,i);if(lv_obj_has_flag(c,LV_OBJ_FLAG_HIDDEN))continue;inside(c,parent);descendants(c);}}
static lv_point_t touch;static bool pressed;
static void read_touch(lv_indev_t *i,lv_indev_data_t *d){(void)i;d->point=touch;d->state=pressed?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED;}
static void tap(lv_indev_t *i,lv_obj_t *button){lv_obj_update_layout(lv_screen_active());lv_area_t a;lv_obj_get_coords(button,&a);touch=(lv_point_t){(a.x1+a.x2)/2,(a.y1+a.y2)/2};pressed=true;lv_indev_read(i);pressed=false;lv_indev_read(i);}
static void ready_state(void){state=(moonraker_state_t){.moonraker_ok=true,.live_data_ok=true,.nozzle_temp=205,.nozzle_target=205,.bed_temp=60,.bed_target=60,.air_temp=30,.chamber_temp=35,.humidity=45,.heater_target=40,.live_velocity=120,.live_flow=8.3,.speed_factor=100,.flow_factor=98,.part_fan_speed=70,.drybox_fan_speed=55,.hotend_count=3,.capabilities={.discovered=true,.has_heated_bed=true,.has_drybox_center_sensor=true,.has_drybox_environment_sensor=true,.has_drybox_heater=true,.has_part_fan=true,.has_drybox_fan=true}};strcpy(state.active_hotend,"extruder");strcpy(state.printer_state,"printing");for(unsigned i=0;i<3;i++){snprintf(state.hotends[i].object_name,sizeof(state.hotends[i].object_name),"extruder%u",i);state.hotends[i].temperature=205+i*10;state.hotends[i].target=205+i*10;}strcpy(state.hotends[0].object_name,"extruder");}
int main(void){lv_init();lv_display_t *d=lv_display_create(1024,600);static uint8_t pixels[1024*50*2];lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);lv_display_set_buffers(d,pixels,NULL,sizeof(pixels),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(d,flush);lv_display_add_event_cb(d,invalidated,LV_EVENT_INVALIDATE_AREA,NULL);lv_indev_t *pointer=lv_indev_create();lv_indev_set_type(pointer,LV_INDEV_TYPE_POINTER);lv_indev_set_read_cb(pointer,read_touch);
for(unsigned theme=0;theme<5;theme++)for(unsigned density=0;density<3;density++)for(unsigned large=0;large<2;large++)for(unsigned size=0;size<3;size++){
 screen_width=(int[]){1024,640,480}[size];lv_display_set_resolution(d,screen_width,size?400:600);
 ui_theme_set_active(theme);ui_theme_set_density(density);ui_theme_set_accessibility((ui_accessibility_t){.large_text=large});
 s_window=300;s_include_targets=true;telemetry_history_reset();ready_state();clock_us=1000000;
 for(unsigned i=0;i<320;i++){clock_us+=2000000;assert(telemetry_history_sample(&state,clock_us));}
 assert(telemetry_history_count()==300);ui_telemetry_show();ui_telemetry_refresh(&state,clock_us);lv_obj_update_layout(s_panel);
 assert(s_graph_host && lv_obj_get_height(s_graph_host)>40);
 assert(lv_obj_get_style_radius(s_range,0)==6 && lv_obj_get_style_shadow_width(s_range,0)==0);
 assert(lv_obj_get_style_radius(s_hotend_selector,0)==6 && lv_obj_get_style_shadow_width(s_hotend_selector,0)==0);
 lv_dropdown_open(s_range);lv_obj_update_layout(s_panel);
 assert(lv_obj_get_style_radius(lv_dropdown_get_list(s_range),0)==6);
 assert(lv_obj_get_style_shadow_width(lv_dropdown_get_list(s_range),0)==0);
 lv_dropdown_close(s_range);inside(s_panel,lv_screen_active());
 for(uint32_t i=0;i<lv_obj_get_child_count(s_panel);i++)inside(lv_obj_get_child(s_panel,i),s_panel);
 for(unsigned i=0;i<4;i++)descendants(s_charts[i].card);
 int32_t *values=lv_chart_get_series_y_array(s_charts[0].chart,s_charts[0].actual);
 assert(values[299]==2050 && strstr(lv_label_get_text(s_charts[0].value),"205.0"));
 assert(strstr(lv_label_get_text(s_status),"LIVE"));
 for(unsigned view=0;view<3;view++){
  tap(pointer,s_tabs[view]);assert(s_view==view);lv_obj_update_layout(s_panel);
  for(unsigned i=0;i<4;i++)descendants(s_charts[i].card);
  if(!size && density==1 && large)screenshot(theme,view);
 }
 tap(pointer,s_tabs[0]);assert(s_view==UI_TELEMETRY_HEAT);
 lv_dropdown_set_selected(s_hotend_selector,2);lv_obj_send_event(s_hotend_selector,LV_EVENT_VALUE_CHANGED,NULL);assert(!strcmp(s_hotend,"extruder2"));assert(lv_chart_get_series_y_array(s_charts[0].chart,s_charts[0].actual)[299]==2250);
 /* A discovery reorder and active tool change must retain the named trace. */
 moonraker_hotend_t swap=state.hotends[0];state.hotends[0]=state.hotends[2];state.hotends[2]=swap;strcpy(state.active_hotend,"extruder1");clock_us+=2000000;ui_telemetry_refresh(&state,clock_us);assert(!strcmp(s_hotend,"extruder2"));assert(lv_chart_get_series_y_array(s_charts[0].chart,s_charts[0].actual)[299]==2250);
 lv_dropdown_set_selected(s_range,0);lv_obj_send_event(s_range,LV_EVENT_VALUE_CHANGED,NULL);assert(s_window==60 && lv_chart_get_point_count(s_charts[0].chart)==60);
 tap(pointer,s_hold_button);assert(s_held);int32_t held[60];memcpy(held,lv_chart_get_series_y_array(s_charts[0].chart,s_charts[0].actual),sizeof(held));
 state.hotends[0].temperature=227;clock_us+=10000000;ui_telemetry_refresh(&state,clock_us);assert(!memcmp(held,lv_chart_get_series_y_array(s_charts[0].chart,s_charts[0].actual),sizeof(held)));assert(strstr(lv_label_get_text(s_charts[0].value),"227.0"));assert(lv_obj_has_state(s_range,LV_STATE_DISABLED));assert(!strcmp(lv_label_get_text(s_charts[0].time),"HELD"));
 tap(pointer,s_hold_button);assert(!s_held);values=lv_chart_get_series_y_array(s_charts[0].chart,s_charts[0].actual);assert(values[59]==2270 && values[58]==LV_CHART_POINT_NONE && values[54]==2250);
 state.live_data_ok=false;state.moonraker_ok=false;clock_us+=4000000;ui_telemetry_refresh(&state,clock_us);assert(strstr(lv_label_get_text(s_status),"OFFLINE"));assert(strstr(lv_label_get_text(s_charts[0].value),"--"));assert(lv_chart_get_series_y_array(s_charts[0].chart,s_charts[0].actual)[59]==LV_CHART_POINT_NONE);
 tap(pointer,s_hold_button);assert(s_held);telemetry_history_reset();clock_us+=2000000;ui_telemetry_refresh(&state,clock_us);assert(!s_held && !telemetry_history_count());for(unsigned i=0;i<60;i++)assert(lv_chart_get_series_y_array(s_charts[0].chart,s_charts[0].actual)[i]==LV_CHART_POINT_NONE);
 ready_state();state.capabilities.has_heated_bed=false;state.capabilities.has_part_fan=false;state.capabilities.has_drybox_environment_sensor=false;clock_us+=2000000;ui_telemetry_refresh(&state,clock_us);assert(!strcmp(lv_label_get_text(s_charts[1].value),"N/A"));telemetry_sample_t sample;assert(telemetry_history_get(0,&sample));assert(isnan(sample.bed_temp)&&isnan(sample.humidity)&&isnan(sample.part_fan_speed));
 lv_refr_now(d);invalidations=0;ui_telemetry_refresh(&state,clock_us);assert(invalidations==0);
 lv_obj_scroll_to_y(s_graph_host,10000,LV_ANIM_OFF);lv_obj_update_layout(s_panel);assert(lv_obj_get_scroll_bottom(s_graph_host)<=1);
 lv_obj_t *root=s_panel;ui_telemetry_show();assert(root==s_panel);ui_telemetry_hide();assert(!s_panel&&!s_charts[0].chart);
 ui_telemetry_show();lv_obj_delete(s_panel);assert(!s_panel&&!s_charts[0].chart);ui_telemetry_show();lv_obj_t *header=lv_obj_get_child(s_panel,0);tap(pointer,lv_obj_get_child(header,1));assert(back_page==UI_SHELL_PAGE_DEVICES && back_nav==UI_SHELL_PAGE_TOOLS);ui_telemetry_hide();
}
/* Physics/data checks independent of theme geometry. */
screen_width=1024;lv_display_set_resolution(d,1024,600);ready_state();telemetry_history_reset();s_window=300;s_include_targets=true;
state.hotends[0].temperature=25;state.hotends[0].target=250;
assert(telemetry_history_sample(&state,0));assert(!telemetry_history_sample(&state,1000000));assert(telemetry_history_sample(&state,2000000));
clock_us=2000000;ui_telemetry_show();ui_telemetry_refresh(&state,clock_us);lv_obj_update_layout(s_panel);
assert(s_charts[0].high>=250 && s_charts[0].low<25);assert(!lv_obj_has_flag(s_charts[0].marker,LV_OBJ_FLAG_HIDDEN));
tap(pointer,s_scale_button);assert(!s_include_targets && s_charts[0].high<30);
state.hotends[0].temperature=500;clock_us+=2000000;ui_telemetry_refresh(&state,clock_us);assert(lv_chart_get_series_y_array(s_charts[0].chart,s_charts[0].actual)[299]==5000);
state.humidity=101;state.part_fan_speed=-1;state.air_temp=-10;clock_us+=2000000;ui_telemetry_refresh(&state,clock_us);
telemetry_sample_t sample;assert(telemetry_history_get(telemetry_history_count()-1,&sample));assert(isnan(sample.humidity)&&isnan(sample.part_fan_speed)&&sample.air_temp==-10);
lv_dropdown_set_selected(s_range,1);lv_obj_send_event(s_range,LV_EVENT_VALUE_CHANGED,NULL);assert(s_window==150);
/* An outlier older than two minutes drops from visible extrema. */
state.hotends[0].temperature=205;for(unsigned i=0;i<65;i++){clock_us+=2000000;ui_telemetry_refresh(&state,clock_us);}
lv_dropdown_set_selected(s_range,0);lv_obj_send_event(s_range,LV_EVENT_VALUE_CHANGED,NULL);assert(strstr(lv_label_get_text(s_charts[0].stats),"Max 205.0"));
lv_dropdown_set_selected(s_range,2);lv_obj_send_event(s_range,LV_EVENT_VALUE_CHANGED,NULL);assert(strstr(lv_label_get_text(s_charts[0].stats),"Max 500.0"));
uint32_t generation=telemetry_history_generation();assert(telemetry_history_sample(&state,100));assert(telemetry_history_generation()!=generation && telemetry_history_count()==1);
ui_telemetry_hide();
/* Real 500 ms UI polling with accumulating jitter must fill every live bin.
 * Validate the entire ring/window, not just the most recent sample. */
ready_state();telemetry_history_reset();clock_us=137000;ui_telemetry_show();
for(unsigned i=0;i<1400;i++) {
 clock_us+=500000+(i%7)*17000;
 ui_telemetry_refresh(&state,clock_us);
}
assert(telemetry_history_count()==300);
for(unsigned n=0;n<4;n++) {
 int32_t *a=lv_chart_get_series_y_array(s_charts[n].chart,s_charts[n].actual);
 for(unsigned i=0;i<300;i++)assert(a[i]!=LV_CHART_POINT_NONE);
}
telemetry_sample_t previous,current;
for(size_t i=1;i<300;i++) {
 assert(telemetry_history_get(i-1,&previous)&&telemetry_history_get(i,&current));
 assert(current.time_us/2000000==previous.time_us/2000000+1);
}
/* Delays/offline/invalid sensors still produce honest gaps. */
state.live_data_ok=false;clock_us+=8000000;ui_telemetry_refresh(&state,clock_us);
state.live_data_ok=true;clock_us+=2000000;ui_telemetry_refresh(&state,clock_us);
int32_t *a=lv_chart_get_series_y_array(s_charts[0].chart,s_charts[0].actual);
assert(a[299]==2050 && a[298]==LV_CHART_POINT_NONE && a[297]==LV_CHART_POINT_NONE);
state.hotends[0].temperature=NAN;clock_us+=2000000;ui_telemetry_refresh(&state,clock_us);
assert(a[299]==LV_CHART_POINT_NONE);
ui_telemetry_hide();
lv_display_delete(d);lv_deinit();puts("PASS: modern telemetry native bounds/scroll, three views, range/hold, real touches, named hotend identity, time gaps, missing capabilities, offline/reset, no-op refresh and owner lifecycle across all five themes/densities/text sizes/viewports");}
