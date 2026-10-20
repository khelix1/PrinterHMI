#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lvgl.h"
static void test_image_src(lv_obj_t *,const void *);
#define lv_image_set_src test_image_src
#include "ui_camera.c"
#undef lv_image_set_src
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
static int missing_camera=-1;
static bool save_ok=true;
static unsigned view_saves,default_saves;
static size_t default_camera;
static void (*save_hook)(void),(*catalog_hook)(void),(*default_hook)(void),(*stop_hook)(void),(*configure_hook)(void),(*take_hook)(void),(*busy_hook)(void),(*start_hook)(void),(*source_hook)(void);
static unsigned takes;
static unsigned configurations;
static void run_hook(void (**slot)(void)){if(*slot){void(*callback)(void)=*slot;*slot=NULL;callback();}}
static void test_image_src(lv_obj_t *image,const void *source){lv_image_set_src(image,source);run_hook(&source_hook);}
static uint32_t config_generation=1;
uint32_t moonraker_config_generation(void){return config_generation;}
int moonraker_config_active_profile_index(void){return 0;}
bool camera_catalog_get(int p,size_t i,camera_catalog_entry_t *e){(void)p;run_hook(&catalog_hook);*e=entry;if((int)i==missing_camera)e->configured=false;return e->configured;}
size_t camera_catalog_default(int p){(void)p;run_hook(&default_hook);return default_camera;}
bool camera_catalog_set_default(int p,size_t i){(void)p;default_saves++;if(save_hook)save_hook();if(!save_ok)return false;default_camera=i;return true;}
bool camera_catalog_set_view(int p,size_t i,unsigned rot,bool h,bool v){(void)p;(void)i;view_saves++;if(save_hook)save_hook();if(!save_ok)return false;entry.rotation=rot;entry.mirror_horizontal=h;entry.mirror_vertical=v;return true;}
void ui_printer_profiles_open_camera_setup(int p,ui_printer_profiles_active_changed_cb_t a,ui_printer_profiles_discover_cb_t b){(void)p;(void)a;(void)b;assert(!s_view_popup && !s_camera_picker_popup);configurations++;run_hook(&configure_hook);}
static bool pending,busy=true,start_ok=true;
static unsigned stream_starts,stream_stops;
static uint8_t *next_pixels;static int next_width,next_height;static size_t next_size;static bool next_ok;
bool camera_stream_take_result(uint8_t **p,size_t *n,int *w,int *h,bool *ok){takes++;bool result=pending;if(pending){pending=false;*p=next_pixels;*n=next_size;*w=next_width;*h=next_height;*ok=next_ok;}run_hook(&take_hook);return result;}
bool camera_stream_busy(void){bool result=busy;run_hook(&busy_hook);return result;}
bool camera_stream_start(const char *url){assert(url);stream_starts++;busy=start_ok;run_hook(&start_hook);return start_ok;}
void camera_stream_request_stop(void){stream_stops++;busy=false;if(pending){heap_caps_free(next_pixels);pending=false;next_pixels=NULL;}run_hook(&stop_hook);}
void camera_stream_stop(void){assert(!"Camera UI must request stop without waiting");}
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
static void dialog_layout(lv_obj_t *popup){lv_obj_update_layout(popup);inside(popup,lv_screen_active());lv_obj_t*footer=lv_obj_get_child(popup,-1),*body=ui_form_body(popup);assert(lv_obj_get_scrollbar_mode(body)==LV_SCROLLBAR_MODE_OFF);inside(footer,popup);lv_obj_t *title=lv_obj_get_child(popup,0);inside(title,popup);assert(lv_obj_get_style_border_side(title,0)==LV_BORDER_SIDE_BOTTOM&&lv_obj_get_style_border_width(title,0)>0);lv_area_t title_area,body_area,footer_area;lv_obj_get_coords(title,&title_area);lv_obj_get_coords(body,&body_area);lv_obj_get_coords(footer,&footer_area);assert(title_area.y2<body_area.y1&&body_area.y2<footer_area.y1);for(unsigned i=0;i<lv_obj_get_child_count(footer);i++){lv_obj_t*b=lv_obj_get_child(footer,i);inside(b,footer);inside(lv_obj_get_child(b,0),b);assert(lv_obj_get_height(b)>=48);}lv_area_t before,after;lv_obj_get_coords(footer,&before);lv_obj_scroll_to_y(body,lv_obj_get_scroll_bottom(body),LV_ANIM_OFF);lv_obj_update_layout(popup);inside(lv_obj_get_child(body,-1),body);assert(lv_obj_get_scroll_bottom(body)<=1);lv_obj_get_coords(footer,&after);assert(!memcmp(&before,&after,sizeof(before)));lv_obj_scroll_to_y(body,0,LV_ANIM_OFF);}
static void click(lv_obj_t*b){lv_obj_send_event(b,LV_EVENT_CLICKED,NULL);}
static lv_obj_t *rotation_option(unsigned i){return lv_obj_get_child(lv_obj_get_child(ui_form_body(s_view_popup),2),i);}
static void reenter_save(void){click(rotation_option(0));camera_view_open_cb(NULL);camera_picker_open_cb(NULL);}
static void retire_save(void){lv_obj_delete(s_view_popup);}
static void switch_save(void){config_generation++;}
static void capture(unsigned theme,const char *name){const char*folder=getenv("CAMERA_DIALOG_SCREENSHOTS");if(!folder)return;lv_refr_now(NULL);char path[512];snprintf(path,sizeof(path),"%s/camera-%s-theme%u.ppm",folder,name,theme);FILE*f=fopen(path,"wb");assert(f);fprintf(f,"P6\n1024 600\n255\n");for(unsigned i=0;i<1024*600;i++){uint16_t v=raster[i];unsigned char c[3]={(v>>11)*255/31,((v>>5)&63)*255/63,(v&31)*255/31};fwrite(c,1,3,f);}fclose(f);}
static void dialog_checks(lv_display_t*d){
 for(unsigned theme=0;theme<5;theme++)for(unsigned density=0;density<3;density++)for(unsigned large=0;large<2;large++)for(unsigned narrow=0;narrow<3;narrow++){
  lv_display_set_resolution(d,(int[]){1024,640,480}[narrow],narrow==2?400:600);ui_theme_set_active(theme);ui_theme_set_density(density);ui_theme_set_accessibility((ui_accessibility_t){.large_text=large});fixture();strcpy(entry.name,"Workshop camera with long name");if(narrow){memset(entry.name,'W',sizeof(entry.name)-1);entry.name[sizeof(entry.name)-1]=0;}strcpy(entry.stream_url,"http://camera-fixture.local/stream");entry.rotation=90;entry.mirror_horizontal=true;entry.mirror_vertical=false;
  camera_view_open_cb(NULL);assert(s_view_popup);dialog_layout(s_view_popup);if(!narrow&&density==1&&large)capture(theme,"view");lv_obj_t*original=s_view_popup;camera_view_open_cb(NULL);assert(s_view_popup==original);assert(lv_obj_has_state(rotation_option(1),LV_STATE_CHECKED));
  lv_area_t r0,r1,r2;lv_obj_get_coords(rotation_option(0),&r0);lv_obj_get_coords(rotation_option(1),&r1);lv_obj_get_coords(rotation_option(2),&r2);assert(r0.y1==r1.y1&&r2.y1>r0.y2);
  lv_obj_t*body=ui_form_body(s_view_popup);for(unsigned i=0;i<4;i++){lv_obj_t*b=rotation_option(i);inside(lv_obj_get_child(b,0),b);assert(lv_obj_get_height(b)>=48);}lv_obj_t*mirrors=lv_obj_get_child(body,4);assert(lv_obj_has_state(lv_obj_get_child(mirrors,0),LV_STATE_CHECKED));
  unsigned before=view_saves;save_ok=false;click(rotation_option(2));assert(view_saves==before+1&&s_view_popup==original&&entry.rotation==90&&strstr(lv_label_get_text(s_view_feedback),"Could not save"));dialog_layout(s_view_popup);save_ok=true;save_hook=reenter_save;click(rotation_option(2));save_hook=NULL;assert(view_saves==before+2&&!s_view_popup&&entry.rotation==180);
  camera_view_open_cb(NULL);click(lv_obj_get_child(lv_obj_get_child(ui_form_body(s_view_popup),4),1));assert(!s_view_popup&&entry.mirror_vertical&&entry.mirror_horizontal);
  camera_view_open_cb(NULL);click(lv_obj_get_child(lv_obj_get_child(s_view_popup,-1),1));assert(!s_view_popup&&!entry.rotation&&!entry.mirror_horizontal&&!entry.mirror_vertical);
  camera_picker_open_cb(NULL);assert(s_camera_picker_popup);dialog_layout(s_camera_picker_popup);if(!narrow&&density==1&&large)capture(theme,"picker");body=ui_form_body(s_camera_picker_popup);for(unsigned i=0;i<4;i++){lv_obj_t*b=lv_obj_get_child(body,i);inside(lv_obj_get_child(b,0),b);assert(lv_obj_get_height(b)>=56);lv_obj_t *name=lv_obj_get_child(b,0);assert(lv_obj_get_style_text_font(name,0)->line_height==UI_FONT_BODY_LARGE->line_height);assert(!strcmp(lv_label_get_text(name),entry.name));assert(lv_label_get_long_mode(name)==LV_LABEL_LONG_WRAP);assert(lv_obj_get_style_text_align(name,0)==LV_TEXT_ALIGN_LEFT);assert(lv_obj_has_state(b,LV_STATE_CHECKED)==(i==s_camera_index));
   if(i==default_camera){lv_obj_t *caption=lv_obj_get_child(b,1);inside(caption,b);assert(!strcmp(lv_label_get_text(caption),"Default camera"));lv_area_t na,ca;lv_obj_get_coords(name,&na);lv_obj_get_coords(caption,&ca);assert(na.x1==ca.x1&&na.y2<ca.y1);}else assert(lv_obj_get_child_count(b)==1);
   if(ui_theme_is_studio()){assert(lv_obj_get_style_radius(b,0)==0);assert(lv_obj_get_style_border_side(b,0)==LV_BORDER_SIDE_BOTTOM);}}original=s_camera_picker_popup;camera_picker_open_cb(NULL);assert(s_camera_picker_popup==original);
  before=default_saves;save_ok=false;click(lv_obj_get_child(body,2));assert(default_saves==before+1&&s_camera_picker_popup==original&&!s_camera_index&&strstr(lv_label_get_text(s_picker_feedback),"Could not save"));save_ok=true;click(lv_obj_get_child(body,2));assert(default_saves==before+2&&!s_camera_picker_popup&&s_camera_index==2&&default_camera==2);
  for(unsigned i=0;i<10;i++){camera_view_open_cb(NULL);lv_obj_delete(s_view_popup);assert(!s_view_popup&&!s_view_feedback&&!s_view_owner);camera_view_set_cb(NULL);camera_picker_open_cb(NULL);lv_obj_delete(s_camera_picker_popup);assert(!s_camera_picker_popup&&!s_picker_feedback&&!s_picker_owner);camera_picker_select_cb(NULL);}
  camera_view_open_cb(NULL);strcpy(entry.stream_url,"http://replacement-camera.local/stream");before=view_saves;click(rotation_option(1));assert(view_saves==before&&s_view_popup&&strstr(lv_label_get_text(s_view_feedback),"configuration changed"));camera_view_close_cb(NULL);
  camera_picker_open_cb(NULL);config_generation++;before=default_saves;click(lv_obj_get_child(ui_form_body(s_camera_picker_popup),0));assert(default_saves==before&&s_camera_picker_popup&&strstr(lv_label_get_text(s_picker_feedback),"Printer changed"));camera_close_popups();s_camera_generation=config_generation;
  camera_view_open_cb(NULL);before=view_saves;save_hook=retire_save;click(rotation_option(1));save_hook=NULL;assert(view_saves==before+1&&!s_view_popup);
  camera_view_open_cb(NULL);before=view_saves;save_hook=switch_save;click(rotation_option(1));save_hook=NULL;assert(view_saves==before+1&&s_view_popup);click(rotation_option(1));assert(view_saves==before+1);camera_close_popups();s_camera_generation=config_generation;
  entry.configured=false;camera_picker_open_cb(NULL);assert(s_camera_picker_popup&&lv_obj_check_type(lv_obj_get_child(ui_form_body(s_camera_picker_popup),0),&lv_label_class));dialog_layout(s_camera_picker_popup);if(!narrow&&density==1&&large)capture(theme,"picker-empty");camera_close_popups();entry.configured=true;
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
static void reopen_page(void){ui_camera_destroy();ui_camera_show();}
static void reopen_view_deleted(lv_event_t *event){(void)event;camera_view_open_cb(NULL);}
static void reopen_picker_deleted(lv_event_t *event){(void)event;camera_picker_open_cb(NULL);}
static void retire_view_read(void){camera_view_close_cb(NULL);}
static void retire_picker_read(void){camera_picker_close_cb(NULL);}
static void delete_selector(void){lv_obj_delete(s_camera_selector);}
static void nested_page_actions(void)
{
 click(s_configure_button);click(s_retry_button);click(s_fullscreen_button);
 if(s_view_actions[0])click(s_view_actions[0]);
 if(s_view_close)click(s_view_close);
 if(s_picker_close)click(s_picker_close);
}
static lv_point_t touch;static bool pressed;
static void read_touch(lv_indev_t *input,lv_indev_data_t *data){(void)input;data->point=touch;data->state=pressed?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED;}
static void tap(lv_indev_t *input,lv_obj_t *object){lv_obj_update_layout(lv_layer_top());lv_obj_update_layout(s_root);lv_area_t area;lv_obj_get_coords(object,&area);touch=(lv_point_t){(area.x1+area.x2)/2,(area.y1+area.y2)/2};pressed=true;lv_indev_read(input);pressed=false;lv_indev_read(input);}
static void hide_camera_on_dialog_delete(lv_event_t *event){(void)event;ui_camera_hide();}
static void control_checks(lv_display_t *display)
{
 lv_display_set_resolution(display,1024,600);lv_indev_t *input=lv_indev_create();lv_indev_set_type(input,LV_INDEV_TYPE_POINTER);lv_indev_set_read_cb(input,read_touch);
 for(unsigned theme=0;theme<5;theme++)for(unsigned density=0;density<3;density++)for(unsigned large=0;large<2;large++){
  ui_theme_set_active(theme);ui_theme_set_density(density);ui_theme_set_accessibility((ui_accessibility_t){.large_text=large});entry.configured=true;strcpy(entry.stream_url,"http://camera-fixture.local/stream");s_setup_active=false;busy=false;save_ok=true;ui_camera_show();
  lv_obj_t *selector_label=lv_obj_get_child(s_camera_selector,0);
  assert(lv_label_get_long_mode(selector_label)==LV_LABEL_LONG_DOT);
  /* A long persisted camera name must stay static after repeated refreshes. */
  strcpy(entry.name,"Workshop camera long name test");
  camera_update_selector();lv_obj_update_layout(s_root);lv_refr_now(display);
  invalidations=0;
  for(unsigned i=0;i<20;i++)camera_update_selector();
  assert(!invalidations && lv_label_get_long_mode(selector_label)==LV_LABEL_LONG_DOT);
  const char *caption=lv_label_get_text(lv_obj_get_child(s_configure_button,0));
  assert(strstr(caption,"Configure"));
  camera_set_status("Camera unavailable. Retrying...");assert(!lv_obj_has_flag(s_retry_button,LV_OBJ_FLAG_HIDDEN)&&lv_obj_get_height(s_retry_button)>=48);
  lv_obj_update_layout(s_root);inside(selector_label,s_camera_selector);
  assert(lv_obj_get_height(selector_label)<=lv_obj_get_content_height(s_camera_selector));
  if(density==1&&large)capture(theme,"page");
  camera_set_viewport(true);assert(lv_obj_has_flag(s_retry_button,LV_OBJ_FLAG_HIDDEN));
  camera_set_viewport(false);assert(!lv_obj_has_flag(s_retry_button,LV_OBJ_FLAG_HIDDEN));
  camera_set_status("Connecting to camera stream...");assert(lv_obj_has_flag(s_retry_button,LV_OBJ_FLAG_HIDDEN));
  camera_view_open_cb(NULL);camera_picker_open_cb(NULL);assert(s_view_popup && s_camera_picker_popup);
  unsigned configure_before=configurations;click(s_configure_button);
  assert(configurations==configure_before+1 && !s_view_popup && !s_camera_picker_popup);
  camera_view_open_cb(NULL);lv_obj_add_event_cb(s_view_popup,hide_camera_on_dialog_delete,LV_EVENT_DELETE,NULL);
  configure_before=configurations;click(s_configure_button);
  assert(configurations==configure_before && lv_obj_has_flag(s_root,LV_OBJ_FLAG_HIDDEN));ui_camera_show();
  unsigned before=view_saves,defaults=default_saves,configs=configurations,starts=stream_starts,stops=stream_stops;
  lv_obj_t *buttons[]={s_view_button,s_camera_selector,s_configure_button,s_fullscreen_button,s_retry_button};
  for(unsigned i=0;i<5;i++){lv_obj_add_state(buttons[i],LV_STATE_DISABLED);click(buttons[i]);lv_obj_remove_state(buttons[i],LV_STATE_DISABLED);}
  assert(!s_view_popup&&!s_camera_picker_popup&&!s_fullscreen&&configurations==configs&&stream_starts==starts&&stream_stops==stops);
  lv_obj_t *fake=lv_button_create(s_root);lv_event_cb_t callbacks[]={camera_view_open_cb,camera_picker_open_cb,camera_configure_cb,camera_fullscreen_cb,camera_retry_cb};
  for(unsigned i=0;i<5;i++){lv_obj_add_event_cb(fake,callbacks[i],LV_EVENT_CLICKED,NULL);click(fake);lv_obj_remove_event_cb(fake,callbacks[i]);}
  assert(!s_view_popup&&!s_camera_picker_popup&&!s_fullscreen&&configurations==configs&&stream_starts==starts&&stream_stops==stops);lv_obj_delete(fake);
  tap(input,s_view_button);assert(s_view_popup);lv_obj_t *review=s_view_popup;
  lv_obj_add_state(s_view_actions[1],LV_STATE_DISABLED);click(s_view_actions[1]);assert(view_saves==before);lv_obj_remove_state(s_view_actions[1],LV_STATE_DISABLED);
  fake=lv_button_create(ui_form_body(review));lv_obj_add_event_cb(fake,camera_view_set_cb,LV_EVENT_CLICKED,(void*)1);lv_obj_add_flag(fake,LV_OBJ_FLAG_EVENT_BUBBLE);click(fake);assert(view_saves==before);lv_obj_delete(fake);
  lv_obj_add_state(s_view_close,LV_STATE_DISABLED);click(s_view_close);assert(s_view_popup==review);lv_obj_remove_state(s_view_close,LV_STATE_DISABLED);
  lv_obj_delete(s_view_actions[2]);assert(!s_view_actions[2]);lv_obj_delete(s_view_feedback);assert(!s_view_feedback);save_ok=false;click(s_view_actions[1]);assert(view_saves==before+1&&s_view_popup==review);save_ok=true;
  save_hook=nested_page_actions;click(s_view_actions[1]);save_hook=NULL;assert(view_saves==before+2&&!s_view_popup&&!s_fullscreen&&configurations==configs);before=view_saves;
  camera_view_open_cb(NULL);catalog_hook=retire_view_read;click(s_view_actions[0]);assert(!s_view_popup&&view_saves==before);
  catalog_hook=reopen_page;camera_view_open_cb(NULL);assert(!s_view_popup&&s_root&&s_image);
  camera_view_open_cb(NULL);catalog_hook=reopen_page;click(s_view_actions[0]);assert(!s_view_popup&&s_root&&view_saves==before);
  tap(input,s_camera_selector);assert(s_camera_picker_popup);review=s_camera_picker_popup;
  lv_obj_add_state(s_picker_actions[1],LV_STATE_DISABLED);click(s_picker_actions[1]);assert(default_saves==defaults);lv_obj_remove_state(s_picker_actions[1],LV_STATE_DISABLED);
  fake=lv_button_create(ui_form_body(review));lv_obj_add_event_cb(fake,camera_picker_select_cb,LV_EVENT_CLICKED,(void*)1);click(fake);assert(default_saves==defaults);lv_obj_delete(fake);
  lv_obj_add_state(s_picker_close,LV_STATE_DISABLED);click(s_picker_close);assert(s_camera_picker_popup==review);lv_obj_remove_state(s_picker_close,LV_STATE_DISABLED);
  lv_obj_delete(s_picker_actions[3]);assert(!s_picker_actions[3]);lv_obj_delete(s_picker_feedback);assert(!s_picker_feedback);save_ok=false;click(s_picker_actions[1]);assert(default_saves==defaults+1&&s_camera_picker_popup==review);save_ok=true;
  save_hook=nested_page_actions;click(s_picker_actions[1]);save_hook=NULL;assert(default_saves==defaults+2&&!s_camera_picker_popup&&!s_fullscreen&&configurations==configs);defaults=default_saves;
  camera_picker_open_cb(NULL);catalog_hook=retire_picker_read;click(s_picker_actions[0]);assert(!s_camera_picker_popup&&default_saves==defaults);
  default_hook=retire_picker_read;camera_picker_open_cb(NULL);assert(!s_camera_picker_popup);
  catalog_hook=retire_picker_read;camera_picker_open_cb(NULL);assert(!s_camera_picker_popup);
  default_hook=reopen_page;camera_picker_open_cb(NULL);assert(!s_camera_picker_popup&&s_root);
  camera_view_open_cb(NULL);lv_obj_add_event_cb(s_view_popup,reopen_view_deleted,LV_EVENT_DELETE,NULL);click(s_view_close);assert(s_view_popup&&s_view_close&&s_view_actions[0]&&s_view_feedback);camera_view_close_cb(NULL);
  camera_picker_open_cb(NULL);lv_obj_add_event_cb(s_camera_picker_popup,reopen_picker_deleted,LV_EVENT_DELETE,NULL);click(s_picker_close);assert(s_camera_picker_popup&&s_picker_close&&s_picker_actions[0]&&s_picker_feedback);camera_picker_close_cb(NULL);
  camera_view_open_cb(NULL);lv_obj_delete(s_view_close);assert(!s_view_close);camera_view_close_cb(NULL);camera_picker_open_cb(NULL);lv_obj_delete(s_picker_close);assert(!s_picker_close);camera_picker_close_cb(NULL);
  configure_hook=nested_page_actions;click(s_configure_button);assert(configurations==configs+1&&!s_fullscreen);configs=configurations;
  camera_view_open_cb(NULL);stop_hook=reopen_page;click(s_view_actions[0]);assert(!s_view_popup&&s_root&&s_image&&!s_camera_dialog_dispatching);
  stop_hook=reopen_page;click(s_retry_button);assert(s_root&&s_image&&!s_camera_dialog_dispatching);
  lv_obj_delete(s_selector_label);assert(!s_selector_label);camera_update_selector();
  lv_obj_t *replacement_label=ui_button_create_label(s_camera_selector,"");camera_update_selector();
  assert(s_selector_label==replacement_label && strstr(s_selector_text,entry.name));
  catalog_hook=delete_selector;camera_update_selector();assert(!s_camera_selector);camera_update_selector();
  lv_obj_t **fields[]={&s_status,&s_fullscreen_button,&s_configure_button,&s_view_button,&s_retry_button};
  for(unsigned i=0;i<5;i++){lv_obj_delete(*fields[i]);assert(!*fields[i]);}
  camera_set_status("Camera unavailable");camera_set_viewport(true);assert(s_fullscreen);lv_obj_add_state(s_image,LV_STATE_DISABLED);click(s_image);assert(s_fullscreen);lv_obj_remove_state(s_image,LV_STATE_DISABLED);tap(input,s_image);assert(!s_fullscreen&&lv_obj_get_parent(s_card)==s_root);
  camera_close_popups();ui_camera_destroy();assert(!s_view_close&&!s_picker_close);for(unsigned i=0;i<7;i++)assert(!s_view_actions[i]);for(unsigned i=0;i<CAMERA_CATALOG_MAX_CAMERAS;i++)assert(!s_picker_actions[i]);
 }
 lv_indev_delete(input);
}

static void retry_during_poll(void){camera_retry_cb(NULL);}
static void view_during_poll(void){click(s_view_actions[1]);}
static void nested_poll(void){camera_poll_cb(NULL);}
static void suspend_poll(void){ui_camera_set_setup_active(true);}
static void release_transform_frame(void){camera_release_frame();}
static void delete_poll_image(void){lv_obj_delete(s_image);}
static void reopen_and_frame(void){reopen_page();offer(3,2,12);camera_poll_cb(NULL);}
static void poll_checks(lv_display_t *display)
{
 lv_display_set_resolution(display,1024,600);
 for(unsigned theme=0;theme<5;theme++)for(unsigned density=0;density<3;density++)for(unsigned large=0;large<2;large++){
  ui_theme_set_active(theme);ui_theme_set_density(density);ui_theme_set_accessibility((ui_accessibility_t){.large_text=large});s_setup_active=false;entry.configured=true;strcpy(entry.stream_url,"http://camera-fixture.local/stream");ui_camera_show();
  unsigned before=takes;lv_timer_t *other=lv_timer_create(camera_poll_cb,60000,NULL);camera_poll_cb(other);assert(takes==before);lv_timer_delete(other);
  offer(3,2,12);take_hook=nested_poll;camera_poll_cb(s_refresh_timer);assert(takes==before+1&&s_frame&&!s_camera_polling);
  camera_release_frame();offer(3,2,12);take_hook=retry_during_poll;camera_poll_cb(NULL);assert(!s_frame&&!s_camera_polling);
  camera_view_open_cb(NULL);offer(3,2,12);take_hook=view_during_poll;camera_poll_cb(NULL);assert(!s_frame&&!s_view_popup&&!s_camera_polling);
  camera_release_frame();offer(3,2,12);take_hook=reopen_page;before=stream_starts;camera_poll_cb(NULL);assert(s_root&&!s_frame&&!s_camera_polling&&stream_starts==before);
  offer(3,2,12);catalog_hook=reopen_page;camera_poll_cb(NULL);assert(s_root&&!s_frame&&!s_camera_polling);
  busy_hook=reopen_page;before=stream_starts;camera_poll_cb(NULL);assert(s_root&&!s_frame&&stream_starts==before);
  catalog_hook=reopen_page;before=stream_starts;camera_poll_cb(NULL);assert(s_root&&stream_starts==before);
  entry.configured=false;default_hook=reopen_page;camera_poll_cb(NULL);assert(s_root&&!s_frame&&!s_camera_polling);entry.configured=true;
  start_hook=reopen_page;before=stream_starts;camera_poll_cb(NULL);assert(s_root&&!s_frame&&!s_camera_stream_started_tick&&stream_starts==before+1);
  config_generation++;default_hook=reopen_page;camera_poll_cb(NULL);assert(s_root&&!s_frame&&!s_camera_polling&&s_camera_generation==config_generation);
  start_ok=false;busy=false;camera_poll_cb(NULL);assert(strstr(lv_label_get_text(s_status),"Camera unavailable")&&!busy);start_ok=true;
  offer(3,2,12);take_hook=switch_save;camera_poll_cb(NULL);assert(!s_frame&&!s_camera_polling);camera_poll_cb(NULL);assert(s_camera_generation==config_generation);
  for(unsigned invalid=0;invalid<6;invalid++){
   busy=true;offer(3,2,12);if(invalid==0)next_width=0;if(invalid==1)next_height=0;if(invalid==2)next_width=-1;if(invalid==3)next_height=65536;if(invalid==4)next_size=2;if(invalid==5)next_ok=false;
   camera_poll_cb(NULL);assert(!s_frame&&!s_camera_polling&&strstr(lv_label_get_text(s_status),"Camera unavailable"));
  }
  offer(3,2,12);take_hook=ui_camera_hide;camera_poll_cb(NULL);assert(!s_frame&&!s_refresh_timer&&!s_camera_polling);ui_camera_show();
  offer(3,2,12);take_hook=suspend_poll;camera_poll_cb(NULL);assert(!s_frame&&!s_refresh_timer&&s_setup_active&&!s_camera_polling);ui_camera_set_setup_active(false);
  offer(3,2,12);camera_poll_cb(NULL);assert(s_frame);source_hook=reopen_page;offer(3,2,12);camera_poll_cb(NULL);assert(s_root&&!s_frame&&!s_camera_polling);
  offer(3,2,12);source_hook=delete_poll_image;camera_poll_cb(NULL);assert(!s_image&&!s_frame&&!s_refresh_timer&&!s_camera_polling);ui_camera_show();
  offer(3,2,12);camera_poll_cb(NULL);assert(s_frame);catalog_hook=release_transform_frame;camera_apply_view_transform();assert(!s_frame);
  offer(3,2,12);camera_poll_cb(NULL);catalog_hook=reopen_page;camera_apply_view_transform();assert(s_root&&!s_frame);
  offer(3,2,12);camera_poll_cb(NULL);assert(s_frame);s_camera_last_frame_tick=lv_tick_get()?lv_tick_get():1;lv_tick_inc(2101);source_hook=reopen_page;camera_poll_cb(NULL);assert(s_root&&!s_frame&&!s_camera_polling&&!strstr(lv_label_get_text(s_status),"Camera unavailable"));
  offer(3,2,12);camera_poll_cb(NULL);assert(s_frame);uint8_t *old=s_frame;source_hook=reopen_and_frame;camera_release_frame();assert(s_frame&&s_frame!=old&&lv_image_get_src(s_image)==&s_frame_dsc);
  ui_camera_destroy();assert(!s_frame&&!s_refresh_timer&&!s_camera_polling&&allocations==frees);
 }
}

static void show_deleted(lv_event_t *event){(void)event;ui_camera_show();}
static void resume_deleted(lv_event_t *event){(void)event;ui_camera_set_setup_active(false);}
static void resume_setup(void){ui_camera_set_setup_active(false);}
static void hide_from_read(void){ui_camera_hide();}
static void show_from_read(void){ui_camera_show();}
static void arm_catalog_reopen(void){catalog_hook=reopen_page;}
static void handoff_checks(lv_display_t *display)
{
 lv_display_set_resolution(display,1024,600);
 for(unsigned theme=0;theme<5;theme++)for(unsigned density=0;density<3;density++)for(unsigned large=0;large<2;large++){
  ui_theme_set_active(theme);ui_theme_set_density(density);ui_theme_set_accessibility((ui_accessibility_t){.large_text=large});entry.configured=true;strcpy(entry.name,"Owned camera");s_setup_active=false;default_camera=0;unsigned baseline=timer_count();
  default_hook=reopen_page;ui_camera_show();assert(s_root&&s_image&&s_fullscreen_button&&s_status&&s_refresh_timer&&timer_count()==baseline+1);ui_camera_destroy();
  default_hook=arm_catalog_reopen;ui_camera_show();assert(s_root&&s_image&&s_fullscreen_button&&s_status&&s_refresh_timer&&timer_count()==baseline+1);ui_camera_destroy();
  default_hook=nested_poll;unsigned reads=takes;ui_camera_show();assert(takes==reads+1&&!s_camera_show_depth);ui_camera_destroy();
  default_hook=switch_save;ui_camera_show();assert(s_root&&s_refresh_timer&&!s_camera_show_depth);camera_poll_cb(NULL);assert(s_camera_generation==config_generation);ui_camera_destroy();
  default_hook=show_from_read;ui_camera_show();assert(s_root&&s_refresh_timer&&timer_count()==baseline+1);
  default_hook=hide_from_read;ui_camera_show();assert(lv_obj_has_flag(s_root,LV_OBJ_FLAG_HIDDEN)&&!s_refresh_timer);ui_camera_show();
  default_hook=hide_from_read;ui_camera_show_fullscreen();assert(!s_fullscreen&&!s_refresh_timer&&lv_obj_has_flag(s_root,LV_OBJ_FLAG_HIDDEN));ui_camera_show();
  camera_view_open_cb(NULL);lv_obj_add_event_cb(s_view_popup,show_deleted,LV_EVENT_DELETE,NULL);ui_camera_hide();assert(s_root&&s_refresh_timer&&!lv_obj_has_flag(s_root,LV_OBJ_FLAG_HIDDEN)&&timer_count()==baseline+1);
  stop_hook=ui_camera_show;ui_camera_hide();assert(s_root&&s_refresh_timer&&!lv_obj_has_flag(s_root,LV_OBJ_FLAG_HIDDEN)&&timer_count()==baseline+1);
  offer(3,2,12);camera_poll_cb(NULL);source_hook=reopen_and_frame;ui_camera_hide();assert(s_root&&s_frame&&s_refresh_timer&&!lv_obj_has_flag(s_root,LV_OBJ_FLAG_HIDDEN));
  camera_view_open_cb(NULL);lv_obj_add_event_cb(s_view_popup,resume_deleted,LV_EVENT_DELETE,NULL);unsigned stops=stream_stops;ui_camera_set_setup_active(true);assert(!s_setup_active&&s_refresh_timer&&stream_stops==stops);
  stop_hook=resume_setup;ui_camera_set_setup_active(true);assert(!s_setup_active&&s_refresh_timer);
  ui_camera_set_setup_active(true);assert(s_setup_active&&!s_refresh_timer&&!s_frame);ui_camera_refresh_catalog();assert(s_selector_label==lv_obj_get_child(s_camera_selector,0)&&strstr(s_selector_text,"Owned camera"));ui_camera_set_setup_active(false);assert(s_refresh_timer);
  ui_camera_hide();ui_camera_refresh_catalog();assert(lv_obj_has_flag(s_root,LV_OBJ_FLAG_HIDDEN)&&!s_refresh_timer);ui_camera_show();
  catalog_hook=reopen_page;ui_camera_refresh_catalog();assert(s_root&&s_image&&s_refresh_timer&&timer_count()==baseline+1);
  catalog_hook=delete_selector;ui_camera_refresh_catalog();assert(!s_camera_selector);ui_camera_refresh_catalog();ui_camera_destroy();
  default_camera=2;missing_camera=2;ui_camera_show();assert(s_camera_index==0&&s_refresh_timer);missing_camera=-1;default_camera=0;ui_camera_destroy();assert(timer_count()==baseline&&allocations==frees);
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
   camera_set_status("Live | 10 FPS");lv_refr_now(d);invalidations=0;for(int i=0;i<100;i++){camera_apply_view_transform();camera_set_status("Live | 10 FPS");}assert(!invalidations);
  }
  entry.rotation=0;entry.mirror_horizontal=entry.mirror_vertical=false;busy=true;offer(640,480,640*480*2);camera_poll_cb(NULL);int before=lv_image_get_scale(s_image);
  camera_set_viewport(true);assert(lv_image_get_scale(s_image)==320 && lv_image_get_scale(s_image)!=before);assert(lv_image_get_rotation(s_image)==0);
  camera_set_viewport(false);assert(lv_image_get_scale(s_image)==((ui_theme_is_studio()?424:UI_PAGE_ROOT_HEIGHT)*256/480));
  offer(10,10,2);camera_poll_cb(NULL);assert(!s_frame && !lv_image_get_src(s_image));
  lv_refr_now(d);invalidations=0;for(int i=0;i<100;i++)camera_mark_unavailable();assert(!invalidations);
  assert(!lv_obj_has_flag(s_retry_button,LV_OBJ_FLAG_HIDDEN));camera_retry_cb(NULL);assert(lv_obj_has_flag(s_retry_button,LV_OBJ_FLAG_HIDDEN)&&busy);
  offer(3,2,12);camera_poll_cb(NULL);assert(s_frame&&strstr(lv_label_get_text(s_status),"Live"));offer(3,2,12);config_generation++;camera_poll_cb(NULL);assert(!s_frame&&!pending&&s_camera_generation==config_generation);
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
 control_checks(d);
 poll_checks(d);
 handoff_checks(d);
 dash32_camera_image=lv_image_create(lv_screen_active());dash32_camera_frame=heap_caps_malloc(12,0);
 dash32_camera_dsc.header.magic=LV_IMAGE_HEADER_MAGIC;dash32_camera_dsc.header.cf=LV_COLOR_FORMAT_RGB565;dash32_camera_dsc.header.w=3;dash32_camera_dsc.header.h=2;dash32_camera_dsc.header.stride=6;dash32_camera_dsc.data=dash32_camera_frame;dash32_camera_dsc.data_size=12;
 lv_image_set_src(dash32_camera_image,&dash32_camera_dsc);dashboard_camera_release_frame();assert(!dash32_camera_frame && !lv_image_get_src(dash32_camera_image));lv_obj_delete(dash32_camera_image);dash32_camera_image=NULL;
 assert(allocations==frees);lv_display_delete(d);lv_deinit();puts("PASS: camera frame retirement before free, repeated status/transform silence, all rotations/mirrors, immediate fullscreen fit, truncated-frame rejection and hide/destroy ownership across five themes/text sizes plus Dashboard source retirement and native camera dialogs/retry/ownership plus timer-balanced root/fullscreen/surface retirement and setup suspension plus owned page/dialog controls, catalog-read fences, native launch/exit and deleted-field tolerance plus scoped nonreentrant polls, rejected stale frames, replacement-safe retirement and interrupted transforms plus owner-bound show/hide/setup/catalog handoffs");
}
