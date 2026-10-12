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
static bool save_ok=true;
static unsigned view_saves,default_saves;
static size_t default_camera;
static void (*save_hook)(void);
static uint32_t config_generation=1;
uint32_t moonraker_config_generation(void){return config_generation;}
int moonraker_config_active_profile_index(void){return 0;}
bool camera_catalog_get(int p,size_t i,camera_catalog_entry_t *e){(void)p;(void)i;*e=entry;return entry.configured;}
size_t camera_catalog_default(int p){(void)p;return default_camera;}
bool camera_catalog_set_default(int p,size_t i){(void)p;default_saves++;if(save_hook)save_hook();if(!save_ok)return false;default_camera=i;return true;}
bool camera_catalog_set_view(int p,size_t i,unsigned rot,bool h,bool v){(void)p;(void)i;view_saves++;if(save_hook)save_hook();if(!save_ok)return false;entry.rotation=rot;entry.mirror_horizontal=h;entry.mirror_vertical=v;return true;}
void ui_printer_profiles_open_camera_setup(int p,ui_printer_profiles_active_changed_cb_t a,ui_printer_profiles_discover_cb_t b){(void)p;(void)a;(void)b;}
static bool pending,busy=true;
static unsigned stream_starts,stream_stops;
static uint8_t *next_pixels;static int next_width,next_height;static size_t next_size;static bool next_ok;
bool camera_stream_take_result(uint8_t **p,size_t *n,int *w,int *h,bool *ok){if(!pending)return false;pending=false;*p=next_pixels;*n=next_size;*w=next_width;*h=next_height;*ok=next_ok;return true;}
bool camera_stream_busy(void){return busy;}
bool camera_stream_start(const char *url){(void)url;stream_starts++;busy=true;return true;}
void camera_stream_request_stop(void){stream_stops++;busy=false;if(pending){heap_caps_free(next_pixels);pending=false;next_pixels=NULL;}}
void camera_stream_stop(void){camera_stream_request_stop();}
void ui_shell_raise_topbar(void){}
void ui_shell_raise_nav(void){}
static void invalidated(lv_event_t *e){(void)e;invalidations++;}
static uint16_t raster[1024*600];
static void flush(lv_display_t *d,const lv_area_t *a,uint8_t *p){uint16_t *v=(uint16_t*)p;for(int y=a->y1;y<=a->y2;y++)for(int x=a->x1;x<=a->x2;x++)raster[y*1024+x]=*v++;lv_display_flush_ready(d);}
static void offer(int w,int h,size_t size){next_pixels=heap_caps_malloc(size,MALLOC_CAP_SPIRAM);for(size_t i=0;i<size/2;i++)((uint16_t*)next_pixels)[i]=(uint16_t)(i+1);next_width=w;next_height=h;next_size=size;next_ok=true;pending=true;}
static void fixture(void){
 s_camera_index=0;default_camera=0;save_ok=true;save_hook=NULL;
 s_root=lv_obj_create(lv_screen_active());s_card=lv_obj_create(s_root);s_image=lv_image_create(s_card);lv_obj_set_size(s_image,UI_PAGE_ROOT_WIDTH,UI_PAGE_ROOT_HEIGHT);
 s_camera_generation=config_generation;s_retry_button=lv_button_create(s_root);lv_label_create(s_retry_button);
 s_status=lv_label_create(s_root);s_fullscreen_button=lv_button_create(s_root);lv_label_create(s_fullscreen_button);
 s_configure_button=lv_button_create(s_root);s_camera_selector=lv_button_create(s_root);lv_label_create(s_camera_selector);s_view_button=lv_button_create(s_root);
 s_view_width=UI_PAGE_ROOT_WIDTH;s_view_height=UI_PAGE_ROOT_HEIGHT;s_setup_active=false;s_fullscreen=false;entry.configured=true;busy=true;
}
static void inside(lv_obj_t *child,lv_obj_t *parent){lv_area_t a,b;lv_obj_get_coords(child,&a);lv_obj_get_coords(parent,&b);assert(a.x1>=b.x1&&a.x2<=b.x2&&a.y1>=b.y1&&a.y2<=b.y2);}
static void dialog_layout(lv_obj_t *popup){lv_obj_update_layout(popup);inside(popup,lv_screen_active());lv_obj_t*footer=lv_obj_get_child(popup,-1),*body=ui_form_body(popup);inside(footer,popup);for(unsigned i=0;i<lv_obj_get_child_count(footer);i++){lv_obj_t*b=lv_obj_get_child(footer,i);inside(b,footer);inside(lv_obj_get_child(b,0),b);assert(lv_obj_get_height(b)>=48);}lv_area_t before,after;lv_obj_get_coords(footer,&before);lv_obj_scroll_to_y(body,lv_obj_get_scroll_bottom(body),LV_ANIM_OFF);lv_obj_update_layout(popup);inside(lv_obj_get_child(body,-1),body);assert(lv_obj_get_scroll_bottom(body)<=1);lv_obj_get_coords(footer,&after);assert(!memcmp(&before,&after,sizeof(before)));lv_obj_scroll_to_y(body,0,LV_ANIM_OFF);}
static void click(lv_obj_t*b){lv_obj_send_event(b,LV_EVENT_CLICKED,NULL);}
static lv_obj_t *rotation_option(unsigned i){return lv_obj_get_child(lv_obj_get_child(ui_form_body(s_view_popup),2),i);}
static void reenter_save(void){click(rotation_option(0));camera_view_open_cb(NULL);camera_picker_open_cb(NULL);}
static void retire_save(void){lv_obj_delete(s_view_popup);}
static void switch_save(void){config_generation++;}
static void capture(unsigned theme,const char *name){const char*folder=getenv("CAMERA_DIALOG_SCREENSHOTS");if(!folder)return;lv_refr_now(NULL);char path[512];snprintf(path,sizeof(path),"%s/camera-%s-theme%u.ppm",folder,name,theme);FILE*f=fopen(path,"wb");assert(f);fprintf(f,"P6\n1024 600\n255\n");for(unsigned i=0;i<1024*600;i++){uint16_t v=raster[i];unsigned char c[3]={(v>>11)*255/31,((v>>5)&63)*255/63,(v&31)*255/31};fwrite(c,1,3,f);}fclose(f);}
static void dialog_checks(lv_display_t*d){
 for(unsigned theme=0;theme<5;theme++)for(unsigned density=0;density<3;density++)for(unsigned large=0;large<2;large++)for(unsigned narrow=0;narrow<3;narrow++){
  lv_display_set_resolution(d,(int[]){1024,640,480}[narrow],narrow==2?400:600);ui_theme_set_active(theme);ui_theme_set_density(density);ui_theme_set_accessibility((ui_accessibility_t){.large_text=large});fixture();strcpy(entry.name,"Workshop camera with long name");strcpy(entry.stream_url,"http://camera-fixture.local/stream");entry.rotation=90;entry.mirror_horizontal=true;entry.mirror_vertical=false;
  camera_view_open_cb(NULL);assert(s_view_popup);dialog_layout(s_view_popup);if(!narrow&&density==1&&large)capture(theme,"view");lv_obj_t*original=s_view_popup;camera_view_open_cb(NULL);assert(s_view_popup==original);assert(lv_obj_has_state(rotation_option(1),LV_STATE_CHECKED));
  lv_area_t r0,r1,r2;lv_obj_get_coords(rotation_option(0),&r0);lv_obj_get_coords(rotation_option(1),&r1);lv_obj_get_coords(rotation_option(2),&r2);assert(r0.y1==r1.y1&&r2.y1>r0.y2);
  lv_obj_t*body=ui_form_body(s_view_popup);for(unsigned i=0;i<4;i++){lv_obj_t*b=rotation_option(i);inside(lv_obj_get_child(b,0),b);assert(lv_obj_get_height(b)>=48);}lv_obj_t*mirrors=lv_obj_get_child(body,4);assert(lv_obj_has_state(lv_obj_get_child(mirrors,0),LV_STATE_CHECKED));
  unsigned before=view_saves;save_ok=false;click(rotation_option(2));assert(view_saves==before+1&&s_view_popup==original&&entry.rotation==90&&strstr(lv_label_get_text(s_view_feedback),"Could not save"));dialog_layout(s_view_popup);save_ok=true;save_hook=reenter_save;click(rotation_option(2));save_hook=NULL;assert(view_saves==before+2&&!s_view_popup&&entry.rotation==180);
  camera_view_open_cb(NULL);click(lv_obj_get_child(lv_obj_get_child(ui_form_body(s_view_popup),4),1));assert(!s_view_popup&&entry.mirror_vertical&&entry.mirror_horizontal);
  camera_view_open_cb(NULL);click(lv_obj_get_child(lv_obj_get_child(s_view_popup,-1),1));assert(!s_view_popup&&!entry.rotation&&!entry.mirror_horizontal&&!entry.mirror_vertical);
  camera_picker_open_cb(NULL);assert(s_camera_picker_popup);dialog_layout(s_camera_picker_popup);if(!narrow&&density==1&&large)capture(theme,"picker");body=ui_form_body(s_camera_picker_popup);for(unsigned i=0;i<4;i++){lv_obj_t*b=lv_obj_get_child(body,i);inside(lv_obj_get_child(b,0),b);assert(lv_obj_get_height(b)>=56);}original=s_camera_picker_popup;camera_picker_open_cb(NULL);assert(s_camera_picker_popup==original);
  before=default_saves;save_ok=false;click(lv_obj_get_child(body,2));assert(default_saves==before+1&&s_camera_picker_popup==original&&!s_camera_index&&strstr(lv_label_get_text(s_picker_feedback),"Could not save"));save_ok=true;click(lv_obj_get_child(body,2));assert(default_saves==before+2&&!s_camera_picker_popup&&s_camera_index==2&&default_camera==2);
  for(unsigned i=0;i<10;i++){camera_view_open_cb(NULL);lv_obj_delete(s_view_popup);assert(!s_view_popup&&!s_view_feedback&&!s_view_owner);camera_view_set_cb(NULL);camera_picker_open_cb(NULL);lv_obj_delete(s_camera_picker_popup);assert(!s_camera_picker_popup&&!s_picker_feedback&&!s_picker_owner);camera_picker_select_cb(NULL);}
  camera_view_open_cb(NULL);strcpy(entry.stream_url,"http://replacement-camera.local/stream");before=view_saves;click(rotation_option(1));assert(view_saves==before&&s_view_popup&&strstr(lv_label_get_text(s_view_feedback),"configuration changed"));camera_view_close_cb(NULL);
  camera_picker_open_cb(NULL);config_generation++;before=default_saves;click(lv_obj_get_child(ui_form_body(s_camera_picker_popup),0));assert(default_saves==before&&s_camera_picker_popup&&strstr(lv_label_get_text(s_picker_feedback),"Printer changed"));camera_close_popups();s_camera_generation=config_generation;
  camera_view_open_cb(NULL);before=view_saves;save_hook=retire_save;click(rotation_option(1));save_hook=NULL;assert(view_saves==before+1&&!s_view_popup);
  camera_view_open_cb(NULL);before=view_saves;save_hook=switch_save;click(rotation_option(1));save_hook=NULL;assert(view_saves==before+1&&s_view_popup);click(rotation_option(1));assert(view_saves==before+1);camera_close_popups();s_camera_generation=config_generation;
  entry.configured=false;camera_picker_open_cb(NULL);assert(s_camera_picker_popup&&lv_obj_check_type(lv_obj_get_child(ui_form_body(s_camera_picker_popup),0),&lv_label_class));dialog_layout(s_camera_picker_popup);camera_close_popups();entry.configured=true;
  ui_camera_hide();assert(!s_view_popup&&!s_camera_picker_popup);ui_camera_destroy();
 }
 lv_display_set_resolution(d,1024,600);
}
static unsigned timer_count(void){unsigned count=0;for(lv_timer_t*t=lv_timer_get_next(NULL);t;t=lv_timer_get_next(t))count++;return count;}
static void lifecycle_checks(lv_display_t*d){
 lv_display_set_resolution(d,1024,600);
 for(unsigned theme=0;theme<5;theme++)for(unsigned density=0;density<3;density++)for(unsigned large=0;large<2;large++){
  ui_theme_set_active(theme);ui_theme_set_density(density);ui_theme_set_accessibility((ui_accessibility_t){.large_text=large});entry.configured=true;save_ok=true;save_hook=NULL;s_setup_active=false;busy=false;entry.rotation=0;entry.mirror_horizontal=entry.mirror_vertical=false;
  unsigned baseline=timer_count();
  for(unsigned i=0;i<5;i++){
   ui_camera_show();offer(3,2,12);camera_poll_cb(NULL);assert(s_frame&&s_refresh_timer);camera_view_open_cb(NULL);camera_picker_open_cb(NULL);assert(s_view_popup&&s_camera_picker_popup);
   camera_set_viewport(true);lv_obj_t*detached=s_card;assert(lv_obj_get_parent(detached)==lv_layer_top());lv_obj_delete(s_root);assert(!s_root&&!s_image&&!s_card&&!s_frame&&!s_refresh_timer&&!s_view_popup&&!s_camera_picker_popup&&!busy&&!lv_obj_get_child_count(lv_layer_top())&&timer_count()==baseline);
   unsigned starts=stream_starts,stops=stream_stops;camera_poll_cb(NULL);camera_retry_cb(NULL);camera_fullscreen_cb(NULL);camera_configure_cb(NULL);camera_view_open_cb(NULL);camera_picker_open_cb(NULL);assert(stream_starts==starts&&stream_stops==stops&&!s_root);
  }
  ui_camera_show();offer(3,2,12);camera_poll_cb(NULL);assert(s_frame);camera_set_viewport(true);lv_obj_delete(s_image);assert(!s_image&&!s_frame&&!s_refresh_timer&&!busy&&!s_fullscreen&&lv_obj_get_parent(s_card)==s_root&&!lv_obj_get_child_count(lv_layer_top()));camera_poll_cb(NULL);ui_camera_show();assert(s_image&&s_card&&s_refresh_timer);ui_camera_destroy();assert(timer_count()==baseline);
  ui_camera_show();offer(3,2,12);camera_poll_cb(NULL);lv_obj_delete(s_card);assert(!s_card&&!s_image&&!s_frame&&!s_refresh_timer);ui_camera_show();assert(s_card&&s_image);ui_camera_destroy();assert(timer_count()==baseline);
  ui_camera_show();offer(3,2,12);camera_poll_cb(NULL);camera_view_open_cb(NULL);ui_camera_set_setup_active(true);assert(s_setup_active&&!s_frame&&!s_refresh_timer&&!s_view_popup&&!busy);unsigned starts=stream_starts;ui_camera_show();ui_camera_show_fullscreen();camera_poll_cb(NULL);camera_retry_cb(NULL);assert(!s_refresh_timer&&!s_fullscreen&&stream_starts==starts);ui_camera_set_setup_active(false);assert(s_refresh_timer&&stream_starts==starts+1);offer(3,2,12);camera_poll_cb(NULL);assert(s_frame);
  ui_camera_hide();starts=stream_starts;unsigned stops=stream_stops;camera_poll_cb(NULL);camera_retry_cb(NULL);camera_fullscreen_cb(NULL);camera_configure_cb(NULL);assert(stream_starts==starts&&stream_stops==stops&&!s_refresh_timer&&!s_frame);ui_camera_set_setup_active(true);ui_camera_set_setup_active(false);assert(!s_refresh_timer);ui_camera_show();assert(s_refresh_timer&&stream_starts>starts);
  offer(3,2,12);camera_poll_cb(NULL);assert(s_frame);config_generation++;ui_camera_show();assert(!s_frame&&!pending&&s_camera_generation==config_generation);camera_set_viewport(true);ui_camera_destroy();assert(!lv_obj_get_child_count(lv_layer_top())&&timer_count()==baseline);
 }
}
int main(void){
 lv_init();lv_display_t *d=lv_display_create(1024,600);static uint8_t pixels[1024*50*2];lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);lv_display_set_buffers(d,pixels,NULL,sizeof(pixels),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(d,flush);lv_display_add_event_cb(d,invalidated,LV_EVENT_INVALIDATE_AREA,NULL);
 for(unsigned theme=0;theme<5;theme++)for(unsigned large=0;large<2;large++){
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
  camera_set_viewport(false);assert(lv_image_get_scale(s_image)==((ui_theme_is_studio()?424:UI_PAGE_ROOT_HEIGHT)*256/480));
  offer(10,10,2);camera_poll_cb(NULL);assert(!s_frame && !lv_image_get_src(s_image));
  lv_refr_now(d);invalidations=0;for(int i=0;i<100;i++)camera_mark_unavailable();assert(!invalidations);
  assert(!lv_obj_has_flag(s_retry_button,LV_OBJ_FLAG_HIDDEN));camera_retry_cb(NULL);assert(lv_obj_has_flag(s_retry_button,LV_OBJ_FLAG_HIDDEN)&&busy);
  offer(3,2,12);camera_poll_cb(NULL);assert(s_frame&&strstr(lv_label_get_text(s_status),"LIVE"));offer(3,2,12);config_generation++;camera_poll_cb(NULL);assert(!s_frame&&!pending&&s_camera_generation==config_generation);
  busy=true;offer(3,2,12);camera_poll_cb(NULL);assert(s_frame);ui_camera_hide();assert(!s_frame && !lv_image_get_src(s_image) && !s_refresh_timer && !busy);
  ui_camera_destroy();assert(!s_root && !s_image && !s_frame);
  entry.configured=false;ui_camera_show();lv_obj_update_layout(s_root);
  assert(strstr(lv_label_get_text(s_status),"No camera")&&lv_obj_has_flag(s_retry_button,LV_OBJ_FLAG_HIDDEN));
  camera_mark_unavailable();lv_obj_update_layout(s_root);assert(!lv_obj_has_flag(s_retry_button,LV_OBJ_FLAG_HIDDEN));
  lv_area_t button,root,label;lv_obj_get_coords(s_retry_button,&button);lv_obj_get_coords(s_root,&root);lv_obj_get_coords(s_status,&label);
  assert(button.x1>=root.x1&&button.x2<=root.x2&&button.y1>=root.y1&&button.y2<=root.y2);
  assert(label.x1>=root.x1&&label.x2<=root.x2&&label.y1>=root.y1&&label.y2<=root.y2);
  lv_obj_t *retry_text=lv_obj_get_child(s_retry_button,0);assert(lv_obj_get_self_width(retry_text)<=lv_obj_get_content_width(s_retry_button));
  entry.configured=true;lv_obj_send_event(s_retry_button,LV_EVENT_CLICKED,NULL);assert(busy&&lv_obj_has_flag(s_retry_button,LV_OBJ_FLAG_HIDDEN));ui_camera_destroy();
 }
 dialog_checks(d);
 lifecycle_checks(d);
 dash32_camera_image=lv_image_create(lv_screen_active());dash32_camera_frame=heap_caps_malloc(12,0);
 dash32_camera_dsc.header.magic=LV_IMAGE_HEADER_MAGIC;dash32_camera_dsc.header.cf=LV_COLOR_FORMAT_RGB565;dash32_camera_dsc.header.w=3;dash32_camera_dsc.header.h=2;dash32_camera_dsc.header.stride=6;dash32_camera_dsc.data=dash32_camera_frame;dash32_camera_dsc.data_size=12;
 lv_image_set_src(dash32_camera_image,&dash32_camera_dsc);dashboard_camera_release_frame();assert(!dash32_camera_frame && !lv_image_get_src(dash32_camera_image));lv_obj_delete(dash32_camera_image);dash32_camera_image=NULL;
 assert(allocations==frees);lv_display_delete(d);lv_deinit();puts("PASS: camera frame retirement before free, repeated status/transform silence, all rotations/mirrors, immediate fullscreen fit, truncated-frame rejection and hide/destroy ownership across five themes/text sizes plus Dashboard source retirement and native camera dialogs/retry/ownership plus timer-balanced root/fullscreen/surface retirement and setup suspension");
}
