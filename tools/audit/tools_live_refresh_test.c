#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define refresh motion_refresh
#define close_cb motion_close_cb
#include "ui_motion_diagnostics.c"
#undef refresh
#undef close_cb
#define refresh endstop_refresh
#define close_cb endstop_close_cb
#include "ui_endstop_status.c"
#undef refresh
#undef close_cb
bool custom_theme_color_override(uint32_t a,uint32_t b,uint32_t c,uint32_t *d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_metric_override(int32_t a,int32_t b,int32_t c,int32_t *d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_accent_override(uint8_t a,uint32_t *b){(void)a;(void)b;return false;}
bool custom_theme_surface_opacity(uint8_t *a){(void)a;return false;}
void *heap_caps_malloc(size_t n,uint32_t caps){(void)caps;return malloc(n);}
void *heap_caps_calloc(size_t n,size_t size,uint32_t caps){(void)caps;return calloc(n,size);}
void heap_caps_free(void *p){free(p);}
static uint32_t generation=1;
static moonraker_state_t state;
static motion_diagnostics_snapshot_t motion;
static endstop_status_snapshot_t endstops;
static int64_t now;
static bool websocket=true;
static unsigned requests,resets,invalidations;
uint32_t moonraker_config_generation(void){return generation;}
void moonraker_state_snapshot(moonraker_state_t *o){*o=state;}
void motion_diagnostics_controller_snapshot(motion_diagnostics_snapshot_t *o){*o=motion;}
int64_t esp_timer_get_time(void){return now;}
bool moonraker_live_websocket_connected(void){return websocket;}
bool moonraker_live_websocket_request_endstops(uint32_t id){(void)id;requests++;return true;}
void endstop_status_controller_reset(void){resets++;memset(&endstops,0,sizeof(endstops));}
void endstop_status_controller_snapshot(endstop_status_snapshot_t *o){*o=endstops;}
uint32_t endstop_status_controller_begin(uint32_t owner){endstops.waiting=true;endstops.owner_generation=owner;endstops.updated_us=now;return 0x40000001U;}
void endstop_status_controller_failed(const char *message){endstops.waiting=false;strcpy(endstops.error,message);}
static void invalidated(lv_event_t *e){(void)e;invalidations++;}
static void flush(lv_display_t *d,const lv_area_t *a,uint8_t *p){(void)a;(void)p;lv_display_flush_ready(d);}
static void same(lv_obj_t *l,const char *t){assert(!strcmp(lv_label_get_text(l),t));}
static void quiet(lv_display_t *d,void (*cb)(lv_timer_t *)){
 lv_refr_now(d);invalidations=0;for(int i=0;i<100;i++)cb(NULL);assert(!invalidations);
}
int main(void){
 lv_init();lv_display_t *d=lv_display_create(1024,600);static uint8_t pixels[1024*50*2];
 lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);lv_display_set_buffers(d,pixels,NULL,sizeof(pixels),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(d,flush);lv_display_add_event_cb(d,invalidated,LV_EVENT_INVALIDATE_AREA,NULL);
 motion_ui_state_t storage={0};s=&storage;
 for(unsigned theme=0;theme<4;theme++)for(unsigned large=0;large<2;large++){
  ui_theme_set_active((ui_theme_id_t)theme);ui_theme_set_accessibility((ui_accessibility_t){.large_text=large});
  memset(s,0,sizeof(*s));memset(&motion,0,sizeof(motion));memset(&state,0,sizeof(state));generation++;s->owner=generation;
  s->popup=lv_obj_create(lv_screen_active());s->status=lv_label_create(s->popup);s->body=lv_label_create(s->popup);s->driver_selector=lv_dropdown_create(s->popup);
  for(unsigned i=0;i<4;i++)s->limits[i]=lv_label_create(s->popup);
  s->view=VIEW_DRIVERS;state.moonraker_ok=state.live_data_ok=true;strcpy(state.printer_state,"ready");
  motion_refresh(NULL);same(s->status,"WAITING FOR DRIVER DISCOVERY");quiet(d,motion_refresh);
  motion.discovered=true;motion_refresh(NULL);same(s->status,"NO TMC DRIVERS DETECTED");quiet(d,motion_refresh);
  motion.driver_count=2;strcpy(motion.drivers[0].name,"stepper_x");strcpy(motion.drivers[1].name,"stepper_y");motion.drivers[0].status_valid=true;motion.drivers[0].run_valid=true;motion.drivers[0].run_current=0.8;
  motion_refresh(NULL);same(s->status,"NO MONITORED FAULT FLAGS REPORTED");assert(!lv_obj_has_state(s->driver_selector,LV_STATE_DISABLED));quiet(d,motion_refresh);
  motion.drivers[0].warning=true;motion_refresh(NULL);same(s->status,"DRIVER WARNING REPORTED");assert(lv_color_eq(lv_obj_get_style_text_color(s->status,0),UI_WARN));quiet(d,motion_refresh);
  motion.drivers[0].fault=true;motion_refresh(NULL);same(s->status,"DRIVER FAULT REPORTED");assert(lv_color_eq(lv_obj_get_style_text_color(s->status,0),theme==UI_THEME_CLASSIC?UI_TEXT_ERROR:UI_DANGER_BRIGHT));quiet(d,motion_refresh);
  lv_dropdown_set_selected(s->driver_selector,1);motion_driver_snapshot_t swapped=motion.drivers[0];motion.drivers[0]=motion.drivers[1];motion.drivers[1]=swapped;motion_refresh(NULL);assert(lv_dropdown_get_selected(s->driver_selector)==0);quiet(d,motion_refresh);
  state.live_data_ok=false;motion_refresh(NULL);same(s->status,"LIVE DRIVER DATA UNAVAILABLE");assert(lv_obj_has_state(s->driver_selector,LV_STATE_DISABLED));quiet(d,motion_refresh);
  state.live_data_ok=true;s->view=VIEW_LIMITS;motion.limit_valid[0]=true;motion.limits[0]=200;motion_refresh(NULL);same(s->limits[0],"200.00 mm/s");quiet(d,motion_refresh);
  motion.limits[0]=250;motion_refresh(NULL);same(s->limits[0],"250.00 mm/s");generation++;motion_refresh(NULL);same(s->status,"PRINTER CHANGED: CLOSE AND REOPEN");quiet(d,motion_refresh);
  s->view=VIEW_DRIVERS;motion_refresh(NULL);quiet(d,motion_refresh);close_popup();assert(!s->popup && !s->status);
  s_popup=lv_obj_create(lv_screen_active());s_body=lv_label_create(s_popup);s_owner=generation;now=1000000;s_next_query=now+2000000;websocket=true;state.live_data_ok=true;strcpy(state.printer_state,"ready");
  memset(&endstops,0,sizeof(endstops));endstops.valid=true;endstops.count=1;strcpy(endstops.items[0].name,"x");endstop_refresh(NULL);assert(strstr(lv_label_get_text(s_body),"x:  OPEN"));quiet(d,endstop_refresh);
  endstops.items[0].triggered=true;endstop_refresh(NULL);assert(strstr(lv_label_get_text(s_body),"x:  TRIGGERED"));quiet(d,endstop_refresh);
  unsigned before=requests;now=s_next_query;endstop_refresh(NULL);assert(requests==before+1);endstop_refresh(NULL);assert(requests==before+1);
  strcpy(state.printer_state,"printing");endstop_refresh(NULL);assert(strstr(lv_label_get_text(s_body),"Readings paused"));quiet(d,endstop_refresh);assert(requests==before+1);
  strcpy(state.printer_state,"ready");websocket=false;endstop_refresh(NULL);assert(strstr(lv_label_get_text(s_body),"Live readings unavailable"));quiet(d,endstop_refresh);assert(requests==before+1);
  websocket=true;generation++;endstop_refresh(NULL);same(s_body,"Printer changed. Close and reopen Endstops.");quiet(d,endstop_refresh);assert(requests==before+1);ui_endstop_status_close();assert(!s_body && !s_popup);
 }
 s=NULL;lv_display_delete(d);lv_deinit();puts("PASS: Tools live refresh silence, motion limits/driver discovery/selection/fault palettes, endstop transitions/query cadence/print/offline/profile guards and cleanup in four themes/text sizes");
}
