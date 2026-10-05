/* Real LVGL refresh suppression with application timers still running. */
#include <assert.h>
#include <stdio.h>
#include "custom_theme.h"
#include "ui_splash.h"
#include "ui_logo_assets.h"
#include "esp_app_desc.h"
bool custom_theme_color_override(uint32_t a,uint32_t b,uint32_t c,uint32_t *d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_metric_override(int32_t a,int32_t b,int32_t c,int32_t *d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_accent_override(uint8_t a,uint32_t *b){(void)a;(void)b;return false;}
bool custom_theme_surface_opacity(uint8_t *a){(void)a;return false;}
const esp_app_desc_t *esp_app_get_description(void){static const esp_app_desc_t app={.version="6.5.6"};return &app;}
const lv_image_dsc_t *ui_logo_assets_splash(void){
 static const uint16_t pixels[1]={0xffff};
 static const lv_image_dsc_t image={.header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_RGB565,.w=1,.h=1,.stride=2},.data_size=2,.data=(const uint8_t*)pixels};
 return &image;
}
static unsigned flushes,application_ticks;
static bool progress_update;
static void flush(lv_display_t *d,const lv_area_t *a,uint8_t *p){
 (void)p;
 if(progress_update){assert(a->x1>=150 && a->x2<880);assert(a->y1>=250 && a->y2<450);}
 flushes++;lv_display_flush_ready(d);
}
static void background(lv_timer_t *t){application_ticks++;lv_label_set_text(lv_timer_get_user_data(t),application_ticks%2?"Startup changed":"Startup changed again");}
int main(void){
 lv_init();lv_display_t *d=lv_display_create(1024,600);lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);
 static uint8_t buffer[1024*40*2];lv_display_set_buffers(d,buffer,NULL,sizeof(buffer),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(d,flush);
 unsigned original_events=lv_display_get_event_count(d);
 lv_obj_t *label=lv_label_create(lv_screen_active());lv_timer_t *app=lv_timer_create(background,20,label);
 for(int pass=0;pass<6;pass++){
  ui_theme_set_active((ui_theme_id_t)(pass/2));
  ui_theme_set_accessibility((ui_accessibility_t){.large_text=pass%2});
  ui_splash_create();ui_splash_display_ready();ui_splash_present_and_freeze();assert(flushes>0);
  unsigned presented=flushes,previous_ticks=application_ticks;
  assert(!lv_display_is_invalidation_enabled(d));assert(lv_timer_get_paused(lv_display_get_refr_timer(d)));
  ui_splash_present_and_freeze();assert(flushes==presented);
  progress_update=true;
  ui_splash_wifi_starting();assert(flushes>presented);presented=flushes;
  ui_splash_wifi_waiting(false);assert(flushes>presented);presented=flushes;
  ui_splash_wifi_waiting(true);assert(flushes>presented);presented=flushes;
  ui_splash_moonraker_ready();assert(flushes>presented);presented=flushes;
  ui_splash_dashboard_ready();assert(flushes>presented);presented=flushes;
  ui_splash_dashboard_ready();assert(flushes==presented);
  lv_obj_t *panel=lv_obj_get_child(lv_layer_top(),0);panel=lv_obj_get_child(panel,0);
  bool reached_100=false;for(uint32_t i=0;i<lv_obj_get_child_count(panel);i++){lv_obj_t *o=lv_obj_get_child(panel,i);if(lv_obj_check_type(o,&lv_bar_class)){assert(lv_bar_get_value(o)==100);reached_100=true;}}
  assert(reached_100);progress_update=false;
  for(int i=0;i<50;i++){lv_tick_inc(20);lv_timer_handler();}
  assert(application_ticks>previous_ticks);assert(flushes==presented);assert(lv_timer_get_paused(lv_display_get_refr_timer(d)));
  ui_splash_destroy();assert(lv_display_is_invalidation_enabled(d));assert(flushes>presented);assert(lv_obj_get_child_count(lv_layer_top())==0);assert(lv_display_get_event_count(d)==original_events);
  presented=flushes;lv_label_set_text(label,"Normal refresh resumed");lv_tick_inc(50);lv_timer_handler();assert(flushes>presented);
  ui_splash_destroy();assert(lv_display_is_invalidation_enabled(d));
 }
 ui_splash_create();ui_splash_destroy();assert(lv_display_is_invalidation_enabled(d));
 ui_splash_create();lv_display_enable_invalidation(d,false);ui_splash_present_and_freeze();ui_splash_destroy();assert(!lv_display_is_invalidation_enabled(d));lv_display_enable_invalidation(d,true);assert(lv_display_is_invalidation_enabled(d));
 lv_timer_delete(app);lv_deinit();puts("PASS: splash progress redraws stay bounded, background redraws held, application timers run and handoff repaints");
}
