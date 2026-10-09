#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ui_camera.c"
#include "dashboard_camera_retirement.inc"
bool custom_theme_color_override(uint32_t a,uint32_t b,uint32_t c,uint32_t *d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_metric_override(int32_t a,int32_t b,int32_t c,int32_t *d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_accent_override(uint8_t a,uint32_t *b){(void)a;(void)b;return false;}
bool custom_theme_surface_opacity(uint8_t *a){(void)a;return false;}
static unsigned allocations, frees, invalidations;
void *heap_caps_malloc(size_t n,uint32_t caps){(void)caps;allocations++;return malloc(n);}
void *heap_caps_calloc(size_t n,size_t size,uint32_t caps){void *p=heap_caps_malloc(n*size,caps);if(p)memset(p,0,n*size);return p;}
void heap_caps_free(void *p){if(!p)return;assert(!s_image || lv_image_get_src(s_image)!=&s_frame_dsc || p!=s_frame);assert(!dash32_camera_image || lv_image_get_src(dash32_camera_image)!=&dash32_camera_dsc || p!=dash32_camera_frame);frees++;free(p);}
static camera_catalog_entry_t entry;
int moonraker_config_active_profile_index(void){return 0;}
bool camera_catalog_get(int p,size_t i,camera_catalog_entry_t *e){(void)p;(void)i;*e=entry;return entry.configured;}
size_t camera_catalog_default(int p){(void)p;return 0;}
static bool pending,busy=true;
static uint8_t *next_pixels;static int next_width,next_height;static size_t next_size;static bool next_ok;
bool camera_stream_take_result(uint8_t **p,size_t *n,int *w,int *h,bool *ok){if(!pending)return false;pending=false;*p=next_pixels;*n=next_size;*w=next_width;*h=next_height;*ok=next_ok;return true;}
bool camera_stream_busy(void){return busy;}
bool camera_stream_start(const char *url){(void)url;busy=true;return true;}
void camera_stream_request_stop(void){busy=false;}
void camera_stream_stop(void){camera_stream_request_stop();}
void ui_shell_raise_topbar(void){}
void ui_shell_raise_nav(void){}
static void invalidated(lv_event_t *e){(void)e;invalidations++;}
static void flush(lv_display_t *d,const lv_area_t *a,uint8_t *p){(void)a;(void)p;lv_display_flush_ready(d);}
static void offer(int w,int h,size_t size){next_pixels=heap_caps_malloc(size,MALLOC_CAP_SPIRAM);for(size_t i=0;i<size/2;i++)((uint16_t*)next_pixels)[i]=(uint16_t)(i+1);next_width=w;next_height=h;next_size=size;next_ok=true;pending=true;}
static void fixture(void){
 s_root=lv_obj_create(lv_screen_active());s_card=lv_obj_create(s_root);s_image=lv_image_create(s_card);lv_obj_set_size(s_image,UI_PAGE_ROOT_WIDTH,UI_PAGE_ROOT_HEIGHT);
 s_status=lv_label_create(s_root);s_fullscreen_button=lv_button_create(s_root);lv_label_create(s_fullscreen_button);
 s_configure_button=lv_button_create(s_root);s_camera_selector=lv_button_create(s_root);lv_label_create(s_camera_selector);s_view_button=lv_button_create(s_root);
 s_view_width=UI_PAGE_ROOT_WIDTH;s_view_height=UI_PAGE_ROOT_HEIGHT;s_setup_active=false;s_fullscreen=false;entry.configured=true;busy=true;
}
int main(void){
 lv_init();lv_display_t *d=lv_display_create(1024,600);static uint8_t pixels[1024*50*2];lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);lv_display_set_buffers(d,pixels,NULL,sizeof(pixels),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(d,flush);lv_display_add_event_cb(d,invalidated,LV_EVENT_INVALIDATE_AREA,NULL);
 for(unsigned theme=0;theme<4;theme++)for(unsigned large=0;large<2;large++){
  ui_theme_set_active((ui_theme_id_t)theme);ui_theme_set_accessibility((ui_accessibility_t){.large_text=large});fixture();
  for(unsigned rotation=0;rotation<4;rotation++)for(unsigned mirror=0;mirror<4;mirror++){
   entry.rotation=rotation*90;entry.mirror_horizontal=mirror&1;entry.mirror_vertical=mirror&2;busy=true;offer(3,2,12);camera_poll_cb(NULL);
   assert(s_frame && lv_image_get_src(s_image)==&s_frame_dsc && s_frame_dsc.header.w==3 && s_frame_dsc.header.h==2);
   uint16_t *data=(uint16_t*)s_frame;
   for(int y=0;y<2;y++)for(int x=0;x<3;x++){int sx=(mirror&1)?2-x:x,sy=(mirror&2)?1-y:y;assert(data[y*3+x]==sy*3+sx+1);}
   assert(lv_image_get_rotation(s_image)==(int)entry.rotation*10);
   camera_set_status("LIVE | 10 FPS");lv_refr_now(d);invalidations=0;for(int i=0;i<100;i++){camera_apply_view_transform();camera_set_status("LIVE | 10 FPS");}assert(!invalidations);
  }
  entry.rotation=0;entry.mirror_horizontal=entry.mirror_vertical=false;busy=true;offer(640,480,640*480*2);camera_poll_cb(NULL);int before=lv_image_get_scale(s_image);
  camera_set_viewport(true);assert(lv_image_get_scale(s_image)==320 && lv_image_get_scale(s_image)!=before);assert(lv_image_get_rotation(s_image)==0);
  camera_set_viewport(false);assert(lv_image_get_scale(s_image)==(UI_PAGE_ROOT_HEIGHT*256/480));
  offer(10,10,2);camera_poll_cb(NULL);assert(!s_frame && !lv_image_get_src(s_image));
  lv_refr_now(d);invalidations=0;for(int i=0;i<100;i++)camera_mark_unavailable();assert(!invalidations);
  busy=true;offer(3,2,12);camera_poll_cb(NULL);assert(s_frame);ui_camera_hide();assert(!s_frame && !lv_image_get_src(s_image) && !s_refresh_timer && !busy);
  ui_camera_destroy();assert(!s_root && !s_image && !s_frame);
 }
 dash32_camera_image=lv_image_create(lv_screen_active());dash32_camera_frame=heap_caps_malloc(12,0);
 dash32_camera_dsc.header.magic=LV_IMAGE_HEADER_MAGIC;dash32_camera_dsc.header.cf=LV_COLOR_FORMAT_RGB565;dash32_camera_dsc.header.w=3;dash32_camera_dsc.header.h=2;dash32_camera_dsc.header.stride=6;dash32_camera_dsc.data=dash32_camera_frame;dash32_camera_dsc.data_size=12;
 lv_image_set_src(dash32_camera_image,&dash32_camera_dsc);dashboard_camera_release_frame();assert(!dash32_camera_frame && !lv_image_get_src(dash32_camera_image));lv_obj_delete(dash32_camera_image);dash32_camera_image=NULL;
 assert(allocations==frees);lv_display_delete(d);lv_deinit();puts("PASS: camera frame retirement before free, repeated status/transform silence, all rotations/mirrors, immediate fullscreen fit, truncated-frame rejection and hide/destroy ownership across four themes/text sizes plus Dashboard source retirement");
}
