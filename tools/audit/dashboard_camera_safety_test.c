#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "lvgl.h"
static void source_set(lv_obj_t *,const void *);
#define lv_image_set_src source_set
#include "ui_dashboard.c"
#undef lv_image_set_src
#include "custom_theme.h"
static unsigned allocated,freed,takes,starts,stops,fullscreen;
static uint32_t generation=1;
static bool pending,busy=true,configured=true,start_ok=true;
static uint8_t *offered;static size_t offered_size;static int offered_w,offered_h;
static void (*take_hook)(void),(*busy_hook)(void),(*start_hook)(void),(*stop_hook)(void),(*get_hook)(void),(*default_hook)(void),(*source_hook)(void);
static void hook(void (**slot)(void)){if(*slot){void(*fn)(void)=*slot;*slot=NULL;fn();}}
static void source_set(lv_obj_t *object,const void *source){lv_image_set_src(object,source);hook(&source_hook);}
void *heap_caps_malloc(size_t n,uint32_t caps){(void)caps;void*p=malloc(n);assert(p);allocated++;return p;}
void heap_caps_free(void *p){if(p){freed++;free(p);}}
uint32_t moonraker_config_generation(void){return generation;}
int moonraker_config_active_profile_index(void){return 0;}
size_t camera_catalog_default(int p){assert(p==0);hook(&default_hook);return 0;}
bool camera_catalog_get(int p,size_t i,camera_catalog_entry_t *entry){assert(p==0&&i==0);hook(&get_hook);*entry=(camera_catalog_entry_t){.configured=configured};strcpy(entry->stream_url,"http://camera.local/stream");return configured;}
bool camera_stream_take_result(uint8_t **p,size_t*n,int*w,int*h,bool*ok){takes++;bool result=pending;if(pending){pending=false;*p=offered;*n=offered_size;*w=offered_w;*h=offered_h;*ok=true;}hook(&take_hook);return result;}
bool camera_stream_busy(void){bool result=busy;hook(&busy_hook);return result;}
bool camera_stream_start(const char *url){assert(!strcmp(url,"http://camera.local/stream"));starts++;busy=start_ok;hook(&start_hook);return start_ok;}
void camera_stream_request_stop(void){stops++;busy=false;if(pending){heap_caps_free(offered);pending=false;}hook(&stop_hook);}
void ui_camera_show_fullscreen(void){fullscreen++;}
bool custom_theme_color_override(uint32_t a,uint32_t b,uint32_t c,uint32_t*d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_metric_override(int32_t a,int32_t b,int32_t c,int32_t*d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_accent_override(uint8_t a,uint32_t*b){(void)a;(void)b;return false;}
bool custom_theme_surface_opacity(uint8_t*a){(void)a;return false;}
static void build(void)
{
 assert(!dash32_root);++dash32_camera_epoch;dash32_root=lv_obj_create(lv_screen_active());dash32_page.root=dash32_root;
 lv_obj_add_event_cb(dash32_root,dashboard_camera_root_deleted,LV_EVENT_DELETE,NULL);
 dash32_thumb_box=lv_obj_create(dash32_root);lv_obj_set_size(dash32_thumb_box,300,180);lv_obj_add_event_cb(dash32_thumb_box,dashboard_camera_field_deleted,LV_EVENT_DELETE,NULL);
 dash32_camera_image=lv_image_create(dash32_thumb_box);lv_obj_add_flag(dash32_camera_image,LV_OBJ_FLAG_CLICKABLE);lv_obj_add_event_cb(dash32_camera_image,dashboard_camera_field_deleted,LV_EVENT_DELETE,NULL);lv_obj_add_event_cb(dash32_camera_image,dashboard_camera_open_fullscreen_cb,LV_EVENT_CLICKED,NULL);
 dash32_camera_status=lv_label_create(dash32_thumb_box);lv_obj_add_event_cb(dash32_camera_status,dashboard_camera_field_deleted,LV_EVENT_DELETE,NULL);
 dash32_camera_toggle=ui_button_create(dash32_root,UI_BUTTON_SECONDARY,"CAMERA");lv_obj_add_event_cb(dash32_camera_toggle,dashboard_camera_field_deleted,LV_EVENT_DELETE,NULL);lv_obj_add_event_cb(dash32_camera_toggle,dashboard_camera_toggle_cb,LV_EVENT_CLICKED,NULL);
}
static void rebuild(void){lv_obj_delete(dash32_root);build();}
static void off(void){dashboard_camera_mode_set(false);}
static void nested(void){dashboard_camera_poll_cb(NULL);}
static void changed(void){generation++;}
static void delete_image(void){lv_obj_delete(dash32_camera_image);}
static void offer(int w,int h,size_t n){offered=heap_caps_malloc(n,0);memset(offered,1,n);offered_size=n;offered_w=w;offered_h=h;pending=true;}
static void reopen_frame(void){rebuild();dashboard_camera_mode_set(true);offer(3,2,12);dashboard_camera_poll_cb(NULL);}
static unsigned timers(void){unsigned n=0;for(lv_timer_t*t=lv_timer_get_next(NULL);t;t=lv_timer_get_next(t))n++;return n;}
int main(void)
{
 lv_init();lv_display_t *display=lv_display_create(1024,600);
 for(unsigned theme=0;theme<5;theme++)for(unsigned density=0;density<3;density++)for(unsigned large=0;large<2;large++){
  ui_theme_set_active(theme);ui_theme_set_density(density);ui_theme_set_accessibility((ui_accessibility_t){.large_text=large});unsigned baseline=timers();build();configured=true;busy=true;
  unsigned before=starts;lv_obj_add_state(dash32_camera_toggle,LV_STATE_DISABLED);lv_obj_send_event(dash32_camera_toggle,LV_EVENT_CLICKED,NULL);assert(!dash32_camera_mode&&starts==before);lv_obj_remove_state(dash32_camera_toggle,LV_STATE_DISABLED);
  lv_obj_t *fake=lv_button_create(dash32_root);lv_obj_add_event_cb(fake,dashboard_camera_toggle_cb,LV_EVENT_CLICKED,NULL);lv_obj_send_event(fake,LV_EVENT_CLICKED,NULL);assert(!dash32_camera_mode);lv_obj_delete(fake);
  dashboard_camera_mode_set(true);offer(3,2,12);unsigned reads=takes;lv_timer_t*other=lv_timer_create(dashboard_camera_poll_cb,60000,NULL);dashboard_camera_poll_cb(other);assert(takes==reads);lv_timer_delete(other);
  take_hook=nested;dashboard_camera_poll_cb(dash32_camera_timer);assert(dash32_camera_frame&&takes==reads+1);
  for(unsigned invalid=0;invalid<4;invalid++){offer(3,2,12);if(invalid==0)offered_w=0;if(invalid==1)offered_h=65536;if(invalid==2)offered_w=-1;if(invalid==3)offered_size=2;dashboard_camera_poll_cb(NULL);assert(!dash32_camera_frame);}
  offer(3,2,12);take_hook=off;dashboard_camera_poll_cb(NULL);assert(!dash32_camera_frame&&!dash32_camera_mode&&!dash32_camera_timer);
  dashboard_camera_mode_set(true);offer(3,2,12);take_hook=rebuild;dashboard_camera_poll_cb(NULL);assert(dash32_root&&!dash32_camera_frame&&!dash32_camera_mode);
  dashboard_camera_mode_set(true);offer(3,2,12);take_hook=changed;dashboard_camera_poll_cb(NULL);assert(!dash32_camera_frame);dashboard_camera_poll_cb(NULL);assert(dash32_camera_generation==generation);
  busy=false;default_hook=rebuild;dashboard_camera_poll_cb(NULL);assert(!dash32_camera_mode&&!dash32_camera_frame);
  dashboard_camera_mode_set(true);busy=false;get_hook=rebuild;dashboard_camera_poll_cb(NULL);assert(!dash32_camera_mode);
  dashboard_camera_mode_set(true);busy=false;busy_hook=rebuild;dashboard_camera_poll_cb(NULL);assert(!dash32_camera_mode);
  dashboard_camera_mode_set(true);busy=false;start_hook=rebuild;dashboard_camera_poll_cb(NULL);assert(!dash32_camera_mode);
  dashboard_camera_mode_set(true);offer(3,2,12);source_hook=delete_image;dashboard_camera_poll_cb(NULL);assert(!dash32_camera_image&&!dash32_camera_frame&&!dash32_camera_timer);rebuild();
  dashboard_camera_mode_set(true);offer(3,2,12);dashboard_camera_poll_cb(NULL);source_hook=reopen_frame;dashboard_camera_release_frame();assert(dash32_camera_frame&&lv_image_get_src(dash32_camera_image)==&dash32_camera_dsc);
  ui_dashboard_set_camera_quiesced(true);assert(!dash32_camera_mode&&!dash32_camera_timer&&!dash32_camera_frame);ui_dashboard_set_camera_quiesced(false);assert(dash32_camera_mode);
  unsigned fs=fullscreen;lv_obj_add_state(dash32_camera_image,LV_STATE_DISABLED);lv_obj_send_event(dash32_camera_image,LV_EVENT_CLICKED,NULL);assert(fullscreen==fs);lv_obj_remove_state(dash32_camera_image,LV_STATE_DISABLED);lv_obj_send_event(dash32_camera_image,LV_EVENT_CLICKED,NULL);assert(fullscreen==fs+1&&!dash32_camera_mode);
  configured=false;ui_dashboard_refresh_camera();assert(lv_obj_has_flag(dash32_camera_toggle,LV_OBJ_FLAG_HIDDEN));configured=true;ui_dashboard_refresh_camera();assert(!lv_obj_has_flag(dash32_camera_toggle,LV_OBJ_FLAG_HIDDEN));
  dashboard_camera_mode_set(true);lv_obj_delete(dash32_camera_status);assert(!dash32_camera_status);dashboard_camera_set_status("safe");lv_obj_delete(dash32_camera_toggle);assert(!dash32_camera_mode&&!dash32_camera_toggle&&!dash32_camera_timer);ui_dashboard_destroy();assert(!dash32_root&&!dash32_page.root&&allocated==freed&&timers()==baseline);
 }
 lv_display_delete(display);lv_deinit();puts("PASS: actual Dashboard camera owner guards, malformed/stale frames, nested polls, callback replacement, field/root retirement, quiescence, fullscreen routing and balanced buffers/timers across all themes/densities/text sizes");
}
