/* Real LVGL Setup Center; transport and persistence are typed fixtures. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
size_t strlcpy(char*,const char*,size_t);
#include "ui_setup_wizard.c"
#include "custom_theme.h"
#include "ui_page_layout_profile.h"
bool custom_theme_color_override(uint32_t a,uint32_t b,uint32_t c,uint32_t*d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_metric_override(int32_t a,int32_t b,int32_t c,int32_t*d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_accent_override(uint8_t a,uint32_t*b){(void)a;(void)b;return false;}
bool custom_theme_surface_opacity(uint8_t*a){(void)a;return false;}
const ui_page_layout_profile_t *custom_theme_page_profile(void){return NULL;}
size_t strlcpy(char*d,const char*s,size_t n){size_t z=strlen(s);if(n){size_t k=z<n-1?z:n-1;memcpy(d,s,k);d[k]=0;}return z;}
static bool wifi_ready,configured,camera_ready,storage_ok=true;
static bool camera_result,discovery_result;
static unsigned editor_opened,connections,completions;
static void (*scan_task)(void*);
static moonraker_profile_t profile={.host="printer.local",.port=7125};
const moonraker_profile_t *moonraker_config_profile(int i){(void)i;profile.configured=configured;return &profile;}
int moonraker_config_active_profile_index(void){return 0;}
const char *moonraker_config_camera_stream_url(int i){(void)i;return camera_ready?"http://camera.local/stream":"";}
bool moonraker_config_set_camera_stream_url(int i,const char*u){(void)i;assert(u[0]);camera_ready=true;return true;}
bool onboarding_controller_mark_complete(void){if(storage_ok)completions++;return storage_ok;}
void ui_printer_profiles_show_for_slot(int slot,void(*changed)(void),void(*discover)(void)){assert(slot==0&&changed&&discover);editor_opened++;}
void ui_printer_profiles_set_discovered_endpoint(const char*h,int p,const char*i){(void)h;(void)p;(void)i;}
void moonraker_discovery_show_in_parent(lv_obj_t*p,const char*t,moonraker_discovery_close_cb_t c,moonraker_discovery_select_cb_t s){(void)p;(void)t;(void)c;(void)s;}
bool moonraker_discovery_start(const esp_ip4_addr_t *ip){(void)ip;return true;}
esp_netif_t *esp_netif_get_handle_from_ifkey(const char*s){(void)s;return &profile;}
esp_err_t esp_netif_get_ip_info(esp_netif_t*n,esp_netif_ip_info_t*i){(void)n;i->ip.addr=1;return ESP_OK;}
esp_err_t esp_wifi_sta_get_ap_info(wifi_ap_record_t*i){memset(i,0,sizeof(*i));return wifi_ready?ESP_OK:ESP_FAIL;}
esp_err_t esp_wifi_scan_start(void*p,bool b){(void)p;(void)b;return ESP_OK;}
esp_err_t esp_wifi_scan_get_ap_num(uint16_t*n){*n=8;return ESP_OK;}
esp_err_t esp_wifi_scan_get_ap_records(uint16_t*n,wifi_ap_record_t*a){for(unsigned i=0;i<*n;i++){snprintf((char*)a[i].ssid,sizeof(a[i].ssid),"Workshop_long_network_name_%u",i);a[i].rssi=-57-(int)i;}return ESP_OK;}
void vTaskDelay(unsigned n){(void)n;}
void vTaskDelete(void*p){(void)p;}
int xTaskCreate(void(*f)(void*),const char*n,unsigned z,void*p,unsigned pr,void*h){(void)n;(void)z;(void)p;(void)pr;(void)h;scan_task=f;return pdPASS;}
bool camera_catalog_get(int p,size_t i,camera_catalog_entry_t*e){(void)p;*e=(camera_catalog_entry_t){.configured=true};snprintf(e->name,sizeof(e->name),"Workshop camera %zu",i);strlcpy(e->stream_url,"http://camera.local/stream",sizeof(e->stream_url));return true;}
bool camera_catalog_set_default(int p,size_t i){(void)p;assert(i<4);return true;}
bool camera_discovery_start(const char*h,int p,const char*k){(void)h;(void)p;(void)k;return true;}
bool camera_discovery_busy(void){return false;}
bool camera_discovery_take_result(moonraker_webcam_t*w,bool*f,size_t*n){(void)w;*f=discovery_result;*n=discovery_result?4:0;return true;}
bool camera_test_start(const char*u){assert(u[0]);return true;}
bool camera_test_take_result(bool*o,int*w,int*h,size_t*b){*o=camera_result;*w=900;*h=520;*b=4096;return true;}
static bool connect_wifi(const char*s,const char*p){assert(s[0]);assert(!strcmp(p,"test-password"));connections++;return true;}
static void inside(lv_obj_t*c,lv_obj_t*p){lv_area_t a,b;lv_obj_get_coords(c,&a);lv_obj_get_coords(p,&b);if(!(a.x1>=b.x1&&a.x2<=b.x2&&a.y1>=b.y1&&a.y2<=b.y2)){fprintf(stderr,"outside %d,%d..%d,%d vs %d,%d..%d,%d\n",a.x1,a.y1,a.x2,a.y2,b.x1,b.y1,b.x2,b.y2);abort();}}
static void check(void){lv_obj_update_layout(s_center);inside(s_center,lv_screen_active());inside(s_setup_content,s_center);inside(s_stage,s_setup_content);inside(s_actions,s_setup_content);assert(lv_obj_get_height(s_stage)>0);for(unsigned i=0;i<lv_obj_get_child_count(s_actions);i++){lv_obj_t*b=lv_obj_get_child(s_actions,i);inside(b,s_actions);inside(lv_obj_get_child(b,0),b);assert(lv_obj_get_height(b)>=48);}lv_area_t a,b;lv_obj_get_coords(s_actions,&a);lv_obj_scroll_to_y(s_stage,2000,LV_ANIM_OFF);lv_obj_update_layout(s_center);lv_obj_get_coords(s_actions,&b);assert(!memcmp(&a,&b,sizeof(a)));lv_obj_scroll_to_y(s_stage,0,LV_ANIM_OFF);}
static unsigned width=1024;static uint16_t raster[1024*600];
static void flush(lv_display_t*d,const lv_area_t*a,uint8_t*p){uint16_t*v=(uint16_t*)p;for(int y=a->y1;y<=a->y2;y++)for(int x=a->x1;x<=a->x2;x++)raster[y*width+x]=*v++;lv_display_flush_ready(d);}
static void snapshot(unsigned theme,const char*name){const char*folder=getenv("SETUP_SCREENSHOTS");if(!folder||width!=1024)return;lv_refr_now(NULL);char path[512];snprintf(path,sizeof(path),"%s/%s-theme%u.ppm",folder,name,theme);FILE*f=fopen(path,"wb");assert(f);fprintf(f,"P6\n1024 600\n255\n");for(unsigned i=0;i<1024*600;i++){uint16_t v=raster[i];unsigned char c[3]={(v>>11)*255/31,((v>>5)&63)*255/63,(v&31)*255/31};fwrite(c,1,3,f);}fclose(f);}
int main(void){lv_init();lv_display_t*d=lv_display_create(1024,600);static uint8_t buffer[1024*50*2];lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);lv_display_set_buffers(d,buffer,NULL,sizeof(buffer),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(d,flush);
 for(unsigned t=0;t<5;t++)for(unsigned density=0;density<3;density++)for(unsigned large=0;large<2;large++)for(unsigned w=0;w<3;w++){
 width=(unsigned[]){1024,640,480}[w];lv_display_set_resolution(d,width,600);ui_theme_set_active(t);ui_theme_set_density(density);ui_theme_set_accessibility((ui_accessibility_t){.large_text=large});wifi_ready=configured=camera_ready=false;
 ui_setup_wizard_show(connect_wifi);check();lv_obj_t*owner=s_center;ui_setup_wizard_show(connect_wifi);assert(owner==s_center);if(density==1&&large)snapshot(t,"welcome");
 for(int i=1;i<5;i++){lv_obj_send_event(s_nav[i],LV_EVENT_CLICKED,NULL);assert(s_setup_step==i);check();}
 unsigned before=completions;finish_cb(NULL);assert(s_center&&completions==before);
 setup_select_printer_cb(NULL);before=editor_opened;center_printer_cb(NULL);assert(editor_opened==before+1);
 setup_select_wifi_cb(NULL);center_wifi_cb(NULL);assert(s_wifi_scan_busy&&scan_task);wifi_scan_open();scan_task(NULL);wifi_scan_ready(NULL);check();assert(lv_obj_get_child_count(s_wifi_list)==8);if(density==1&&large)snapshot(t,"networks");
 lv_obj_send_event(lv_obj_get_child(s_wifi_list,0),LV_EVENT_CLICKED,NULL);check();assert(s_password&&s_keyboard&&lv_textarea_get_password_mode(s_password));assert(lv_textarea_get_max_length(s_password)==63);lv_textarea_set_text(s_password,"test-password");before=connections;wifi_save_cb(NULL);assert(connections==before+1&&s_poll);wifi_ready=true;wifi_verify(NULL);assert(s_wifi_done);wifi_return_timer(s_poll);check();
 configured=true;setup_select_camera_cb(NULL);center_camera_cb(NULL);discovery_result=false;camera_discovery_poll(NULL);check();
 assert(!s_poll&&lv_obj_get_child_count(s_camera_list)==0&&lv_obj_get_child_count(s_actions)==2);
 lv_obj_send_event(lv_obj_get_child(s_actions,1),LV_EVENT_CLICKED,NULL);assert(s_setup_step==SETUP_STEP_COMPLETE&&!camera_ready);check();
 before=completions;finish_cb(NULL);assert(!s_center&&s_complete_popup&&completions==before+1);setup_completion_done_cb(NULL);
 ui_setup_wizard_show(connect_wifi);setup_select_camera_cb(NULL);center_camera_cb(NULL);discovery_result=true;camera_discovery_poll(NULL);check();assert(lv_obj_get_child_count(s_camera_list)==4);lv_obj_send_event(lv_obj_get_child(s_camera_list,0),LV_EVENT_CLICKED,NULL);camera_result=false;camera_test_poll(NULL);assert(s_center);lv_obj_send_event(lv_obj_get_child(s_camera_list,0),LV_EVENT_CLICKED,NULL);camera_result=true;camera_test_poll(NULL);check();camera_save_cb(NULL);assert(camera_ready);check();
 setup_select_complete_cb(NULL);check();if(density==1&&large)snapshot(t,"review");storage_ok=false;finish_cb(NULL);assert(s_center);storage_ok=true;finish_cb(NULL);assert(!s_center&&!s_setup_content&&!s_status&&!s_poll&&s_complete_popup);lv_obj_update_layout(s_complete_popup);inside(s_complete_popup,lv_screen_active());setup_completion_done_cb(NULL);
 ui_setup_wizard_show(connect_wifi);lv_obj_delete(s_center);assert(!s_center&&!s_setup_content&&!s_status&&!s_keyboard);ui_setup_wizard_show(connect_wifi);later_cb(NULL);assert(!s_center);
 }
 lv_display_delete(d);lv_deinit();puts("PASS: Setup Center layout, five themes, density/text variants, navigation, scan/password, profile routing, camera verification, completion gates and teardown");return 0;}
