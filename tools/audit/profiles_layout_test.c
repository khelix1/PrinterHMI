#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
size_t strlcpy(char*,const char*,size_t);
#include "ui_printer_profiles.c"
bool custom_theme_color_override(uint32_t a,uint32_t b,uint32_t c,uint32_t*d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_metric_override(int32_t a,int32_t b,int32_t c,int32_t*d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_accent_override(uint8_t a,uint32_t*b){(void)a;(void)b;return false;}
bool custom_theme_surface_opacity(uint8_t*a){(void)a;return false;}
static moonraker_profile_t profiles[4];static int active;
size_t strlcpy(char*d,const char*s,size_t n){size_t len=strlen(s);if(n){size_t k=len<n-1?len:n-1;memcpy(d,s,k);d[k]=0;}return len;}
__attribute__((noinline)) const moonraker_profile_t*moonraker_config_profile(int i){return i>=0&&i<4?&profiles[i]:NULL;}
int moonraker_config_active_profile_index(void){return active;}
__attribute__((noinline)) const char*moonraker_config_active_profile_name(void){return profiles[active].name;}
size_t moonraker_config_profile_count(void){size_t n=0;for(int i=0;i<4;i++)n+=profiles[i].configured;return n;}
bool moonraker_config_select_profile(int i){active=i;return true;}
__attribute__((noinline)) const char*moonraker_config_host(void){return profiles[active].host;}
int moonraker_config_port(void){return profiles[active].port;}
static int saves,deletes,changed,discovered;static bool storage_ok=true;
bool moonraker_config_save_profile(int i,const char*n,const char*h,int p,const char*k){if(!storage_ok)return false;saves++;profiles[i].configured=true;strlcpy(profiles[i].name,n,sizeof(profiles[i].name));strlcpy(profiles[i].host,h,sizeof(profiles[i].host));profiles[i].port=p;strlcpy(profiles[i].api_key,k,sizeof(profiles[i].api_key));return true;}
bool moonraker_config_delete_profile(int i){if(!storage_ok)return false;deletes++;profiles[i].configured=false;if(i==active)active=1;return true;}
bool moonraker_config_set_camera_stream_url(int i,const char*u){(void)i;(void)u;return true;}
bool moonraker_config_set_transport_security(int i,bool secure){profiles[i].secure_transport=secure;return true;}
void printer_profile_health_set(int i,bool k,bool o){(void)i;(void)k;(void)o;}
void printer_preview_cache_invalidate(int i){(void)i;}
void printer_preview_store_invalidate(int i){(void)i;}
void ui_printer_chooser_refresh(void){}
void ui_camera_set_setup_active(bool v){(void)v;}
void ui_camera_refresh_catalog(void){}
void ui_dashboard_refresh_camera(void){}
size_t camera_catalog_default(int i){(void)i;return 0;}
bool camera_catalog_get(int i,size_t s,camera_catalog_entry_t*e){(void)i;(void)s;memset(e,0,sizeof(*e));return false;}
bool camera_catalog_set(int i,size_t s,const char*n,const char*u){(void)i;(void)s;(void)n;(void)u;return true;}
bool camera_catalog_set_default(int i,size_t s){(void)i;(void)s;return true;}
bool camera_catalog_clear(int i,size_t s){(void)i;(void)s;return true;}
bool camera_test_start(const char*u){(void)u;return true;}
bool camera_test_busy(void){return false;}
bool camera_test_take_result(bool*o,int*h,int*s,size_t*b){(void)o;(void)h;(void)s;(void)b;return false;}
bool camera_discovery_start(const char*h,int p,const char*k){(void)h;(void)p;(void)k;return true;}
bool camera_discovery_busy(void){return false;}
bool camera_discovery_take_result(moonraker_webcam_t*c,bool*o,size_t*n){(void)c;(void)o;(void)n;return false;}
bool moonraker_endpoint_test_start(const char*h,int p,const char*k){(void)h;(void)p;(void)k;return true;}
bool moonraker_endpoint_test_busy(void){return false;}
bool moonraker_endpoint_test_take_result(moonraker_probe_result_t*r,bool*o){(void)r;(void)o;return false;}
bool moonraker_transport_security_import_ca_file_for_profile(int i,const char*p){(void)i;(void)p;return false;}
const char*moonraker_transport_security_ca_pem_for_profile(int i){(void)i;return NULL;}
static void active_changed(void){changed++;}
static void discover(void){discovered++;}
static void click(lv_obj_t*o){lv_obj_send_event(o,LV_EVENT_CLICKED,NULL);}
static uint16_t raster[1024*600];static int viewport=1024;
static void flush(lv_display_t*d,const lv_area_t*a,uint8_t*p){uint16_t*v=(uint16_t*)p;for(int y=a->y1;y<=a->y2;y++)for(int x=a->x1;x<=a->x2;x++)raster[y*viewport+x]=*v++;lv_display_flush_ready(d);}
static void snapshot(int theme,const char*name){const char*folder=getenv("PROFILES_SCREENSHOTS");if(!folder)return;lv_refr_now(NULL);char path[512];snprintf(path,sizeof(path),"%s/%s-theme%d.ppm",folder,name,theme);FILE*f=fopen(path,"wb");assert(f);fprintf(f,"P6\n1024 600\n255\n");for(int i=0;i<1024*600;i++){uint16_t v=raster[i];unsigned char c[3]={(v>>11)*255/31,((v>>5)&63)*255/63,(v&31)*255/31};fwrite(c,1,3,f);}fclose(f);}
static void inside(lv_obj_t*c,lv_obj_t*p){lv_area_t a,b;lv_obj_get_coords(c,&a);lv_obj_get_coords(p,&b);if(!(a.x1>=b.x1&&a.x2<=b.x2&&a.y1>=b.y1&&a.y2<=b.y2)){fprintf(stderr,"Outside %d,%d..%d,%d vs %d,%d..%d,%d\n",a.x1,a.y1,a.x2,a.y2,b.x1,b.y1,b.x2,b.y2);abort();}}
static void fit(lv_obj_t*button){lv_obj_t*l=lv_obj_get_child(button,0);inside(l,button);lv_point_t n;lv_text_get_size(&n,lv_label_get_text(l),lv_obj_get_style_text_font(l,0),lv_obj_get_style_text_letter_space(l,0),0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);assert(n.x<=lv_obj_get_width(l));assert(lv_obj_get_height(l)==lv_obj_get_style_text_font(l,0)->line_height);assert(lv_obj_get_height(button)>=48);}
static void layout(lv_obj_t*popup){lv_obj_update_layout(popup);inside(popup,lv_screen_active());lv_obj_t*body=lv_obj_get_child(popup,1),*footer=lv_obj_get_child(popup,-1);inside(body,popup);inside(footer,popup);assert(lv_obj_get_height(body)>50);for(unsigned i=0;i<lv_obj_get_child_count(footer);i++){lv_obj_t*b=lv_obj_get_child(footer,i);inside(b,footer);fit(b);}lv_area_t a,b;lv_obj_get_coords(footer,&a);lv_obj_scroll_to_y(body,2000,LV_ANIM_OFF);lv_obj_update_layout(popup);lv_obj_get_coords(footer,&b);assert(!memcmp(&a,&b,sizeof(a)));lv_obj_t*last=lv_obj_get_child(body,-1);inside(last,body);lv_obj_scroll_to_y(body,0,LV_ANIM_OFF);lv_obj_update_layout(popup);}
static void reset(void){memset(profiles,0,sizeof(profiles));profiles[0]=(moonraker_profile_t){.configured=true,.name="Workshop printer with long name",.host="workshop-printer-long-hostname.local",.port=7125};profiles[1]=(moonraker_profile_t){.configured=true,.name="Printer B",.host="printer-b.local",.port=7126};active=0;storage_ok=true;}
int main(void){lv_init();lv_display_t*d=lv_display_create(1024,600);static uint8_t buf[1024*40*2];lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);lv_display_set_buffers(d,buf,NULL,sizeof(buf),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(d,flush);
for(int size=0;size<3;size++)for(int theme=0;theme<5;theme++)for(int density=0;density<3;density++)for(int large=0;large<2;large++){
viewport=(int[]){1024,640,480}[size];lv_display_set_resolution(d,viewport,size==2?400:600);ui_theme_set_active(theme);ui_theme_set_density(density);ui_theme_set_accessibility((ui_accessibility_t){.large_text=large});reset();int old_saves=saves,old_deletes=deletes,old_changed=changed;
ui_printer_profiles_show(active_changed,discover);layout(s_manager_popup);if(ui_theme_is_studio())assert(lv_obj_get_width(s_manager_popup)==viewport-48);for(int i=0;i<4;i++){lv_obj_t*r=s_profile_rows[i];for(unsigned j=0;j<lv_obj_get_child_count(r);j++)inside(lv_obj_get_child(r,j),r);}if(!size&&density==1&&large)snapshot(theme,"manager");
click(s_profile_rows[2]);assert(s_selected_profile==2);manager_select_cb(NULL);assert(active==0&&changed==old_changed);manager_delete_cb(NULL);assert(!s_delete_popup);
click(s_profile_rows[0]);manager_edit_cb(NULL);layout(s_editor_popup);lv_obj_t*fields[]={s_editor_name,s_editor_host,s_editor_port};for(unsigned i=0;i<3;i++){assert(lv_obj_get_height(fields[i])>=56);assert(lv_obj_get_style_text_font(fields[i],0)==UI_FONT_BODY_LARGE);}assert(!strcmp(lv_textarea_get_text(s_editor_name),profiles[0].name));if(!size&&density==1&&large)snapshot(theme,"editor");
int old_discovered=discovered;editor_discover_cb(NULL);assert(discovered==old_discovered+1);lv_textarea_set_text(s_editor_host,"");editor_save_cb(NULL);assert(s_editor_popup&&saves==old_saves);lv_textarea_set_text(s_editor_host,"changed.local");lv_textarea_set_text(s_editor_port,"0");editor_save_cb(NULL);assert(s_editor_popup&&saves==old_saves);lv_textarea_set_text(s_editor_port,"7127");s_editor_secure=true;editor_save_cb(NULL);assert(s_editor_popup&&saves==old_saves);s_editor_secure=false;storage_ok=false;editor_save_cb(NULL);assert(s_editor_popup&&saves==old_saves);storage_ok=true;
ui_printer_profiles_set_discovered_endpoint("discovered.local",7128,"Moonraker");assert(!strcmp(lv_textarea_get_text(s_editor_host),"discovered.local"));assert(!strcmp(lv_textarea_get_text(s_editor_name),profiles[0].name));editor_save_cb(NULL);assert(!s_editor_popup&&saves==old_saves+1&&changed==old_changed+1);assert(!strcmp(profiles[0].host,"discovered.local")&&profiles[0].port==7128);
manager_edit_cb(NULL);lv_textarea_set_text(s_editor_host,"cancelled.local");editor_cancel_cb(NULL);assert(!s_editor_popup&&saves==old_saves+1);assert(strcmp(profiles[0].host,"cancelled.local"));
manager_delete_cb(NULL);assert(s_delete_popup);layout(s_delete_popup);if(!size&&density==1&&large)snapshot(theme,"remove");delete_cancel_cb(NULL);assert(!s_delete_popup&&deletes==old_deletes);manager_delete_cb(NULL);storage_ok=false;delete_confirm_cb(NULL);assert(!s_delete_popup&&deletes==old_deletes);storage_ok=true;manager_delete_cb(NULL);delete_confirm_cb(NULL);assert(!s_delete_popup&&deletes==old_deletes+1&&active==1&&changed==old_changed+2);manager_delete_cb(NULL);assert(!s_delete_popup);
ui_printer_profiles_close_all();assert(!s_manager_popup&&!s_manager_list&&!s_editor_popup&&!s_active_changed_cb);reset();ui_printer_profiles_show_for_slot(2,active_changed,discover);assert(s_editor_popup&&s_editor_profile==2);layout(s_editor_popup);editor_test_cb(NULL);assert(!s_editor_test_timer);lv_textarea_set_text(s_editor_host,"test.local");editor_test_cb(NULL);assert(s_editor_test_timer);lv_obj_delete(s_editor_popup);assert(!s_editor_popup&&!s_editor_test_timer&&!s_editor_host);manager_edit_cb(NULL);lv_obj_delete(s_manager_popup);assert(!s_editor_popup&&!s_manager_popup&&!s_manager_list);
}
lv_display_delete(d);lv_deinit();puts("PASS: profile manager/editor/remove layouts in five themes, three densities, both text sizes and three viewports; pinned actions, scroll reach, large fields, selection, discovery, save validation/failure/cancel, remove guards and teardown");}
