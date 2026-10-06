#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ui_printer_chooser.c"

bool custom_theme_color_override(uint32_t a,uint32_t b,uint32_t c,uint32_t *d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_metric_override(int32_t a,int32_t b,int32_t c,int32_t *d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_accent_override(uint8_t a,uint32_t *b){(void)a;(void)b;return false;}
bool custom_theme_surface_opacity(uint8_t *a){(void)a;return false;}
static moonraker_profile_t profiles[4];
static moonraker_state_t state;
static bool known[4],online[4],fresh[4];
static char health[4][32];
static int active;
static bool preview_ready;
static uint32_t revision;
static uint16_t pixels[16];
static lv_image_dsc_t image={.header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_RGB565,.w=4,.h=4,.stride=8},.data_size=32,.data=(const uint8_t*)pixels};
const moonraker_profile_t *moonraker_config_profile(int i){return &profiles[i];}
int moonraker_config_active_profile_index(void){return active;}
void moonraker_state_snapshot(moonraker_state_t *out){*out=state;}
bool printer_profile_health_get(int i,bool *out){*out=known[i];return online[i];}
bool printer_profile_health_get_live_state(int i,char *out,size_t n){snprintf(out,n,"%.31s",health[i]);return health[i][0]!=0;}
bool printer_profile_health_live_state_fresh(int i,int64_t age){assert(age==5000000LL);return fresh[i];}
const lv_image_dsc_t *printer_preview_cache_image(int i,const char **file,uint32_t *rev){
 if(i!=0 || !preview_ready)return NULL;
 *file="print.gcode";*rev=revision;return &image;
}
static unsigned flushes,invalidations;
static void flush(lv_display_t *d,const lv_area_t *a,uint8_t *p){(void)a;(void)p;flushes++;lv_display_flush_ready(d);}
static void invalidated(lv_event_t *e){(void)e;invalidations++;}
static void settle(lv_display_t *d){lv_refr_now(d);invalidations=0;}
int main(void){
 lv_init();lv_display_t *d=lv_display_create(1024,600);
 static uint8_t buffers[2][1024*600*2];
 lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);
 lv_display_set_buffers(d,buffers[0],buffers[1],sizeof(buffers[0]),LV_DISPLAY_RENDER_MODE_DIRECT);
 lv_display_set_flush_cb(d,flush);lv_display_add_event_cb(d,invalidated,LV_EVENT_INVALIDATE_AREA,NULL);
 profiles[0]=(moonraker_profile_t){.configured=true,.name="Printer A",.host="printer-a.local",.port=7125};
 profiles[1]=(moonraker_profile_t){.configured=true,.name="Printer B",.host="printer-b.local",.port=7126};
 known[1]=online[1]=fresh[1]=true;snprintf(health[1],32,"printing");
 for(int theme=0;theme<3;theme++)for(int large=0;large<2;large++){
  ui_theme_set_active((ui_theme_id_t)theme);ui_theme_set_accessibility((ui_accessibility_t){.large_text=large});
  state=(moonraker_state_t){0};active=0;preview_ready=false;revision=1;
  unsigned events=lv_display_get_event_count(d);
  ui_printer_chooser_show(NULL,NULL);assert(s_root && s_timer);
  assert(!strcmp(lv_label_get_text(s_cards[1].status),"printing"));
  assert(!strcmp(lv_label_get_text(s_cards[2].status),"NOT CONFIGURED"));
  settle(d);unsigned before=flushes;
  for(int i=0;i<6;i++){lv_tick_inc(500);lv_timer_handler();}
  assert(invalidations==0 && flushes==before);
  state.moonraker_ok=true;snprintf(state.printer_state,sizeof(state.printer_state),"paused");
  ui_printer_chooser_refresh();assert(invalidations>0);
  assert(!strcmp(lv_label_get_text(s_cards[0].status),"paused"));settle(d);
  ui_printer_chooser_refresh();assert(!invalidations);
  preview_ready=true;ui_printer_chooser_refresh();
  assert(s_cards[0].preview_image && !lv_obj_has_flag(s_cards[0].preview_image,LV_OBJ_FLAG_HIDDEN));
  settle(d);before=flushes;
  for(int i=0;i<6;i++){lv_tick_inc(500);lv_timer_handler();}
  assert(!invalidations && flushes==before);
  revision++;ui_printer_chooser_refresh();assert(invalidations>0);settle(d);
  preview_ready=false;ui_printer_chooser_refresh();assert(lv_obj_has_flag(s_cards[0].preview_image,LV_OBJ_FLAG_HIDDEN));settle(d);
  ui_printer_chooser_refresh();assert(!invalidations);
  active=1;ui_printer_chooser_refresh();assert(lv_obj_has_flag(s_cards[0].active,LV_OBJ_FLAG_HIDDEN));
  assert(!lv_obj_has_flag(s_cards[1].active,LV_OBJ_FLAG_HIDDEN));
  ui_printer_chooser_hide();assert(!s_root && !s_timer && !s_cards[0].root);
  assert(lv_display_get_event_count(d)==events);
 }
 lv_display_delete(d);lv_deinit();
 puts("PASS: 500ms chooser timers stay redraw-free when unchanged, all themes/text sizes, active/inactive state lifetime, live status/preview/revision/profile updates and teardown");
}
