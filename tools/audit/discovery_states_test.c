#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lvgl.h"
#include "ui_theme.h"
#include "freertos/task.h"
static bool fail_task, in_worker;
static void (*task_fn)(void *);
static unsigned saved;
bool custom_theme_color_override(uint32_t a,uint32_t b,uint32_t c,uint32_t *d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_metric_override(int32_t a,int32_t b,int32_t c,int32_t *d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_accent_override(uint8_t a,uint32_t *b){(void)a;(void)b;return false;}
bool custom_theme_surface_opacity(uint8_t *a){(void)a;return false;}
size_t strlcpy(char *d,const char *s,size_t n){size_t z=strlen(s);if(n)snprintf(d,n,"%s",s);return z;}
void vTaskDelay(unsigned n){(void)n;}
void vTaskDelete(void *p){(void)p;assert(in_worker);}
#if defined(TEST_WEBCAM)
#include "camera_discovery_controller.c"
static uint32_t generation=1;
static int active_profile, expected_profile;
static size_t probe_count=2;
uint32_t moonraker_config_generation(void){return generation;}
int moonraker_config_active_profile_index(void){return active_profile;}
bool camera_catalog_set(int p,size_t i,const char *n,const char *u){assert(!in_worker&&p==expected_profile&&i<probe_count&&n[0]&&u[0]);saved++;if(p==active_profile&&i==0)generation++;return true;}
size_t moonraker_probe_webcams_with_api_key(const char *h,int p,const char *k,moonraker_webcam_t *out,size_t cap){assert(in_worker&&!strcmp(h,"printer.local")&&p==7125&&!strcmp(k,"key")&&cap>=probe_count);for(size_t i=0;i<probe_count;i++){snprintf(out[i].name,sizeof(out[i].name),"Camera %zu",i);strcpy(out[i].stream_url,"http://fixture/stream");}return probe_count;}
int xTaskCreatePinnedToCoreWithCaps(void(*f)(void*),const char*n,unsigned z,void*a,unsigned pr,TaskHandle_t*t,int c,unsigned caps){(void)n;(void)z;(void)a;(void)pr;(void)c;(void)caps;if(fail_task)return 0;*t=(void*)1;task_fn=f;return pdPASS;}
static void worker(void){in_worker=true;task_fn(NULL);in_worker=false;}
int main(void){moonraker_webcam_t w;bool found;size_t count;
 assert(camera_discovery_start("printer.local",7125,"key"));assert(camera_discovery_busy());worker();assert(!saved&&!camera_discovery_busy());generation++;active_profile=expected_profile=1;assert(!camera_discovery_take_result(&w,&found,&count)&&!saved);
 assert(camera_discovery_start("printer.local",7125,"key"));worker();assert(camera_discovery_take_result(&w,&found,&count)&&found&&count==2&&saved==2);assert(!camera_discovery_take_result(&w,&found,&count));
 expected_profile=2;assert(camera_discovery_start_for_profile(2,"printer.local",7125,"key"));worker();assert(camera_discovery_take_result(&w,&found,&count)&&saved==4&&s_profile==2);assert(active_profile==1);expected_profile=active_profile;
 probe_count=0;assert(camera_discovery_start("printer.local",7125,"key"));worker();assert(camera_discovery_take_result(&w,&found,&count)&&!found&&!count&&!w.name[0]&&saved==4);
 fail_task=true;assert(!camera_discovery_start("printer.local",7125,"key")&&!camera_discovery_busy());fail_task=false;assert(camera_discovery_start("printer.local",7125,"key"));worker();assert(camera_discovery_take_result(NULL,NULL,NULL));
 puts("PASS: real camera discovery worker captures printer identity; stale results discarded; persistence only during current-owner consumption; empty/start-failure/retry states");return 0;}
#else
#include "moonraker_discovery.c"
static uint32_t printer_generation=1;
uint32_t moonraker_config_generation(void){return printer_generation;}
static unsigned closed, selected;
static void (*select_hook)(void),(*close_hook)(void),(*lock_hook)(void);
static unsigned hook_lock_size;
static bool probe_found;
static lv_point_t touch;static bool pressed;
static void read_touch(lv_indev_t*i,lv_indev_data_t*d){(void)i;d->point=touch;d->state=pressed?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED;}
static void tap(lv_indev_t*i,lv_obj_t*b){lv_obj_update_layout(b);lv_area_t a;lv_obj_get_coords(b,&a);touch=(lv_point_t){(a.x1+a.x2)/2,(a.y1+a.y2)/2};pressed=true;lv_indev_read(i);pressed=false;lv_indev_read(i);}
bool bsp_display_lock(unsigned n){if(lock_hook&&n==hook_lock_size){void(*hook)(void)=lock_hook;lock_hook=NULL;hook();}return true;}
void bsp_display_unlock(void){}
int xTaskCreate(void(*f)(void*),const char*n,unsigned z,void*a,unsigned pr,void*t){(void)n;(void)z;(void)a;(void)pr;if(fail_task)return 0;*(TaskHandle_t*)t=(void*)1;task_fn=f;return pdPASS;}
size_t moonraker_probe_open_ports(const char*h,const int*p,size_t n,int ms,bool*out,size_t cap){(void)h;(void)p;(void)ms;assert(in_worker&&cap>=n);memset(out,0,cap*sizeof(*out));if(probe_found&&n){out[0]=true;return 1;}return 0;}
bool moonraker_probe_endpoint(const char*h,int p,moonraker_probe_result_t*r){(void)h;if(probe_found&&p==7125){r->klippy_ready=true;strcpy(r->identity,"Verified fixture");return true;}return false;}
static void on_close(void){closed++;if(close_hook)close_hook();}
static void on_select(const char*h,int p,const char*i){assert(h[0]&&p==7125&&i[0]);selected++;if(select_hook)select_hook();assert(h[0]&&p==7125&&i[0]);}
static void wrong_close(void){assert(!"Repeated Show replaced Close");}
static void wrong_select(const char*h,int p,const char*i){(void)h;(void)p;(void)i;assert(!"Repeated Show replaced selection");}
static void click(lv_obj_t *object){lv_obj_send_event(object,LV_EVENT_CLICKED,NULL);}
static void nested_select_close(void){
 click(lv_obj_get_child(s_results_list,0));discovery_retry_cb(NULL);moonraker_discovery_close();
}
static void reopen_close(void){moonraker_discovery_show("Replacement",on_close,on_select);moonraker_discovery_close();}
static void reopen_deleted(lv_event_t *event){(void)event;moonraker_discovery_show("Replacement",on_close,on_select);}
static void replace_locked(void){moonraker_discovery_close();moonraker_discovery_show("Keep new scan",on_close,on_select);}
static void delete_locked_status(void){lv_obj_delete(s_status_label);}
static void worker(void){in_worker=true;task_fn(NULL);in_worker=false;}
static void safety_checks(const esp_ip4_addr_t *ip)
{
 moonraker_discovery_show("Initial",on_close,on_select);
 moonraker_discovery_show("Do not reset",wrong_close,wrong_select);assert(!strcmp(s_status,"Initial"));
 moonraker_probe_result_t probe={.klippy_ready=true};strcpy(probe.identity,"Safe identity");
 assert(moonraker_discovery_add_candidate("192.0.2.126",7125,&probe));
 unsigned before=selected;lv_obj_t *row=lv_obj_get_child(s_results_list,0);
 lv_obj_add_state(row,LV_STATE_DISABLED);click(row);assert(selected==before);lv_obj_remove_state(row,LV_STATE_DISABLED);
 select_hook=nested_select_close;click(row);select_hook=NULL;assert(selected==before+1&&!s_popup);
 unsigned closes=closed;moonraker_discovery_close();assert(closed==closes);
 moonraker_discovery_show("Controls",on_close,on_select);
 lv_obj_t *fake=lv_button_create(s_popup);lv_obj_add_event_cb(fake,discovery_retry_cb,LV_EVENT_CLICKED,NULL);
 lv_obj_add_event_cb(fake,close_event_cb,LV_EVENT_CLICKED,NULL);click(fake);assert(s_popup&&!s_scan_task_handle);lv_obj_delete(fake);
 lv_obj_add_state(s_retry_action,LV_STATE_DISABLED);discovery_retry_cb(NULL);assert(!s_scan_task_handle);
 lv_obj_delete(s_retry_action);assert(!s_retry_action);discovery_retry_cb(NULL);assert(!s_scan_task_handle);
 lv_obj_delete(s_close_action);assert(!s_close_action);close_event_cb(NULL);assert(s_popup);moonraker_discovery_close();
 moonraker_discovery_show("Status",on_close,on_select);hook_lock_size=50;lock_hook=delete_locked_status;
 moonraker_discovery_set_status("No stale label");assert(!s_status_label&&!strcmp(s_status,"No stale label"));
 moonraker_discovery_close();moonraker_discovery_show("Old status owner",on_close,on_select);
 hook_lock_size=50;lock_hook=replace_locked;moonraker_discovery_set_status("Discard old status");
 assert(s_status_label&&!strcmp(s_status,"Keep new scan"));
 moonraker_discovery_close();moonraker_discovery_show("Old scan",on_close,on_select);
 assert(moonraker_discovery_start(ip));moonraker_discovery_close();moonraker_discovery_show("Keep new scan",on_close,on_select);
 worker();assert(!s_scan_task_handle&&!strcmp(s_status,"Keep new scan")&&!s_candidate_count);
 assert(moonraker_discovery_start(ip));hook_lock_size=100;lock_hook=replace_locked;probe_found=true;
 worker();probe_found=false;assert(!lock_hook&&!s_scan_task_handle&&!s_candidate_count&&!strcmp(s_status,"Keep new scan"));
 assert(moonraker_discovery_add_candidate("192.0.2.126",7125,&probe));row=lv_obj_get_child(s_results_list,0);
 before=selected;printer_generation++;click(row);discovery_retry_cb(NULL);
 assert(selected==before&&!s_scan_task_handle&&strstr(s_status,"Printer changed"));moonraker_discovery_close();
 moonraker_discovery_show("Delete list",on_close,on_select);assert(moonraker_discovery_start(ip));
 lv_obj_delete(s_results_list);assert(!s_results_list&&s_cancelled);worker();assert(!s_scan_task_handle);moonraker_discovery_close();
 moonraker_discovery_show("Close replacement",on_close,on_select);close_hook=reopen_close;
 moonraker_discovery_close();close_hook=NULL;assert(s_popup&&s_status_label&&s_results_list&&!strcmp(s_status,"Replacement"));
 moonraker_discovery_close();moonraker_discovery_show("Delete replacement",on_close,on_select);
 lv_obj_add_event_cb(s_popup,reopen_deleted,LV_EVENT_DELETE,NULL);moonraker_discovery_close();
 assert(s_popup&&s_close_cb==on_close&&s_select_cb==on_select&&!strcmp(s_status,"Replacement"));
 moonraker_discovery_close();closes=closed;assert(!moonraker_discovery_start(ip));moonraker_discovery_close();assert(closed==closes);
}
static uint16_t raster[1024*600];
static void flush(lv_display_t*d,const lv_area_t*a,uint8_t*p){int w=lv_display_get_horizontal_resolution(d);uint16_t*v=(uint16_t*)p;for(int y=a->y1;y<=a->y2;y++)for(int x=a->x1;x<=a->x2;x++)raster[y*w+x]=*v++;lv_display_flush_ready(d);}
static void snapshot(void){const char*path=getenv("DISCOVERY_SCREENSHOT");if(!path)return;lv_refr_now(NULL);FILE*f=fopen(path,"wb");assert(f);fprintf(f,"P6\n1024 600\n255\n");for(unsigned i=0;i<1024*600;i++){uint16_t v=raster[i];unsigned char c[3]={(v>>11)*255/31,((v>>5)&63)*255/63,(v&31)*255/31};fwrite(c,1,3,f);}fclose(f);}
static void inside(lv_obj_t*a,lv_obj_t*b){lv_area_t x,y;lv_obj_get_coords(a,&x);lv_obj_get_coords(b,&y);assert(x.x1>=y.x1&&x.y1>=y.y1&&x.x2<=y.x2&&x.y2<=y.y2);}
int main(void){lv_init();lv_display_t*d=lv_display_create(1024,600);static uint8_t pixels[1024*40*2];lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);lv_display_set_buffers(d,pixels,NULL,sizeof(pixels),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(d,flush);esp_ip4_addr_t ip={.addr=0x640144c0};
 lv_indev_t*pointer=lv_indev_create();lv_indev_set_type(pointer,LV_INDEV_TYPE_POINTER);lv_indev_set_read_cb(pointer,read_touch);
 for(unsigned theme=0;theme<5;theme++)for(unsigned density=0;density<3;density++)for(unsigned large=0;large<2;large++)for(unsigned size=0;size<3;size++){
  lv_display_set_resolution(d,(unsigned[]){1024,640,480}[size],(unsigned[]){600,480,320}[size]);ui_theme_set_active(theme);ui_theme_set_density(density);ui_theme_set_accessibility((ui_accessibility_t){.large_text=large});
  lv_obj_t*page=lv_obj_create(lv_screen_active());lv_obj_set_size(page,lv_display_get_horizontal_resolution(d)-16,lv_display_get_vertical_resolution(d)-16);lv_obj_set_pos(page,8,8);lv_obj_clear_flag(page,LV_OBJ_FLAG_SCROLLABLE);
  moonraker_discovery_show_in_parent(page,"Searching this network for printers...",on_close,on_select);assert(s_popup);lv_obj_update_layout(s_popup);inside(s_popup,lv_screen_active());lv_obj_t*footer=lv_obj_get_child(s_popup,-1);inside(footer,s_popup);for(unsigned i=0;i<2;i++)inside(lv_obj_get_child(footer,i),footer);
  fail_task=true;assert(!moonraker_discovery_start(&ip)&&strstr(s_status,"Could not start"));fail_task=false;tap(pointer,lv_obj_get_child(footer,0));assert(s_scan_task_handle);discovery_retry_cb(NULL);assert(strstr(s_status,"still finishing"));in_worker=true;task_fn(NULL);in_worker=false;assert(!s_scan_task_handle&&strstr(s_status,"No Moonraker"));if(theme==4&&density==1&&large&&!size)snapshot();
  moonraker_probe_result_t probe={.klippy_ready=true};strcpy(probe.identity,"Workshop printer with a long descriptive name");assert(moonraker_discovery_add_candidate("192.0.2.126",7125,&probe));lv_obj_update_layout(s_popup);lv_obj_t*row=lv_obj_get_child(s_results_list,0);lv_obj_scroll_to_view_recursive(row,LV_ANIM_OFF);lv_obj_update_layout(s_popup);unsigned selections=selected;tap(pointer,row);assert(selected==selections+1);
  discovery_retry_cb(NULL);assert(!s_candidate_count&&!lv_obj_get_child_count(s_results_list));lv_obj_delete(page);assert(s_cancelled&&!s_popup&&!s_status_label&&!s_results_list);in_worker=true;task_fn(NULL);in_worker=false;assert(!s_scan_task_handle);
  moonraker_discovery_show("Retry after cancellation",on_close,on_select);unsigned before=closed;tap(pointer,lv_obj_get_child(lv_obj_get_child(s_popup,-1),1));assert(closed==before+1&&!s_popup);safety_checks(&ip);
 }
 (void)saved;lv_indev_delete(pointer);lv_display_delete(d);lv_deinit();puts("PASS: Moonraker discovery retry/select/close ownership, captured endpoint/callbacks, deleted fields, printer changes, late-worker/under-lock publication fences and replacement teardown across all themes/densities/text/viewports");return 0;}
#endif
