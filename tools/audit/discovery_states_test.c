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
static unsigned closed, selected;
static lv_point_t touch;static bool pressed;
static void read_touch(lv_indev_t*i,lv_indev_data_t*d){(void)i;d->point=touch;d->state=pressed?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED;}
static void tap(lv_indev_t*i,lv_obj_t*b){lv_obj_update_layout(b);lv_area_t a;lv_obj_get_coords(b,&a);touch=(lv_point_t){(a.x1+a.x2)/2,(a.y1+a.y2)/2};pressed=true;lv_indev_read(i);pressed=false;lv_indev_read(i);}
bool bsp_display_lock(unsigned n){(void)n;return true;}
void bsp_display_unlock(void){}
int xTaskCreate(void(*f)(void*),const char*n,unsigned z,void*a,unsigned pr,void*t){(void)n;(void)z;(void)a;(void)pr;if(fail_task)return 0;*(TaskHandle_t*)t=(void*)1;task_fn=f;return pdPASS;}
size_t moonraker_probe_open_ports(const char*h,const int*p,size_t n,int ms,bool*out,size_t cap){(void)h;(void)p;(void)ms;assert(in_worker&&cap>=n);memset(out,0,cap*sizeof(*out));return 0;}
bool moonraker_probe_endpoint(const char*h,int p,moonraker_probe_result_t*r){(void)h;(void)p;(void)r;return false;}
static void on_close(void){closed++;}
static void on_select(const char*h,int p,const char*i){assert(h[0]&&p==7125&&i[0]);selected++;}
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
  moonraker_discovery_show("Retry after cancellation",on_close,on_select);unsigned before=closed;tap(pointer,lv_obj_get_child(lv_obj_get_child(s_popup,-1),1));assert(closed==before+1&&!s_popup);
 }
 (void)saved;lv_indev_delete(pointer);lv_display_delete(d);lv_deinit();puts("PASS: real Moonraker discovery empty/retry, native footer bounds, row selection, external deletion/cancellation and reopen across five themes/text sizes/short viewports");return 0;}
#endif
