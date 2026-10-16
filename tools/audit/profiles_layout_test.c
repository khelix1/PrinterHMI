#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
size_t strlcpy(char*,const char*,size_t);
#define opendir profile_test_opendir
#define readdir profile_test_readdir
#define closedir profile_test_closedir
#define lv_textarea_set_text profile_test_textarea_set_text
#include "ui_printer_profiles.c"
#include "keyboard_layout_checks.h"
#undef opendir
#undef readdir
#undef closedir
#undef lv_textarea_set_text
void lv_textarea_set_text(lv_obj_t *object,const char *value);
static void (*textarea_write_hook)(lv_obj_t *);
void profile_test_textarea_set_text(lv_obj_t *object,const char *value)
{
 lv_textarea_set_text(object,value);
 if(textarea_write_hook){void(*hook)(lv_obj_t *)=textarea_write_hook;textarea_write_hook=NULL;hook(object);}
}
static int directory_token, directory_index;static bool directory_available=true,directory_long_names;
static unsigned directory_closes;
static void (*directory_open_hook)(void),(*directory_read_hook)(void),(*directory_close_hook)(void);
DIR *profile_test_opendir(const char *path){assert(!strcmp(path,"/sdcard"));directory_index=0;if(directory_open_hook)directory_open_hook();return directory_available?(DIR*)&directory_token:NULL;}
struct dirent *profile_test_readdir(DIR *dir){assert(dir==(DIR*)&directory_token);static struct dirent e;if(directory_read_hook){void(*hook)(void)=directory_read_hook;directory_read_hook=NULL;hook();}if(directory_index>=8)return NULL;if(directory_long_names){memset(e.d_name,'a',sizeof(e.d_name)-1);e.d_name[sizeof(e.d_name)-1]=0;strcpy(e.d_name+sizeof(e.d_name)-5,".pem");directory_index++;return &e;}snprintf(e.d_name,sizeof(e.d_name),"certificate_with_a_long_name_%d.%s",directory_index,directory_index?"pem":"txt");directory_index++;return &e;}
int profile_test_closedir(DIR *dir){assert(dir==(DIR*)&directory_token);directory_closes++;if(directory_close_hook)directory_close_hook();return 0;}
bool custom_theme_color_override(uint32_t a,uint32_t b,uint32_t c,uint32_t*d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_metric_override(int32_t a,int32_t b,int32_t c,int32_t*d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_accent_override(uint8_t a,uint32_t*b){(void)a;(void)b;return false;}
bool custom_theme_surface_opacity(uint8_t*a){(void)a;return false;}
static moonraker_profile_t profiles[4];static int active;
size_t strlcpy(char*d,const char*s,size_t n){size_t len=strlen(s);if(n){size_t k=len<n-1?len:n-1;memcpy(d,s,k);d[k]=0;}return len;}
static void (*profile_read_hook)(void);
__attribute__((noinline)) const moonraker_profile_t*moonraker_config_profile(int i){if(profile_read_hook){void(*hook)(void)=profile_read_hook;profile_read_hook=NULL;hook();}return i>=0&&i<4?&profiles[i]:NULL;}
int moonraker_config_active_profile_index(void){return active;}
__attribute__((noinline)) const char*moonraker_config_active_profile_name(void){return profiles[active].name;}
size_t moonraker_config_profile_count(void){size_t n=0;for(int i=0;i<4;i++)n+=profiles[i].configured;return n;}
bool moonraker_config_select_profile(int i);
__attribute__((noinline)) const char*moonraker_config_host(void){return profiles[active].host;}
int moonraker_config_port(void){return profiles[active].port;}
static int saves,deletes,changed,discovered;static bool storage_ok=true;
static int fail_write_stage,last_delete_target;
static unsigned base_writes,url_writes,catalog_writes,security_writes,select_writes;
static void (*persist_hook)(void),(*url_hook)(void),(*select_hook)(void),(*remove_hook)(void);
bool moonraker_config_save_profile(int i,const char*n,const char*h,int p,const char*k){base_writes++;if(persist_hook)persist_hook();if(!storage_ok||fail_write_stage==1)return false;saves++;profiles[i].configured=true;strlcpy(profiles[i].name,n,sizeof(profiles[i].name));strlcpy(profiles[i].host,h,sizeof(profiles[i].host));profiles[i].port=p;strlcpy(profiles[i].api_key,k,sizeof(profiles[i].api_key));return true;}
bool moonraker_config_delete_profile(int i){if(remove_hook)remove_hook();if(!storage_ok)return false;last_delete_target=i;deletes++;profiles[i].configured=false;if(i==active)active=1;return true;}
bool moonraker_config_set_camera_stream_url(int i,const char*u){(void)i;(void)u;url_writes++;if(url_hook)url_hook();return fail_write_stage!=2;}
bool moonraker_config_select_profile(int i){select_writes++;if(select_hook)select_hook();if(!storage_ok)return false;active=i;return true;}
bool moonraker_config_set_transport_security(int i,bool secure){security_writes++;if(fail_write_stage==4)return false;profiles[i].secure_transport=secure;return true;}
void printer_profile_health_set(int i,bool k,bool o){(void)i;(void)k;(void)o;}
void printer_preview_cache_invalidate(int i){(void)i;}
void printer_preview_store_invalidate(int i){(void)i;}
void ui_printer_chooser_refresh(void){}
static bool camera_setup;
void ui_camera_set_setup_active(bool v){camera_setup=v;}
void ui_camera_refresh_catalog(void){}
void ui_dashboard_refresh_camera(void){}
static size_t default_camera,cleared_camera;static unsigned clears;
static bool camera_clear_ok=true;
static int cleared_profile;
static void (*camera_clear_hook)(void);
static void (*camera_set_hook)(void),(*camera_default_hook)(void),(*camera_get_hook)(void);
static int camera_set_profile,camera_default_profile;static char camera_set_name[CAMERA_CATALOG_NAME_LENGTH];
static size_t camera_set_slot;
static unsigned camera_default_writes,camera_test_starts,camera_test_polls,camera_find_starts,camera_find_polls;
static bool camera_default_ok=true,camera_test_accept=true,camera_test_ready,camera_test_ok=true;
static bool camera_find_accept=true,camera_find_ready,camera_find_found=true,camera_find_advance;
static void (*camera_test_start_hook)(void),(*camera_test_poll_hook)(void),(*camera_find_start_hook)(void),(*camera_find_poll_hook)(void);
size_t camera_catalog_default(int i){(void)i;return default_camera;}
bool camera_catalog_get(int i,size_t s,camera_catalog_entry_t*e){(void)i;if(camera_get_hook){void(*hook)(void)=camera_get_hook;camera_get_hook=NULL;hook();}*e=(camera_catalog_entry_t){.configured=true};snprintf(e->name,sizeof(e->name),"Workshop camera with long name%zu",s);snprintf(e->stream_url,sizeof(e->stream_url),"http://camera%zu.local/stream",s);return true;}
bool camera_catalog_set(int i,size_t s,const char*n,const char*u){camera_set_profile=i;camera_set_slot=s;strlcpy(camera_set_name,n,sizeof(camera_set_name));char name[32],url[192];strlcpy(name,n,sizeof(name));strlcpy(url,u,sizeof(url));catalog_writes++;if(camera_set_hook)camera_set_hook();assert(!strcmp(name,n)&&!strcmp(url,u));return fail_write_stage!=3;}
bool camera_catalog_set_default(int i,size_t s){camera_default_profile=i;camera_default_writes++;if(camera_default_hook)camera_default_hook();if(!camera_default_ok)return false;default_camera=s;return true;}
bool camera_catalog_clear(int i,size_t s){clears++;cleared_camera=s;cleared_profile=i;if(camera_clear_hook)camera_clear_hook();return camera_clear_ok;}
bool camera_test_start(const char*u){camera_test_starts++;char url[192];strlcpy(url,u,sizeof(url));if(camera_test_start_hook)camera_test_start_hook();assert(!strcmp(url,u));return camera_test_accept;}
bool camera_test_busy(void){return false;}
bool camera_test_take_result(bool*o,int*h,int*s,size_t*b){camera_test_polls++;if(camera_test_poll_hook)camera_test_poll_hook();if(!camera_test_ready)return false;camera_test_ready=false;*o=camera_test_ok;*h=640;*s=480;*b=4096;return true;}
bool camera_discovery_start(const char*h,int p,const char*k){(void)p;camera_find_starts++;char host[64],key[160];strlcpy(host,h,sizeof(host));strlcpy(key,k,sizeof(key));if(camera_find_start_hook)camera_find_start_hook();assert(!strcmp(host,h)&&!strcmp(key,k));return camera_find_accept;}
static uint32_t config_generation=1;
static int discovered_profile=-1;
uint32_t moonraker_config_generation(void){return config_generation;}
bool camera_discovery_start_for_profile(int i,const char*h,int p,const char*k){assert(i>=0&&i<MOONRAKER_CONFIG_MAX_PROFILES);discovered_profile=i;return camera_discovery_start(h,p,k);}
bool camera_discovery_busy(void){return false;}
bool camera_discovery_take_result(moonraker_webcam_t*c,bool*o,size_t*n){camera_find_polls++;if(camera_find_poll_hook)camera_find_poll_hook();if(!camera_find_ready)return false;camera_find_ready=false;*c=(moonraker_webcam_t){0};strlcpy(c->name,"Imported Camera",sizeof(c->name));strlcpy(c->stream_url,"http://imported.local/stream",sizeof(c->stream_url));*o=camera_find_found;*n=camera_find_found?3:0;if(camera_find_advance)config_generation++;return true;}
static unsigned test_starts,test_polls;
static bool test_accept=true,test_ready,test_verified;
static void (*test_start_hook)(void),(*test_poll_hook)(void);
bool moonraker_endpoint_test_start(const char*h,int p,const char*k){assert(h[0]&&p>0);test_starts++;char host[64],key[160];strlcpy(host,h,sizeof(host));strlcpy(key,k,sizeof(key));if(test_start_hook)test_start_hook();assert(!strcmp(host,h)&&!strcmp(key,k));return test_accept;}
bool moonraker_endpoint_test_busy(void){return false;}
bool moonraker_endpoint_test_take_result(moonraker_probe_result_t*r,bool*o){test_polls++;if(test_poll_hook)test_poll_hook();if(!test_ready)return false;*r=(moonraker_probe_result_t){.klippy_ready=true};strcpy(r->identity,"Verified fixture");*o=test_verified;test_ready=false;return true;}
static bool import_ok;static char imported_path[128];static int imported_profile;static unsigned imports;
static void (*import_hook)(void);
bool moonraker_transport_security_import_ca_file_for_profile(int i,const char*p){imports++;imported_profile=i;strlcpy(imported_path,p,sizeof(imported_path));if(import_hook)import_hook();assert(!strcmp(imported_path,p));return import_ok;}
const char*moonraker_transport_security_ca_pem_for_profile(int i){(void)i;return NULL;}
static void active_changed(void){changed++;}
static void discover(void){discovered++;}
static void click(lv_obj_t*o){lv_obj_send_event(o,LV_EVENT_CLICKED,NULL);}
static uint16_t raster[1024*600];static int viewport=1024;
static void flush(lv_display_t*d,const lv_area_t*a,uint8_t*p){uint16_t*v=(uint16_t*)p;for(int y=a->y1;y<=a->y2;y++)for(int x=a->x1;x<=a->x2;x++)raster[y*viewport+x]=*v++;lv_display_flush_ready(d);}
static void snapshot(int theme,const char*name){const char*folder=getenv("PROFILES_SCREENSHOTS");if(!folder)return;lv_refr_now(NULL);char path[512];snprintf(path,sizeof(path),"%s/%s-theme%d.ppm",folder,name,theme);FILE*f=fopen(path,"wb");assert(f);fprintf(f,"P6\n1024 600\n255\n");for(int i=0;i<1024*600;i++){uint16_t v=raster[i];unsigned char c[3]={(v>>11)*255/31,((v>>5)&63)*255/63,(v&31)*255/31};fwrite(c,1,3,f);}fclose(f);}
static void inside(lv_obj_t*c,lv_obj_t*p){lv_area_t a,b;lv_obj_get_coords(c,&a);lv_obj_get_coords(p,&b);if(!(a.x1>=b.x1&&a.x2<=b.x2&&a.y1>=b.y1&&a.y2<=b.y2)){fprintf(stderr,"Outside %d,%d..%d,%d vs %d,%d..%d,%d\n",a.x1,a.y1,a.x2,a.y2,b.x1,b.y1,b.x2,b.y2);abort();}}
static void fit(lv_obj_t*button){lv_obj_t*l=lv_obj_get_child(button,0);inside(l,button);lv_point_t n;lv_text_get_size(&n,lv_label_get_text(l),lv_obj_get_style_text_font(l,0),lv_obj_get_style_text_letter_space(l,0),0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);if(n.x>lv_obj_get_width(l))fprintf(stderr,"fit theme=%d viewport=%d text=%s needed=%d width=%d\n",ui_theme_get_active(),viewport,lv_label_get_text(l),n.x,lv_obj_get_width(l));assert(n.x<=lv_obj_get_width(l));assert(lv_obj_get_height(l)==lv_obj_get_style_text_font(l,0)->line_height);assert(lv_obj_get_height(button)>=48);}
static void layout(lv_obj_t*popup){lv_obj_update_layout(popup);inside(popup,lv_screen_active());lv_obj_t*body=lv_obj_get_child(popup,1),*footer=lv_obj_get_child(popup,-1);inside(body,popup);inside(footer,popup);assert(lv_obj_get_height(body)>50);for(unsigned i=0;i<lv_obj_get_child_count(footer);i++){lv_obj_t*b=lv_obj_get_child(footer,i);inside(b,footer);fit(b);}lv_area_t a,b;lv_obj_get_coords(footer,&a);lv_obj_scroll_to_y(body,2000,LV_ANIM_OFF);lv_obj_update_layout(popup);lv_obj_get_coords(footer,&b);assert(!memcmp(&a,&b,sizeof(a)));lv_obj_t*last=lv_obj_get_child(body,-1);inside(last,body);lv_obj_scroll_to_y(body,0,LV_ANIM_OFF);lv_obj_update_layout(popup);}
static void manager_polish_checks(void)
{
 lv_obj_update_layout(s_manager_popup);
 lv_obj_t *footer=lv_obj_get_child(s_manager_popup,-1);
 assert(lv_obj_get_parent(s_manager_remove_action)==footer);
 assert(lv_obj_get_parent(s_manager_edit_action)==footer);
 assert(lv_obj_get_parent(s_manager_select_action)==footer);
 assert(lv_obj_get_parent(s_manager_close_action)==footer);
 assert(lv_obj_get_child_count(footer)==4);
 for(int i=0;i<MOONRAKER_CONFIG_MAX_PROFILES;i++){
  lv_obj_t *row=s_profile_rows[i],*name=lv_obj_get_child(row,0),*endpoint=lv_obj_get_child(row,1),*badge=lv_obj_get_child(row,2);
  assert(lv_obj_get_style_radius(row,0)==0);
  assert(lv_obj_has_state(row,LV_STATE_USER_1)==(i==s_selected_profile));
  assert(!strcmp(lv_label_get_text(badge),profiles[i].configured && i==active ? "Active" : ""));
  assert(lv_label_get_long_mode(name)==LV_LABEL_LONG_DOT && lv_label_get_long_mode(endpoint)==LV_LABEL_LONG_DOT);
  assert(lv_obj_get_height(name)==lv_obj_get_style_text_font(name,0)->line_height);
  assert(lv_obj_get_height(endpoint)==lv_obj_get_style_text_font(endpoint,0)->line_height);
  assert(lv_obj_get_style_pad_right(name,0)>=lv_obj_get_width(badge));
 }
 assert(!strcmp(lv_label_get_text(lv_obj_get_child(s_manager_edit_action,0)),
  profiles[s_selected_profile].configured?LV_SYMBOL_EDIT " Edit printer":LV_SYMBOL_PLUS " Add printer"));
}
static void editor_polish_checks(void)
{
 lv_obj_t *body=lv_obj_get_child(s_editor_popup,1);
 lv_obj_t *host_group=lv_obj_get_parent(s_editor_host),*port_group=lv_obj_get_parent(s_editor_port);
 assert(host_group!=port_group && lv_obj_get_parent(host_group)==lv_obj_get_parent(port_group));
 inside(s_editor_host,host_group);inside(s_editor_port,port_group);
 lv_area_t host,port;lv_obj_get_coords(s_editor_host,&host);lv_obj_get_coords(s_editor_port,&port);
 assert(host.y1==port.y1 && host.x2<port.x1);
 lv_point_t digits;lv_text_get_size(&digits,"65535",lv_obj_get_style_text_font(s_editor_port,0),lv_obj_get_style_text_letter_space(s_editor_port,0),0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
 assert(digits.x<=lv_obj_get_content_width(s_editor_port));
 fit(s_editor_discover_action);fit(s_security_opener);fit(s_editor_auth_action);
 assert(!strcmp(lv_label_get_text(lv_obj_get_child(s_editor_test_action,0)),LV_SYMBOL_PLAY " Test connection"));
 if(viewport==1024){
  assert(lv_obj_get_scroll_bottom(body)<=0);
  inside(lv_obj_get_parent(s_editor_name),body);inside(lv_obj_get_parent(host_group),body);
  inside(s_security_opener,body);inside(s_editor_auth_action,body);inside(s_editor_status,body);
 }
}
static void camera_polish_checks(void)
{
 lv_obj_update_layout(s_editor_camera_popup);
 int columns=viewport>=900?4:2;
 for(unsigned i=0;i<CAMERA_CATALOG_MAX_CAMERAS;i++){
  lv_obj_t *button=s_editor_camera_slot_buttons[i],*name=lv_obj_get_child(button,0),*marker=lv_obj_get_child(button,1);
  inside(name,button);inside(marker,button);
  /* DOT mode intentionally truncates names; enforce the clipping box and a
   * single line rather than requiring the full visible buffer to measure in. */
  assert(lv_label_get_long_mode(name)==LV_LABEL_LONG_DOT);
  assert(lv_obj_get_height(name)==lv_obj_get_style_text_font(name,0)->line_height);
  lv_area_t a,b;lv_obj_get_coords(name,&a);lv_obj_get_coords(marker,&b);assert(a.y2<b.y1);
  assert(lv_obj_has_state(button,LV_STATE_CHECKED)==(i==s_editor_camera_slot));
  assert(lv_obj_get_y(button)==lv_obj_get_y(s_editor_camera_slot_buttons[(i/columns)*columns]));
  for(unsigned j=0;j<i;j++){
   lv_obj_get_coords(button,&a);lv_obj_get_coords(s_editor_camera_slot_buttons[j],&b);
   assert(a.x2<b.x1 || b.x2<a.x1 || a.y2<b.y1 || b.y2<a.y1);
  }
  if(!strcmp(lv_label_get_text(marker),"Default"))assert(i==default_camera);
 }
 fit(s_camera_find);fit(s_camera_test);fit(s_camera_default);fit(s_camera_remove_action);
 assert(lv_obj_get_parent(s_editor_camera_popup)==lv_screen_active());
}
static void count_scroll_draw(lv_event_t *event)
{
 unsigned *count=lv_event_get_user_data(event);(*count)++;
}
static void profile_scroll_render_check(lv_obj_t *popup,bool require_cover)
{
 lv_obj_t *body=lv_obj_get_child(popup,1),*footer=lv_obj_get_child(popup,-1);
 lv_obj_update_layout(popup);lv_obj_scroll_to_y(body,0,LV_ANIM_OFF);lv_refr_now(NULL);
 lv_area_t before,after;lv_obj_get_coords(footer,&before);
 bool overflow=lv_obj_get_scroll_bottom(body)>0;
 unsigned parent_draws=0,body_draws=0;
 lv_obj_add_event_cb(popup,count_scroll_draw,LV_EVENT_DRAW_MAIN_BEGIN,&parent_draws);
 lv_obj_add_event_cb(body,count_scroll_draw,LV_EVENT_DRAW_MAIN_BEGIN,&body_draws);
 for(int i=1;i<=6;i++){
  if(overflow)lv_obj_scroll_to_y(body,i*24,LV_ANIM_OFF);else lv_obj_invalidate(body);lv_refr_now(NULL);
 }
 lv_obj_get_coords(footer,&after);assert(!memcmp(&before,&after,sizeof(before)));
 assert((overflow?lv_obj_get_scroll_y(body)>0:lv_obj_get_scroll_y(body)==0) && body_draws>0);
 if(getenv("PROFILE_SCROLL_DIAGNOSTIC"))printf("SCROLL theme=%d overflow=%d parent_draws=%u body_draws=%u\n",ui_theme_get_active(),overflow,parent_draws,body_draws);
 fflush(stdout);
 if(require_cover)assert(parent_draws==0);
 lv_obj_remove_event_cb_with_user_data(popup,count_scroll_draw,&parent_draws);
 lv_obj_remove_event_cb_with_user_data(body,count_scroll_draw,&body_draws);
 lv_obj_scroll_to_y(body,0,LV_ANIM_OFF);lv_obj_update_layout(popup);
}
static void reset(void){default_camera=0;import_ok=false;memset(profiles,0,sizeof(profiles));profiles[0]=(moonraker_profile_t){.configured=true,.name="Workshop printer with long name",.host="workshop-printer-long-hostname.local",.port=7125};profiles[1]=(moonraker_profile_t){.configured=true,.name="Printer B",.host="printer-b.local",.port=7126};active=0;storage_ok=true;}
static void nested_checks(int theme,bool render)
{
    manager_edit_cb(NULL);
    lv_obj_t *target=s_editor_port;
    click(target);assert(s_editor_keyboard_popup);layout(s_editor_keyboard_popup);
    assert(lv_keyboard_get_mode(s_editor_keyboard)==LV_KEYBOARD_MODE_NUMBER);
    assert(lv_textarea_get_max_length(s_editor_keyboard_value)==5);
    assert(!strcmp(lv_textarea_get_accepted_chars(s_editor_keyboard_value),"0123456789"));
    keyboard_layout_check(s_editor_keyboard_popup,s_editor_keyboard,s_editor_keyboard_value);
    lv_textarea_set_text(s_editor_keyboard_value,"0");lv_obj_t*keyboard_owner=s_editor_keyboard_popup;click(target);assert(s_editor_keyboard_popup==keyboard_owner&&!strcmp(lv_textarea_get_text(s_editor_keyboard_value),"0"));lv_obj_send_event(s_editor_keyboard,LV_EVENT_READY,NULL);assert(s_editor_keyboard_popup&&!strcmp(lv_textarea_get_text(s_editor_keyboard_value),"0"));
    lv_textarea_set_text(s_editor_keyboard_value,"65536");lv_obj_send_event(s_editor_keyboard,LV_EVENT_READY,NULL);assert(s_editor_keyboard_popup);
    lv_textarea_set_text(s_editor_keyboard_value,"7129");
    if(render)snapshot(theme,"keyboard");
    lv_obj_send_event(s_editor_keyboard,LV_EVENT_READY,NULL);
    assert(!s_editor_keyboard_popup&&!strcmp(lv_textarea_get_text(target),"7129"));
    click(target);lv_textarea_set_text(s_editor_keyboard_value,"7130");
    lv_obj_send_event(s_editor_keyboard,LV_EVENT_CANCEL,NULL);
    assert(!s_editor_keyboard_popup&&!strcmp(lv_textarea_get_text(target),"7129"));
    click(s_editor_name);assert(lv_textarea_get_max_length(s_editor_keyboard_value)==MOONRAKER_CONFIG_NAME_LENGTH-1);
    lv_obj_delete(s_editor_keyboard_popup);assert(!s_editor_keyboard_value&&!s_editor_keyboard_target);
    editor_auth_open_cb(NULL);layout(s_editor_auth_popup);
    assert(lv_textarea_get_password_mode(s_editor_auth_key));
    if(render)snapshot(theme,"authentication");
    click(s_editor_auth_key);layout(s_editor_keyboard_popup);
    assert(lv_textarea_get_password_mode(s_editor_keyboard_value));
    assert(lv_textarea_get_max_length(s_editor_keyboard_value)==MOONRAKER_CONFIG_API_KEY_LENGTH-1);
    lv_textarea_set_text(s_editor_keyboard_value,"test-key");editor_keyboard_popup_done_cb(NULL);
    editor_auth_save_cb(NULL);assert(!strcmp(s_editor_api_key,"test-key"));
    editor_auth_open_cb(NULL);lv_textarea_set_text(s_editor_auth_key,"discarded-key");editor_auth_cancel_cb(NULL);
    assert(!strcmp(s_editor_api_key,"test-key"));
    editor_auth_open_cb(NULL);click(s_editor_auth_key);lv_obj_delete(s_editor_auth_popup);
    assert(!s_editor_auth_key&&!s_editor_keyboard_popup);
    s_editor_secure=true;editor_security_open_cb(NULL);layout(s_editor_security_popup);
    if(render)snapshot(theme,"security");
    editor_security_cancel_cb(NULL);assert(s_editor_secure);
    editor_security_open_cb(NULL);editor_security_standard_cb(NULL);assert(!s_editor_secure);
    editor_security_open_cb(NULL);editor_security_pem_picker_open_cb(NULL);layout(s_editor_pem_picker);
    lv_obj_t *pem_body=lv_obj_get_child(s_editor_pem_picker,1);
    assert(lv_obj_get_child_count(pem_body)==EDITOR_PEM_MAX_FILES+1);
    if(render)snapshot(theme,"certificates");
    click(lv_obj_get_child(pem_body,1));assert(s_editor_pem_picker&&!s_editor_secure);
    assert(strstr(imported_path,"certificate_with_a_long_name_1.pem"));
    import_ok=true;click(lv_obj_get_child(pem_body,2));
    assert(!s_editor_pem_picker&&!s_editor_security_popup&&s_editor_secure);
    editor_security_open_cb(NULL);directory_available=false;editor_security_pem_picker_open_cb(NULL);layout(s_editor_pem_picker);
    assert(lv_obj_get_child_count(lv_obj_get_child(s_editor_pem_picker,1))==2);
    lv_obj_delete(s_editor_security_popup);assert(!s_editor_security_popup&&!s_editor_pem_picker);directory_available=true;
    editor_camera_open_cb(NULL);assert(camera_setup);layout(s_editor_camera_popup);
    lv_obj_update_layout(s_editor_camera_popup);
    for(unsigned i=0;i<4;i++){
        lv_obj_t *button=s_editor_camera_slot_buttons[i],*label=lv_obj_get_child(button,0);
        inside(button,lv_obj_get_parent(button));inside(label,button);
        assert(lv_label_get_long_mode(label)==LV_LABEL_LONG_DOT);
    }
    camera_polish_checks();profile_scroll_render_check(s_editor_camera_popup,true);
    if(render)snapshot(theme,"camera");
    click(s_editor_camera_slot_buttons[2]);assert(s_editor_camera_slot==2);camera_polish_checks();
    assert(!strcmp(lv_label_get_text(lv_obj_get_child(s_editor_camera_slot_buttons[default_camera],1)),"Default"));
    assert(strstr(lv_textarea_get_text(s_editor_camera_stream),"camera2.local"));
    click(s_editor_camera_stream);layout(s_editor_keyboard_popup);
    assert(lv_textarea_get_max_length(s_editor_keyboard_value)==MOONRAKER_CONFIG_CAMERA_URL_LENGTH-1);
    editor_keyboard_cancel_cb(NULL);
    editor_camera_make_default_cb(NULL);assert(default_camera==2);
    editor_camera_test_cb(NULL);editor_camera_discover_cb(NULL);
    assert(s_editor_camera_test_timer&&s_editor_camera_discovery_timer&&discovered_profile==s_editor_profile);
    editor_camera_remove_cb(NULL);layout(s_editor_camera_remove_popup);
    if(render)snapshot(theme,"camera-remove");
    unsigned before=clears;editor_camera_remove_cancel_cb(NULL);assert(clears==before);
    editor_camera_remove_cb(NULL);editor_camera_remove_apply_cb(NULL);assert(clears==before+1&&cleared_camera==2);
    click(s_editor_camera_name);lv_obj_delete(s_editor_camera_popup);
    assert(!camera_setup&&!s_editor_keyboard_popup&&!s_editor_camera_stream);
    assert(!s_editor_camera_test_timer&&!s_editor_camera_discovery_timer);
    editor_camera_open_cb(NULL);click(s_editor_camera_stream);lv_obj_delete(s_editor_popup);
    assert(!camera_setup&&!s_editor_camera_popup&&!s_editor_keyboard_popup&&!s_editor_host);
}
static void nested_test(void){editor_test_cb(NULL);editor_save_cb(NULL);}
static void reopen_editor(void){editor_close();manager_edit_cb(NULL);}
static void replace_editor_deleted(lv_event_t *event){(void)event;manager_edit_cb(NULL);lv_textarea_set_text(s_editor_host,"replacement.local");}
static void replace_manager_deleted(lv_event_t *event){(void)event;ui_printer_profiles_show_for_slot(2,active_changed,discover);}
static void replace_auth_deleted(lv_event_t *event){(void)event;editor_auth_open_cb(NULL);}
static void profile_draft_safety(void)
{
 reset();test_ready=false;test_accept=true;ui_printer_profiles_show_for_slot(0,active_changed,discover);manager_edit_cb(NULL);
 unsigned before=test_starts;int persisted=saves;
 test_start_hook=nested_test;editor_test_cb(NULL);test_start_hook=NULL;
 assert(test_starts==before+1&&saves==persisted&&s_editor_test_timer);
 editor_test_cb(NULL);assert(test_starts==before+1);
 lv_timer_t *other=lv_timer_create(editor_test_poll_cb,60000,NULL);unsigned polls=test_polls;
 editor_test_poll_cb(other);assert(test_polls==polls&&s_editor_test_timer);lv_timer_delete(other);
 lv_textarea_set_text(s_editor_host,"changed.local");editor_test_poll_cb(s_editor_test_timer);
 assert(!s_editor_test_timer&&strstr(lv_label_get_text(s_editor_status),"details changed"));
 for(unsigned changed_field=0;changed_field<4;++changed_field){
  editor_test_cb(NULL);assert(s_editor_test_timer);
  if(changed_field==0)lv_textarea_set_text(s_editor_port,"7126");
  if(changed_field==1)strcpy(s_editor_api_key,"new-key");
  if(changed_field==2)s_editor_secure=!s_editor_secure;
  if(changed_field==3)config_generation++;
  polls=test_polls;editor_test_poll_cb(s_editor_test_timer);assert(!s_editor_test_timer&&test_polls==polls);
  lv_textarea_set_text(s_editor_port,"7125");s_editor_secure=false;
 }
 test_accept=false;editor_test_cb(NULL);assert(!s_editor_test_timer);test_accept=true;
 test_start_hook=reopen_editor;editor_test_cb(NULL);test_start_hook=NULL;
 assert(s_editor_popup&&!s_editor_test_timer&&s_test_draft.key[0]==0);
 editor_test_cb(NULL);test_poll_hook=reopen_editor;test_ready=true;test_verified=true;
 editor_test_poll_cb(s_editor_test_timer);test_poll_hook=NULL;
 assert(s_editor_popup&&!s_editor_test_timer&&!strstr(lv_label_get_text(s_editor_status),"Verified fixture"));
 editor_test_cb(NULL);test_ready=true;editor_test_poll_cb(s_editor_test_timer);
 assert(!s_editor_test_timer&&strstr(lv_label_get_text(s_editor_status),"Verified fixture")&&!s_test_draft.key[0]);
 lv_obj_add_state(s_editor_test_action,LV_STATE_DISABLED);before=test_starts;editor_test_cb(NULL);assert(test_starts==before);
 lv_obj_remove_state(s_editor_test_action,LV_STATE_DISABLED);
 lv_obj_t *fake=lv_button_create(s_editor_popup);lv_obj_add_event_cb(fake,editor_test_cb,LV_EVENT_CLICKED,NULL);
 click(fake);assert(test_starts==before);lv_obj_delete(fake);
 editor_auth_open_cb(NULL);lv_textarea_set_text(s_editor_auth_key,"retained-key");
 lv_obj_delete(s_editor_auth_use);editor_auth_save_cb(NULL);assert(strcmp(s_editor_api_key,"retained-key"));editor_auth_close();
 editor_auth_open_cb(NULL);lv_obj_delete(s_editor_auth_key);assert(!s_editor_auth_key);editor_auth_save_cb(NULL);editor_auth_close();
 editor_auth_open_cb(NULL);lv_textarea_set_text(s_editor_auth_key,"retained-key");
 lv_obj_add_event_cb(s_editor_auth_popup,replace_auth_deleted,LV_EVENT_DELETE,NULL);editor_auth_save_cb(NULL);
 assert(s_editor_auth_popup&&s_editor_auth_key&&!strcmp(s_editor_api_key,"retained-key"));editor_auth_close();
 click(s_editor_host);lv_obj_delete(s_editor_host);assert(!s_editor_host&&!s_editor_keyboard_target);
 editor_keyboard_popup_done_cb(NULL);assert(s_editor_keyboard_popup);editor_keyboard_discard();editor_close();manager_edit_cb(NULL);
 click(s_editor_host);lv_obj_delete(s_editor_keyboard_value);assert(!s_editor_keyboard_value&&!lv_keyboard_get_textarea(s_editor_keyboard));
 editor_keyboard_popup_done_cb(NULL);editor_keyboard_discard();click(s_editor_host);lv_obj_delete(s_editor_key_done);
 editor_keyboard_popup_done_cb(NULL);assert(s_editor_keyboard_popup);editor_keyboard_discard();
 editor_test_cb(NULL);lv_obj_delete(s_editor_status);editor_test_poll_cb(s_editor_test_timer);assert(!s_editor_status&&!s_editor_test_timer);
 editor_close();manager_edit_cb(NULL);lv_obj_add_event_cb(s_editor_popup,replace_editor_deleted,LV_EVENT_DELETE,NULL);editor_close();
 assert(s_editor_popup&&s_editor_host&&!strcmp(lv_textarea_get_text(s_editor_host),"replacement.local"));
 lv_obj_add_event_cb(s_manager_popup,replace_manager_deleted,LV_EVENT_DELETE,NULL);ui_printer_profiles_close_all();
 assert(s_manager_popup&&s_editor_popup&&s_editor_profile==2&&s_active_changed_cb==active_changed);
 ui_printer_profiles_close_all();assert(!s_manager_popup&&!s_editor_popup&&!s_editor_test_timer&&!s_test_draft.key[0]);
}
static void persist_nested(void){editor_save_cb(NULL);manager_select_cb(NULL);manager_delete_cb(NULL);ui_printer_profiles_set_discovered_endpoint("redirected.local",7125,"Redirect");assert(strcmp(lv_textarea_get_text(s_editor_host),"redirected.local"));}
static void persist_replace(void){ui_printer_profiles_close_all();ui_printer_profiles_show_for_slot(2,active_changed,discover);}
static void remove_nested(void){delete_confirm_cb(NULL);s_selected_profile=1;}
static void select_nested(void){manager_select_cb(NULL);manager_delete_cb(NULL);}
static void wrong_changed(void){assert(!"Repeated Show replaced the callback");}
static void profile_persistence_safety(void)
{
 reset();ui_printer_profiles_show_for_slot(0,active_changed,discover);manager_edit_cb(NULL);
 ui_printer_profiles_show(wrong_changed,discover);assert(s_active_changed_cb==active_changed);
 unsigned writes=base_writes;persist_hook=persist_nested;editor_save_cb(NULL);persist_hook=NULL;
 assert(base_writes==writes+1&&!s_editor_popup);
 for(int stage=1;stage<=4;++stage){
  manager_edit_cb(NULL);lv_textarea_set_text(s_editor_host,"retry.local");fail_write_stage=stage;
  int notifications=changed;editor_save_cb(NULL);
  assert(s_editor_popup&&!strcmp(lv_textarea_get_text(s_editor_host),"retry.local"));
  assert(changed==notifications+(stage==1?0:1));
  if(stage>1)assert(strstr(lv_label_get_text(s_editor_status),"saved"));
  fail_write_stage=0;editor_save_cb(NULL);assert(!s_editor_popup);
 }
 manager_edit_cb(NULL);writes=base_writes;lv_obj_add_state(s_editor_save_action,LV_STATE_DISABLED);
 editor_save_cb(NULL);assert(base_writes==writes);lv_obj_remove_state(s_editor_save_action,LV_STATE_DISABLED);
 lv_obj_t *fake=lv_button_create(s_editor_popup);lv_obj_add_event_cb(fake,editor_save_cb,LV_EVENT_CLICKED,NULL);click(fake);
 assert(base_writes==writes);lv_obj_delete(fake);lv_obj_delete(s_editor_save_action);editor_save_cb(NULL);assert(base_writes==writes);
 editor_close();manager_edit_cb(NULL);lv_textarea_set_text(s_editor_host,"captured.local");
 unsigned urls=url_writes;persist_hook=persist_replace;editor_save_cb(NULL);persist_hook=NULL;
 assert(s_editor_popup&&s_editor_profile==2&&url_writes==urls&&!strcmp(profiles[0].host,"captured.local"));
 ui_printer_profiles_close_all();reset();ui_printer_profiles_show_for_slot(0,active_changed,discover);manager_edit_cb(NULL);
 unsigned names=catalog_writes;url_hook=persist_replace;editor_save_cb(NULL);url_hook=NULL;
 assert(s_editor_popup&&s_editor_profile==2&&catalog_writes==names);ui_printer_profiles_close_all();
 reset();ui_printer_profiles_show_for_slot(1,active_changed,discover);unsigned selections=select_writes;
 storage_ok=false;manager_select_cb(NULL);assert(active==0&&select_writes==selections+1);
 storage_ok=true;select_hook=select_nested;manager_select_cb(NULL);select_hook=NULL;assert(active==1&&select_writes==selections+2);
 ui_printer_profiles_close_all();reset();ui_printer_profiles_show_for_slot(0,active_changed,discover);
 manager_delete_cb(NULL);int removed=deletes;printer_profile_health_set(0,true,false);config_generation++;
 delete_confirm_cb(NULL);assert(deletes==removed&&s_delete_popup&&strstr(lv_label_get_text(s_delete_status),"changed"));delete_close();
 manager_delete_cb(NULL);profiles[1].configured=false;delete_confirm_cb(NULL);assert(deletes==removed&&s_delete_popup);profiles[1].configured=true;delete_close();
 manager_delete_cb(NULL);lv_obj_delete(s_delete_apply);delete_confirm_cb(NULL);assert(deletes==removed);delete_close();
 manager_delete_cb(NULL);storage_ok=false;delete_confirm_cb(NULL);assert(s_delete_popup&&deletes==removed);
 storage_ok=true;remove_hook=remove_nested;delete_confirm_cb(NULL);remove_hook=NULL;
 assert(deletes==removed+1&&last_delete_target==0&&!s_delete_popup&&active==1);
 delete_confirm_cb(NULL);assert(deletes==removed+1);
 ui_printer_profiles_close_all();reset();ui_printer_profiles_show_for_slot(1,active_changed,discover);
 select_hook=persist_replace;manager_select_cb(NULL);select_hook=NULL;assert(s_editor_popup&&s_editor_profile==2&&active==1);
 ui_printer_profiles_close_all();
}
static void camera_remove_nested(void)
{
 editor_camera_remove_apply_cb(NULL);editor_camera_make_default_cb(NULL);editor_camera_save_cb(NULL);
 editor_camera_test_cb(NULL);editor_camera_discover_cb(NULL);
 click(s_editor_camera_slot_buttons[3]);
}
static void camera_replace_deleted(lv_event_t *event){(void)event;editor_camera_open_cb(NULL);}
static void camera_review_replace_deleted(lv_event_t *event){(void)event;editor_camera_remove_cb(NULL);}
static void camera_remove_replace(void){editor_close();manager_edit_cb(NULL);}
static void camera_remove_delete_status(void){lv_obj_delete(s_editor_camera_status);}
static void profile_camera_removal_safety(void)
{
 reset();camera_clear_ok=true;ui_printer_profiles_show_for_slot(0,active_changed,discover);manager_edit_cb(NULL);editor_camera_open_cb(NULL);
 click(s_editor_camera_slot_buttons[2]);unsigned before=clears;unsigned writes=catalog_writes;
 lv_obj_add_state(s_camera_remove_action,LV_STATE_DISABLED);editor_camera_remove_cb(NULL);assert(!s_editor_camera_remove_popup);
 lv_obj_remove_state(s_camera_remove_action,LV_STATE_DISABLED);
 lv_obj_t *fake=lv_button_create(s_editor_camera_popup);lv_obj_add_event_cb(fake,editor_camera_remove_cb,LV_EVENT_CLICKED,NULL);
 click(fake);assert(!s_editor_camera_remove_popup);lv_obj_delete(fake);
 editor_camera_test_cb(NULL);editor_camera_discover_cb(NULL);assert(s_editor_camera_test_timer&&s_editor_camera_discovery_timer);
 editor_camera_remove_cb(NULL);lv_obj_t *review=s_editor_camera_remove_popup;
 assert(review&&s_camera_remove_slot==2&&!s_editor_camera_test_timer&&!s_editor_camera_discovery_timer);
 click(s_editor_camera_slot_buttons[3]);editor_camera_save_cb(NULL);editor_camera_make_default_cb(NULL);
 assert(s_editor_camera_slot==2&&s_editor_camera_remove_popup==review&&catalog_writes==writes);
 lv_obj_add_state(s_camera_remove_apply,LV_STATE_DISABLED);editor_camera_remove_apply_cb(NULL);assert(clears==before);
 lv_obj_remove_state(s_camera_remove_apply,LV_STATE_DISABLED);
 fake=lv_button_create(review);lv_obj_add_event_cb(fake,editor_camera_remove_apply_cb,LV_EVENT_CLICKED,NULL);
 click(fake);assert(clears==before);lv_obj_delete(fake);
 config_generation++;editor_camera_remove_apply_cb(NULL);
 assert(clears==before&&s_editor_camera_remove_popup==review&&strstr(lv_label_get_text(s_camera_remove_status),"changed"));layout(review);
 editor_camera_remove_cancel_cb(NULL);assert(!s_editor_camera_remove_popup&&clears==before);
 editor_camera_remove_cb(NULL);review=s_editor_camera_remove_popup;
 camera_clear_ok=false;editor_camera_remove_apply_cb(NULL);
 assert(clears==before+1&&s_editor_camera_remove_popup==review&&strstr(lv_label_get_text(s_camera_remove_status),"Try Remove"));layout(review);
 camera_clear_ok=true;camera_clear_hook=camera_remove_nested;editor_camera_remove_apply_cb(NULL);camera_clear_hook=NULL;
 assert(clears==before+2&&cleared_profile==0&&cleared_camera==2&&!s_editor_camera_remove_popup);
 assert(!lv_textarea_get_text(s_editor_camera_stream)[0]&&strstr(lv_label_get_text(s_editor_camera_status),"removed"));
 editor_camera_remove_apply_cb(NULL);assert(clears==before+2);
 editor_camera_remove_cb(NULL);lv_obj_delete(s_camera_remove_apply);assert(!s_camera_remove_apply);
 editor_camera_remove_apply_cb(NULL);assert(clears==before+2);editor_camera_remove_cancel_cb(NULL);
 editor_camera_remove_cb(NULL);lv_obj_delete(s_camera_remove_status);camera_clear_ok=false;editor_camera_remove_apply_cb(NULL);
 assert(s_editor_camera_remove_popup);camera_clear_ok=true;editor_camera_remove_cancel_cb(NULL);
 editor_camera_remove_cb(NULL);lv_obj_delete(s_editor_camera_remove_popup);
 assert(!s_editor_camera_remove_popup&&!s_camera_remove_apply&&!s_camera_remove_cancel&&s_camera_remove_profile==-1);
 editor_camera_remove_cb(NULL);lv_obj_add_event_cb(s_editor_camera_remove_popup,camera_review_replace_deleted,LV_EVENT_DELETE,NULL);
 editor_camera_remove_cancel_cb(NULL);assert(s_editor_camera_remove_popup&&s_camera_remove_apply&&s_camera_remove_profile==0);
 editor_camera_remove_cancel_cb(NULL);
 /* The reviewed target remains captured even if selection is changed outside an event. */
 editor_camera_remove_cb(NULL);s_editor_camera_slot=3;editor_camera_remove_apply_cb(NULL);
 assert(cleared_camera==2&&cleared_profile==0&&s_editor_camera_slot==3);before=clears;
 /* A backend callback can replace the parent editor; old completion must not paint it. */
 editor_camera_remove_cb(NULL);camera_clear_hook=camera_remove_replace;editor_camera_remove_apply_cb(NULL);camera_clear_hook=NULL;
 assert(clears==before+1&&cleared_camera==3&&s_editor_popup&&!s_editor_camera_popup&&!s_camera_remove_apply);
 editor_camera_open_cb(NULL);s_editor_camera_slot=0;strcpy(s_editor_camera_url,"http://primary.local/stream");
 editor_camera_remove_cb(NULL);camera_clear_hook=camera_remove_delete_status;editor_camera_remove_apply_cb(NULL);camera_clear_hook=NULL;
 assert(!s_editor_camera_url[0]&&!s_editor_camera_remove_popup&&!s_editor_camera_status&&!lv_textarea_get_text(s_editor_camera_stream)[0]);
 editor_camera_close();editor_camera_open_cb(NULL);lv_obj_delete(s_camera_remove_action);editor_camera_remove_cb(NULL);assert(!s_editor_camera_remove_popup);
 editor_camera_close();editor_camera_open_cb(NULL);
 lv_obj_delete(s_editor_camera_slot_buttons[2]);assert(!s_editor_camera_slot_buttons[2]);editor_camera_update_slot_buttons();
 lv_obj_add_event_cb(s_editor_camera_popup,camera_replace_deleted,LV_EVENT_DELETE,NULL);editor_camera_close();
 assert(s_editor_camera_popup&&s_editor_camera_stream&&s_camera_remove_action&&camera_setup);
 editor_camera_test_cb(NULL);editor_camera_discover_cb(NULL);
 lv_timer_t *test=s_editor_camera_test_timer,*discovery=s_editor_camera_discovery_timer;
 lv_timer_t *other=lv_timer_create(editor_camera_test_poll_cb,60000,NULL);
 editor_camera_test_poll_cb(other);editor_camera_discovery_poll_cb(other);
 assert(s_editor_camera_test_timer==test&&s_editor_camera_discovery_timer==discovery);lv_timer_delete(other);
 editor_camera_remove_cb(NULL);lv_obj_add_event_cb(s_editor_camera_remove_popup,camera_replace_deleted,LV_EVENT_DELETE,NULL);
 editor_camera_close();assert(s_editor_camera_popup&&s_editor_camera_name&&camera_setup&&!s_editor_camera_test_timer&&!s_editor_camera_discovery_timer);
 ui_printer_profiles_close_all();assert(!s_editor_camera_popup&&!s_camera_remove_action&&!camera_setup);
}
static void camera_operation_nested(void)
{
 editor_camera_save_cb(NULL);editor_camera_make_default_cb(NULL);editor_camera_test_cb(NULL);editor_camera_discover_cb(NULL);
 editor_camera_remove_cb(NULL);click(s_editor_camera_slot_buttons[3]);editor_save_cb(NULL);
}
static void camera_reopen(void){editor_camera_close();editor_camera_open_cb(NULL);}
static void camera_parent_replace(void){editor_close();manager_edit_cb(NULL);}
static void camera_save_delete_name(void){lv_obj_delete(s_editor_camera_name);}
static void camera_poll_change_url(void){lv_textarea_set_text(s_editor_camera_stream,"http://changed-during-poll.local/stream");}
static void camera_camera_fixture_open(void)
{
 ui_printer_profiles_close_all();reset();ui_printer_profiles_show_for_slot(0,active_changed,discover);manager_edit_cb(NULL);
 editor_camera_open_cb(NULL);lv_textarea_set_text(s_editor_camera_stream,"http://draft.local/stream");
 lv_textarea_set_text(s_editor_camera_name,"Captured Camera");
}
static void profile_camera_operation_safety(void)
{
 camera_camera_fixture_open();
 unsigned writes=catalog_writes,defaults=camera_default_writes,tests=camera_test_starts,finds=camera_find_starts;
 lv_obj_t **actions[]={&s_camera_save,&s_camera_default,&s_camera_test,&s_camera_find,&s_camera_cancel};
 lv_event_cb_t callbacks[]={editor_camera_save_cb,editor_camera_make_default_cb,editor_camera_test_cb,editor_camera_discover_cb,editor_camera_cancel_cb};
 for(unsigned i=0;i<5;i++){
  lv_obj_add_state(*actions[i],LV_STATE_DISABLED);callbacks[i](NULL);assert(s_editor_camera_popup);
  lv_obj_remove_state(*actions[i],LV_STATE_DISABLED);
  lv_obj_t *fake=lv_button_create(s_editor_camera_popup);lv_obj_add_event_cb(fake,callbacks[i],LV_EVENT_CLICKED,NULL);click(fake);lv_obj_delete(fake);
 }
 assert(catalog_writes==writes&&camera_default_writes==defaults&&camera_test_starts==tests&&camera_find_starts==finds);
 camera_set_hook=camera_operation_nested;editor_camera_make_default_cb(NULL);camera_set_hook=NULL;
 assert(catalog_writes==writes+1&&camera_default_writes==defaults+1&&camera_test_starts==tests&&camera_find_starts==finds);
 assert(s_editor_camera_slot==0&&camera_set_slot==0&&camera_set_profile==0&&camera_default_profile==0);
 camera_default_ok=false;editor_camera_make_default_cb(NULL);assert(strstr(lv_label_get_text(s_editor_camera_status),"Camera saved"));
 camera_default_ok=true;editor_camera_make_default_cb(NULL);assert(strstr(lv_label_get_text(s_editor_camera_status),"now the default"));
 fail_write_stage=3;editor_camera_save_cb(NULL);assert(s_editor_camera_popup&&strstr(lv_label_get_text(s_editor_camera_status),"Retry"));
 assert(!strcmp(lv_textarea_get_text(s_editor_camera_stream),"http://draft.local/stream"));
 fail_write_stage=0;editor_camera_save_cb(NULL);assert(!s_editor_camera_popup&&strstr(lv_label_get_text(s_editor_status),"applied"));
 editor_camera_open_cb(NULL);writes=catalog_writes;lv_textarea_set_text(s_editor_camera_stream,"rtsp://invalid.local");
 click(s_editor_camera_slot_buttons[2]);editor_camera_make_default_cb(NULL);editor_camera_save_cb(NULL);assert(catalog_writes==writes&&s_editor_camera_slot==0);
 lv_textarea_set_text(s_editor_camera_stream,"http://valid.local/stream");click(s_editor_camera_slot_buttons[2]);
 fail_write_stage=3;click(s_editor_camera_slot_buttons[1]);assert(s_editor_camera_slot==2);fail_write_stage=0;
 click(s_editor_camera_slot_buttons[1]);assert(s_editor_camera_slot==1);
 defaults=camera_default_writes;camera_set_hook=camera_parent_replace;editor_camera_make_default_cb(NULL);camera_set_hook=NULL;
 assert(!s_editor_camera_popup&&s_editor_popup&&camera_default_writes==defaults);
 editor_camera_open_cb(NULL);lv_textarea_set_text(s_editor_camera_stream,"http://valid.local/stream");
 camera_default_hook=camera_parent_replace;editor_camera_make_default_cb(NULL);camera_default_hook=NULL;
 assert(!s_editor_camera_popup&&s_editor_popup);
 editor_camera_open_cb(NULL);lv_textarea_set_text(s_editor_camera_stream,"http://valid.local/stream");
 camera_set_hook=camera_parent_replace;editor_camera_save_cb(NULL);camera_set_hook=NULL;
 assert(s_editor_popup&&!s_editor_camera_popup&&!strstr(lv_label_get_text(s_editor_status),"source applied"));
 editor_camera_open_cb(NULL);camera_get_hook=camera_reopen;editor_camera_load_slot();assert(s_editor_camera_popup&&s_camera_save);
 editor_camera_open_cb(NULL);lv_obj_delete(s_editor_camera_name);writes=catalog_writes;
 editor_camera_save_cb(NULL);editor_camera_make_default_cb(NULL);assert(catalog_writes==writes);
 for(unsigned i=0;i<5;i++){
  camera_camera_fixture_open();writes=catalog_writes;tests=camera_test_starts;finds=camera_find_starts;defaults=camera_default_writes;
  lv_obj_delete(*actions[i]);callbacks[i](NULL);
  assert(s_editor_camera_popup&&catalog_writes==writes&&camera_test_starts==tests&&camera_find_starts==finds&&camera_default_writes==defaults);
 }
 camera_camera_fixture_open();camera_set_hook=camera_save_delete_name;editor_camera_save_cb(NULL);camera_set_hook=NULL;
 assert(s_editor_camera_popup&&!s_editor_camera_name&&!strcmp(s_editor_camera_url,"http://draft.local/stream"));
 camera_camera_fixture_open();tests=camera_test_starts;finds=camera_find_starts;
 camera_test_start_hook=camera_operation_nested;editor_camera_test_cb(NULL);camera_test_start_hook=NULL;
 camera_find_start_hook=camera_operation_nested;editor_camera_discover_cb(NULL);camera_find_start_hook=NULL;
 assert(camera_test_starts==tests+1&&camera_find_starts==finds+1&&s_editor_camera_test_timer&&s_editor_camera_discovery_timer);
 editor_camera_test_cb(NULL);editor_camera_discover_cb(NULL);assert(camera_test_starts==tests+1&&camera_find_starts==finds+1);
 unsigned polls=camera_test_polls;lv_textarea_set_text(s_editor_camera_stream,"http://edited.local/stream");
 editor_camera_test_poll_cb(s_editor_camera_test_timer);assert(!s_editor_camera_test_timer&&camera_test_polls==polls&&strstr(lv_label_get_text(s_editor_camera_status),"changed"));
 polls=camera_find_polls;editor_camera_discovery_poll_cb(s_editor_camera_discovery_timer);assert(!s_editor_camera_discovery_timer&&camera_find_polls==polls&&!s_camera_find_job.key[0]);
 for(unsigned field=0;field<6;field++){
  camera_camera_fixture_open();strcpy(s_editor_api_key,"captured-search-key");editor_camera_discover_cb(NULL);assert(s_editor_camera_discovery_timer);
  if(field==0)lv_textarea_set_text(s_editor_host,"new-host.local");
  if(field==1)lv_textarea_set_text(s_editor_port,"7126");
  if(field==2)strcpy(s_editor_api_key,"new-key");
  if(field==3)s_editor_secure=!s_editor_secure;
  if(field==4)lv_textarea_set_text(s_editor_camera_name,"Edited Camera");
  if(field==5)config_generation++;
  polls=camera_find_polls;editor_camera_discovery_poll_cb(s_editor_camera_discovery_timer);
  assert(!s_editor_camera_discovery_timer&&camera_find_polls==polls&&!s_camera_find_job.key[0]);
 }
 camera_camera_fixture_open();camera_test_accept=false;editor_camera_test_cb(NULL);assert(!s_editor_camera_test_timer);camera_test_accept=true;
 camera_find_accept=false;editor_camera_discover_cb(NULL);assert(!s_editor_camera_discovery_timer&&!s_camera_find_job.key[0]);camera_find_accept=true;
 camera_test_start_hook=camera_parent_replace;editor_camera_test_cb(NULL);camera_test_start_hook=NULL;assert(s_editor_popup&&!s_editor_camera_test_timer);
 editor_camera_open_cb(NULL);camera_find_start_hook=camera_parent_replace;editor_camera_discover_cb(NULL);camera_find_start_hook=NULL;
 assert(s_editor_popup&&!s_editor_camera_discovery_timer&&!s_camera_find_job.key[0]);
 camera_camera_fixture_open();editor_camera_test_cb(NULL);camera_test_ready=true;camera_test_poll_hook=camera_parent_replace;
 editor_camera_test_poll_cb(s_editor_camera_test_timer);camera_test_poll_hook=NULL;assert(s_editor_popup&&!s_editor_camera_test_timer);camera_test_ready=false;
 editor_camera_open_cb(NULL);editor_camera_discover_cb(NULL);camera_find_ready=true;camera_find_poll_hook=camera_parent_replace;
 editor_camera_discovery_poll_cb(s_editor_camera_discovery_timer);camera_find_poll_hook=NULL;assert(s_editor_popup&&!s_editor_camera_discovery_timer);camera_find_ready=false;
 camera_camera_fixture_open();editor_camera_test_cb(NULL);camera_test_ready=true;camera_test_poll_hook=camera_poll_change_url;
 editor_camera_test_poll_cb(s_editor_camera_test_timer);camera_test_poll_hook=NULL;assert(!s_editor_camera_test_timer&&!strstr(lv_label_get_text(s_editor_camera_status),"verified"));
 editor_camera_test_cb(NULL);camera_test_ready=true;editor_camera_test_poll_cb(s_editor_camera_test_timer);
 assert(!s_editor_camera_test_timer&&strstr(lv_label_get_text(s_editor_camera_status),"640 x 480"));
 editor_camera_test_cb(NULL);editor_camera_discover_cb(NULL);click(s_editor_camera_slot_buttons[2]);
 assert(!s_editor_camera_test_timer&&!s_editor_camera_discovery_timer&&!s_camera_find_job.key[0]);
 /* Import can advance its own generation and must keep the staged primary URL current from any slot. */
 editor_camera_discover_cb(NULL);camera_find_ready=true;camera_find_advance=true;editor_camera_discovery_poll_cb(s_editor_camera_discovery_timer);camera_find_advance=false;
 assert(!s_editor_camera_discovery_timer&&!strcmp(s_editor_camera_url,"http://imported.local/stream")&&strstr(lv_label_get_text(s_editor_camera_status),"imported"));
 editor_camera_discover_cb(NULL);camera_find_ready=true;camera_find_found=false;editor_camera_discovery_poll_cb(s_editor_camera_discovery_timer);camera_find_found=true;
 assert(!s_editor_camera_discovery_timer&&strstr(lv_label_get_text(s_editor_camera_status),"manually"));
 editor_camera_discover_cb(NULL);lv_obj_delete(s_editor_camera_stream);assert(!s_editor_camera_discovery_timer&&!s_camera_find_job.key[0]);
 ui_printer_profiles_close_all();assert(!s_camera_save&&!s_camera_test&&!s_camera_find&&!s_camera_default&&!s_camera_cancel);
}
static void security_fixture_open(void)
{
 ui_printer_profiles_close_all();reset();ui_printer_profiles_show_for_slot(0,active_changed,discover);manager_edit_cb(NULL);
 editor_security_open_cb(NULL);
}
static void security_import_nested(void)
{
 click(s_pem_rows[1]);editor_security_standard_cb(NULL);editor_security_pem_picker_open_cb(NULL);
 editor_security_open_cb(NULL);editor_save_cb(NULL);
}
static void security_import_replace(void){editor_close();manager_edit_cb(NULL);}
static void security_replace_deleted(lv_event_t *event){(void)event;editor_security_open_cb(NULL);}
static void security_picker_replace_deleted(lv_event_t *event){(void)event;editor_security_pem_picker_open_cb(NULL);}
static void security_scan_nested(void){editor_security_pem_picker_open_cb(NULL);editor_security_standard_cb(NULL);}
static void security_scan_changed(void){config_generation++;}
static void security_scan_delete_body(void){lv_obj_delete(s_pem_body);}
static void security_import_delete_feedback(void){lv_obj_delete(s_pem_status);}
static void security_import_edit_port(void){lv_textarea_set_text(s_editor_port,"444");}
static void profile_security_safety(void)
{
 security_fixture_open();editor_security_close();
 unsigned before=imports;lv_obj_add_state(s_security_opener,LV_STATE_DISABLED);editor_security_open_cb(NULL);assert(!s_editor_security_popup);
 lv_obj_remove_state(s_security_opener,LV_STATE_DISABLED);
 lv_obj_t *fake=lv_button_create(s_editor_popup);lv_obj_add_event_cb(fake,editor_security_open_cb,LV_EVENT_CLICKED,NULL);click(fake);assert(!s_editor_security_popup);lv_obj_delete(fake);
 editor_security_open_cb(NULL);
 lv_obj_t **controls[]={&s_security_standard,&s_security_select,&s_security_cancel};
 lv_event_cb_t callbacks[]={editor_security_standard_cb,editor_security_pem_picker_open_cb,editor_security_cancel_cb};
 for(unsigned i=0;i<3;++i){
  lv_obj_add_state(*controls[i],LV_STATE_DISABLED);callbacks[i](NULL);assert(s_editor_security_popup&&!s_editor_pem_picker);
  lv_obj_remove_state(*controls[i],LV_STATE_DISABLED);
  fake=lv_button_create(s_editor_security_popup);lv_obj_add_event_cb(fake,callbacks[i],LV_EVENT_CLICKED,NULL);click(fake);lv_obj_delete(fake);
  assert(s_editor_security_popup&&!s_editor_pem_picker);
 }
 editor_security_pem_picker_open_cb(NULL);lv_obj_t *picker=s_editor_pem_picker;
 lv_obj_add_state(s_pem_rows[0],LV_STATE_DISABLED);click(s_pem_rows[0]);assert(imports==before);
 lv_obj_remove_state(s_pem_rows[0],LV_STATE_DISABLED);
 fake=lv_button_create(picker);lv_obj_add_event_cb(fake,editor_security_pem_selected_cb,LV_EVENT_CLICKED,NULL);click(fake);assert(imports==before);lv_obj_delete(fake);
 lv_obj_add_state(s_pem_close,LV_STATE_DISABLED);editor_pem_picker_close_cb(NULL);assert(s_editor_pem_picker==picker);
 lv_obj_remove_state(s_pem_close,LV_STATE_DISABLED);
 import_ok=false;click(s_pem_rows[0]);assert(imports==before+1&&s_editor_pem_picker==picker&&strstr(lv_label_get_text(s_pem_status),"failed"));layout(picker);
 editor_security_pem_picker_open_cb(NULL);assert(s_editor_pem_picker==picker&&strstr(lv_label_get_text(s_pem_status),"failed"));
 import_ok=true;import_hook=security_import_nested;lv_textarea_set_text(s_editor_port,"7125");click(s_pem_rows[1]);import_hook=NULL;
 assert(imports==before+2&&imported_profile==0&&strstr(imported_path,"_2.pem")&&s_editor_secure&&!s_editor_pem_picker&&!s_editor_security_popup);
 assert(!strcmp(lv_textarea_get_text(s_editor_port),"443")&&strstr(lv_label_get_text(s_editor_status),"save"));
 security_fixture_open();editor_security_pem_picker_open_cb(NULL);before=imports;config_generation++;click(s_pem_rows[0]);
 assert(imports==before&&strstr(lv_label_get_text(s_pem_status),"changed"));editor_pem_picker_close();
 editor_security_standard_cb(NULL);assert(s_editor_security_popup&&strstr(lv_label_get_text(s_security_status),"changed"));editor_security_cancel_cb(NULL);
 security_fixture_open();editor_security_pem_picker_open_cb(NULL);before=imports;active=1;click(s_pem_rows[0]);assert(imports==before);active=0;
 lv_obj_delete(s_pem_rows[0]);assert(!s_pem_rows[0]&&!s_editor_pem_paths[0][0]);
 lv_obj_delete(s_editor_port);click(s_pem_rows[1]);assert(imports==before&&strstr(lv_label_get_text(s_pem_status),"unavailable"));
 security_fixture_open();editor_security_pem_picker_open_cb(NULL);import_ok=true;import_hook=security_import_replace;click(s_pem_rows[0]);import_hook=NULL;
 assert(s_editor_popup&&!s_editor_security_popup&&!s_editor_pem_picker&&!s_editor_secure&&!strstr(lv_label_get_text(s_editor_status),"CA imported"));
 security_fixture_open();editor_security_pem_picker_open_cb(NULL);import_ok=false;import_hook=security_import_delete_feedback;click(s_pem_rows[0]);import_hook=NULL;
 assert(s_editor_pem_picker&&!s_pem_status&&!s_editor_secure);import_ok=true;click(s_pem_rows[0]);assert(s_editor_secure&&!s_editor_pem_picker);
 security_fixture_open();editor_security_pem_picker_open_cb(NULL);import_ok=true;import_hook=security_import_edit_port;click(s_pem_rows[0]);import_hook=NULL;
 assert(s_editor_secure&&!strcmp(lv_textarea_get_text(s_editor_port),"444"));
 security_fixture_open();editor_security_pem_picker_open_cb(NULL);lv_obj_delete(s_pem_close);editor_pem_picker_close_cb(NULL);assert(s_editor_pem_picker);
 editor_pem_picker_close();lv_obj_add_event_cb(s_editor_security_popup,security_replace_deleted,LV_EVENT_DELETE,NULL);editor_security_close();
 assert(s_editor_security_popup&&s_security_select&&s_security_editor==s_editor_popup);
 editor_security_pem_picker_open_cb(NULL);lv_obj_add_event_cb(s_editor_pem_picker,security_picker_replace_deleted,LV_EVENT_DELETE,NULL);editor_pem_picker_close();
 assert(s_editor_pem_picker&&s_pem_rows[0]&&s_editor_pem_paths[0][0]);
 lv_obj_delete(s_editor_pem_picker);assert(!s_pem_rows[0]&&!s_pem_close&&!s_editor_pem_paths[0][0]);
 editor_security_pem_picker_open_cb(NULL);lv_obj_add_event_cb(s_editor_pem_picker,security_replace_deleted,LV_EVENT_DELETE,NULL);editor_security_close();
 assert(s_editor_security_popup&&s_security_standard&&!s_editor_pem_picker);
 for(unsigned i=0;i<3;++i){security_fixture_open();lv_obj_delete(*controls[i]);callbacks[i](NULL);assert(s_editor_security_popup&&!s_editor_pem_picker);}
 security_fixture_open();editor_security_close();lv_obj_delete(s_security_opener);editor_security_open_cb(NULL);assert(!s_editor_security_popup);
 for(unsigned phase=0;phase<3;++phase){
  security_fixture_open();unsigned closed=directory_closes;
  if(phase==0)directory_open_hook=security_import_replace;
  if(phase==1)directory_read_hook=security_import_replace;
  if(phase==2)directory_close_hook=security_import_replace;
  editor_security_pem_picker_open_cb(NULL);directory_open_hook=directory_read_hook=directory_close_hook=NULL;
  assert(directory_closes==closed+1&&s_editor_popup&&!s_editor_pem_picker&&!s_editor_security_popup);
 }
 security_fixture_open();directory_open_hook=security_scan_nested;editor_security_pem_picker_open_cb(NULL);directory_open_hook=NULL;
 assert(s_editor_pem_picker&&s_pem_rows[0]&&!s_editor_secure);
 security_fixture_open();directory_read_hook=security_scan_changed;editor_security_pem_picker_open_cb(NULL);
 assert(s_editor_pem_picker&&s_pem_close&&strstr(lv_label_get_text(s_pem_status),"interrupted"));editor_pem_picker_close_cb(NULL);
 security_fixture_open();directory_read_hook=security_scan_delete_body;editor_security_pem_picker_open_cb(NULL);
 assert(s_editor_pem_picker&&!s_pem_body&&s_pem_close);editor_pem_picker_close_cb(NULL);
 security_fixture_open();directory_long_names=true;editor_security_pem_picker_open_cb(NULL);directory_long_names=false;
 assert(s_editor_pem_picker&&!s_pem_rows[0]);layout(s_editor_pem_picker);
 assert(!editor_pem_name("../ca.pem")&&!editor_pem_name("sub/ca.pem")&&editor_pem_name("CA.PEM"));
 ui_printer_profiles_close_all();assert(!s_security_opener&&!s_security_select&&!s_security_status&&!s_security_editor&&!s_pem_rows[0]&&!s_pem_close&&s_security_profile==-1);
}
static void controls_fixture_open(void)
{
 ui_printer_profiles_close_all();reset();ui_printer_profiles_show_for_slot(0,active_changed,discover);manager_edit_cb(NULL);
}
static void controls_discover_sync(void)
{
 discovered++;editor_discover_cb(NULL);editor_save_cb(NULL);manager_close_cb(NULL);
 ui_printer_profiles_set_discovered_endpoint("sync-selected.local",7126,"Selected Printer");
}
static void controls_discover_replace(void)
{
 ui_printer_profiles_close_all();ui_printer_profiles_show_for_slot(2,active_changed,discover);
 lv_textarea_set_text(s_editor_host,"replacement.local");
}
static void controls_keyboard_replace(lv_event_t *event){(void)event;click(s_editor_name);}
static void controls_keyboard_parent_replace(lv_event_t *event){(void)event;editor_close();manager_edit_cb(NULL);}
static void controls_handoff_replace(lv_obj_t *object){(void)object;controls_discover_replace();}
static void controls_handoff_delete_port(lv_obj_t *object){(void)object;lv_obj_delete(s_editor_port);}
static void controls_handoff_nested(lv_obj_t *object)
{
 (void)object;ui_printer_profiles_set_discovered_endpoint("nested.local",7129,"Nested");
 editor_save_cb(NULL);editor_auth_open_cb(NULL);editor_camera_open_cb(NULL);editor_cancel_cb(NULL);editor_discover_cb(NULL);
}
static void controls_handoff_generation(lv_obj_t *object){(void)object;config_generation++;}
static void profile_editor_control_safety(void)
{
 controls_fixture_open();unsigned found=discovered;int saved=saves;
 lv_obj_t **buttons[]={&s_editor_cancel_action,&s_editor_discover_action,&s_editor_auth_action,&s_editor_camera_action};
 lv_event_cb_t callbacks[]={editor_cancel_cb,editor_discover_cb,editor_auth_open_cb,editor_camera_open_cb};
 for(unsigned i=0;i<4;++i){
  lv_obj_add_state(*buttons[i],LV_STATE_DISABLED);callbacks[i](NULL);lv_obj_remove_state(*buttons[i],LV_STATE_DISABLED);
  lv_obj_t *fake=lv_button_create(s_editor_popup);lv_obj_add_event_cb(fake,callbacks[i],LV_EVENT_CLICKED,NULL);click(fake);lv_obj_delete(fake);
  assert(s_editor_popup&&!s_editor_auth_popup&&!s_editor_camera_popup&&(unsigned)discovered==found);
 }
 for(unsigned i=0;i<4;++i){controls_fixture_open();lv_obj_delete(*buttons[i]);callbacks[i](NULL);assert(s_editor_popup&&!s_editor_auth_popup&&!s_editor_camera_popup);}
 controls_fixture_open();s_discover_cb=controls_discover_sync;found=discovered;saved=saves;editor_discover_cb(NULL);
 assert((unsigned)discovered==found+1&&saves==saved&&s_editor_popup&&!strcmp(lv_textarea_get_text(s_editor_host),"sync-selected.local")&&!s_profile_discovery_dispatching);
 s_discover_cb=controls_discover_replace;editor_discover_cb(NULL);assert(s_editor_profile==2&&!s_profile_discovery_dispatching);
 controls_fixture_open();editor_auth_open_cb(NULL);lv_obj_add_state(s_auth_cancel_action,LV_STATE_DISABLED);editor_auth_cancel_cb(NULL);assert(s_editor_auth_popup);
 lv_obj_remove_state(s_auth_cancel_action,LV_STATE_DISABLED);lv_obj_delete(s_auth_cancel_action);editor_auth_cancel_cb(NULL);assert(s_editor_auth_popup);editor_auth_close();
 click(s_editor_host);lv_obj_t *keyboard=s_editor_keyboard_popup;
 lv_obj_add_state(s_key_cancel_action,LV_STATE_DISABLED);editor_keyboard_cancel_cb(NULL);assert(s_editor_keyboard_popup==keyboard);
 lv_obj_remove_state(s_key_cancel_action,LV_STATE_DISABLED);
 lv_obj_t *fake=lv_button_create(keyboard);lv_obj_add_event_cb(fake,editor_keyboard_cancel_cb,LV_EVENT_CANCEL,NULL);lv_obj_send_event(fake,LV_EVENT_CANCEL,NULL);assert(s_editor_keyboard_popup==keyboard);lv_obj_delete(fake);
 lv_obj_delete(s_key_cancel_action);editor_keyboard_cancel_cb(NULL);assert(s_editor_keyboard_popup==keyboard);
 lv_obj_send_event(s_editor_keyboard,LV_EVENT_CANCEL,NULL);assert(!s_editor_keyboard_popup&&!s_key_cancel_action);
 click(s_editor_host);lv_obj_add_event_cb(s_editor_keyboard_popup,controls_keyboard_replace,LV_EVENT_DELETE,NULL);click(s_editor_port);
 assert(s_editor_keyboard_popup&&s_editor_keyboard_target==s_editor_name);editor_keyboard_discard();
 click(s_editor_host);lv_obj_add_event_cb(s_editor_keyboard_popup,controls_keyboard_parent_replace,LV_EVENT_DELETE,NULL);click(s_editor_port);
 assert(s_editor_popup&&!s_editor_keyboard_popup&&s_editor_host&&s_editor_port);
 /* Caller strings can alias the fields being replaced. Copy before writing either field. */
 lv_textarea_set_text(s_editor_name,"Printer 1");
 const char *host=lv_textarea_get_text(s_editor_host),*identity=lv_textarea_get_text(s_editor_port);
 editor_test_cb(NULL);assert(s_editor_test_timer);saved=saves;found=discovered;textarea_write_hook=controls_handoff_nested;
 ui_printer_profiles_set_discovered_endpoint(host,7128,identity);
 assert(!s_editor_test_timer&&!strcmp(lv_textarea_get_text(s_editor_port),"7128")&&!strcmp(lv_textarea_get_text(s_editor_name),"7125")&&saves==saved&&(unsigned)discovered==found&&!s_editor_auth_popup&&!s_editor_camera_popup);
 textarea_write_hook=controls_handoff_replace;ui_printer_profiles_set_discovered_endpoint("old-selected.local",7129,"Old");
 assert(s_editor_profile==2&&!strcmp(lv_textarea_get_text(s_editor_host),"replacement.local")&&!strcmp(lv_textarea_get_text(s_editor_port),"7125")&&!strstr(lv_label_get_text(s_editor_status),"endpoint selected"));
 controls_fixture_open();textarea_write_hook=controls_handoff_delete_port;ui_printer_profiles_set_discovered_endpoint("selected.local",7129,"Old");assert(!s_editor_port&&s_editor_popup);
 controls_fixture_open();textarea_write_hook=controls_handoff_generation;ui_printer_profiles_set_discovered_endpoint("selected.local",7129,"Old");
 assert(!strcmp(lv_textarea_get_text(s_editor_port),"7125")&&!strstr(lv_label_get_text(s_editor_status),"endpoint selected"));
 editor_close();lv_obj_t **manager_buttons[]={&s_manager_close_action,&s_manager_edit_action};
 lv_event_cb_t manager_callbacks[]={manager_close_cb,manager_edit_action_cb};
 for(unsigned i=0;i<2;++i){
  lv_obj_add_state(*manager_buttons[i],LV_STATE_DISABLED);manager_callbacks[i](NULL);lv_obj_remove_state(*manager_buttons[i],LV_STATE_DISABLED);
  fake=lv_button_create(s_manager_popup);lv_obj_add_event_cb(fake,manager_callbacks[i],LV_EVENT_CLICKED,NULL);click(fake);lv_obj_delete(fake);
  assert(s_manager_popup&&!s_editor_popup);
 }
 for(unsigned i=0;i<2;++i){controls_fixture_open();editor_close();lv_obj_delete(*manager_buttons[i]);manager_callbacks[i](NULL);assert(s_manager_popup&&!s_editor_popup);}
 controls_fixture_open();editor_close();manager_delete_cb(NULL);manager_edit_action_cb(NULL);assert(s_delete_popup&&!s_editor_popup);delete_close();
 s_selected_profile=-1;manager_edit_cb(NULL);assert(!s_editor_popup);s_selected_profile=0;
 profile_read_hook=controls_discover_replace;manager_edit_cb(NULL);assert(s_editor_profile==2&&s_editor_popup);
 controls_fixture_open();strcpy(s_editor_camera_names[0],"Other printer camera");editor_close();s_selected_profile=1;manager_edit_cb(NULL);
 assert(strcmp(s_editor_camera_names[0],"Other printer camera")&&strstr(s_editor_camera_names[0],"Workshop camera"));
 editor_save_cb(NULL);assert(camera_set_profile==1&&strstr(camera_set_name,"Workshop camera"));
 manager_edit_cb(NULL);editor_close();camera_get_hook=controls_discover_replace;manager_edit_cb(NULL);assert(s_editor_profile==2);
 controls_fixture_open();editor_close();profile_read_hook=controls_discover_replace;click(s_profile_rows[0]);
 assert(s_editor_profile==2&&!strstr(lv_label_get_text(s_manager_status),"Selected: Workshop"));
 ui_printer_profiles_close_all();assert(!s_manager_close_action&&!s_manager_edit_action&&!s_editor_cancel_action&&!s_editor_discover_action&&!s_editor_auth_action&&!s_editor_camera_action&&!s_auth_cancel_action&&!s_key_cancel_action);
}
static uint32_t captured_discovery_request;
static void capture_discovery_request(void){captured_discovery_request=ui_printer_profiles_discovery_request();assert(captured_discovery_request);}
static void deliver_discovery_request(void)
{
 capture_discovery_request();assert(ui_printer_profiles_apply_discovered_endpoint(captured_discovery_request,"sync-request.local",7126,"Selected"));
}
static void profile_discovery_request_safety(void)
{
 controls_fixture_open();s_discover_cb=capture_discovery_request;editor_discover_cb(NULL);uint32_t old=captured_discovery_request;
 editor_close();s_selected_profile=1;manager_edit_cb(NULL);
 assert(!ui_printer_profiles_apply_discovered_endpoint(old,"wrong.local",7129,"Wrong")&&!strcmp(lv_textarea_get_text(s_editor_host),"printer-b.local"));
 editor_discover_cb(NULL);uint32_t current=captured_discovery_request;assert(current&&current!=old);
 assert(!ui_printer_profiles_apply_discovered_endpoint(old,"wrong.local",7129,"Wrong")&&ui_printer_profiles_discovery_request()==current);
 ui_printer_profiles_cancel_discovery_request(old);assert(ui_printer_profiles_discovery_request()==current);
 assert(!ui_printer_profiles_apply_discovered_endpoint(current,"",0,"Wrong")&&ui_printer_profiles_discovery_request()==current);
 assert(ui_printer_profiles_apply_discovered_endpoint(current,"selected.local",7128,"Selected"));
 assert(!strcmp(lv_textarea_get_text(s_editor_host),"selected.local")&&!ui_printer_profiles_discovery_request());
 assert(!ui_printer_profiles_apply_discovered_endpoint(current,"wrong.local",7129,"Wrong"));
 for(unsigned field=0;field<7;++field){
  controls_fixture_open();s_discover_cb=capture_discovery_request;editor_discover_cb(NULL);current=captured_discovery_request;
  if(field==0)lv_textarea_set_text(s_editor_host,"edited.local");
  if(field==1)lv_textarea_set_text(s_editor_port,"7126");
  if(field==2)lv_textarea_set_text(s_editor_name,"My edited name");
  if(field==3)config_generation++;
  if(field==4)active=1;
  if(field==5)lv_obj_delete(s_editor_host);
  if(field==6){editor_close();manager_edit_cb(NULL);}
  assert(!ui_printer_profiles_apply_discovered_endpoint(current,"wrong.local",7129,"Wrong")&&!ui_printer_profiles_discovery_request());
 }
 controls_fixture_open();s_discover_cb=deliver_discovery_request;editor_discover_cb(NULL);
 assert(!strcmp(lv_textarea_get_text(s_editor_host),"sync-request.local")&&!ui_printer_profiles_discovery_request());
 s_discover_cb=capture_discovery_request;editor_discover_cb(NULL);current=captured_discovery_request;
 s_profile_dispatching=true;assert(!ui_printer_profiles_apply_discovered_endpoint(current,"wrong.local",7129,"Wrong"));s_profile_dispatching=false;
 assert(ui_printer_profiles_discovery_request()==current);
 textarea_write_hook=controls_handoff_replace;
 assert(!ui_printer_profiles_apply_discovered_endpoint(current,"selected.local",7129,"Selected"));
 assert(s_editor_profile==2&&!strcmp(lv_textarea_get_text(s_editor_host),"replacement.local")&&!strcmp(lv_textarea_get_text(s_editor_port),"7125"));
 s_discover_cb=capture_discovery_request;editor_discover_cb(NULL);current=captured_discovery_request;
 ui_printer_profiles_set_discovered_endpoint("legacy-selected.local",7127,"Legacy");
 assert(!ui_printer_profiles_apply_discovered_endpoint(current,"wrong.local",7129,"Wrong"));
 editor_discover_cb(NULL);current=captured_discovery_request;ui_printer_profiles_cancel_discovery_request(current);
 assert(!ui_printer_profiles_apply_discovered_endpoint(current,"wrong.local",7129,"Wrong")&&!ui_printer_profiles_discovery_request());
 ui_printer_profiles_close_all();assert(!ui_printer_profiles_discovery_request());
}
static bool modal_pointer_pressed;
static lv_point_t modal_pointer_point={6,6};
static void modal_pointer_read(lv_indev_t *input,lv_indev_data_t *data)
{
 (void)input;data->point=modal_pointer_point;data->state=modal_pointer_pressed?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED;
}
static void modal_pointer_tap(lv_indev_t *input)
{
 lv_obj_update_layout(lv_screen_active());
 modal_pointer_pressed=true;lv_indev_read(input);modal_pointer_pressed=false;lv_indev_read(input);
}
static void profile_modal_layers(void)
{
 reset();unsigned hits=0;
 lv_obj_t *underlay=lv_button_create(lv_screen_active());lv_obj_remove_style_all(underlay);
 lv_obj_set_pos(underlay,0,0);lv_obj_set_size(underlay,12,12);
 lv_obj_add_event_cb(underlay,count_scroll_draw,LV_EVENT_CLICKED,&hits);
 lv_indev_t *input=lv_indev_create();lv_indev_set_type(input,LV_INDEV_TYPE_POINTER);lv_indev_set_read_cb(input,modal_pointer_read);
 lv_obj_t *launcher=ui_popup_create_on_screen(viewport-32,lv_display_get_vertical_resolution(NULL)-32,UI_POPUP_STANDARD);
 ui_printer_profiles_show(active_changed,discover);manager_edit_cb(NULL);
 assert(lv_obj_get_parent(s_manager_popup)==lv_screen_active());
 assert(lv_obj_get_parent(s_editor_popup)==lv_screen_active());
 assert(lv_obj_get_index(launcher)<lv_obj_get_index(s_manager_popup));
 assert(lv_obj_get_index(s_manager_popup)<lv_obj_get_index(s_editor_popup));
 modal_pointer_tap(input);assert(hits==0);
 editor_camera_open_cb(NULL);assert(s_editor_camera_popup && lv_obj_get_parent(s_editor_camera_popup)==lv_screen_active());
 assert(lv_obj_get_index(s_editor_camera_popup)>lv_obj_get_index(s_editor_popup));
 lv_obj_update_layout(s_editor_camera_popup);lv_area_t selected_slot;
 lv_obj_get_coords(s_editor_camera_slot_buttons[s_editor_camera_slot],&selected_slot);
 modal_pointer_point=(lv_point_t){(selected_slot.x1+selected_slot.x2)/2,(selected_slot.y1+selected_slot.y2)/2};
 modal_pointer_tap(input);assert(lv_obj_has_state(s_editor_camera_slot_buttons[s_editor_camera_slot],LV_STATE_CHECKED));
 modal_pointer_tap(input);assert(lv_obj_has_state(s_editor_camera_slot_buttons[s_editor_camera_slot],LV_STATE_CHECKED));
 modal_pointer_point=(lv_point_t){6,6};modal_pointer_tap(input);assert(hits==0);
 click(s_editor_camera_name);assert(s_editor_keyboard_popup && lv_obj_get_parent(s_editor_keyboard_popup)==lv_layer_top());
 modal_pointer_tap(input);assert(hits==0);editor_camera_close();assert(s_editor_popup&&!s_editor_keyboard_popup);
 editor_auth_open_cb(NULL);assert(s_editor_auth_popup && lv_obj_get_parent(s_editor_auth_popup)==lv_layer_top());
 modal_pointer_tap(input);assert(hits==0);editor_auth_close();assert(s_editor_popup);
 ui_printer_profiles_close_all();lv_obj_delete(launcher);
 modal_pointer_tap(input);assert(hits==1);
 lv_indev_delete(input);lv_obj_delete(underlay);
}
int main(void){lv_init();lv_display_t*d=lv_display_create(1024,600);static uint8_t buf[1024*40*2];lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);lv_display_set_buffers(d,buf,NULL,sizeof(buf),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(d,flush);
if(getenv("PROFILE_SCROLL_DIAGNOSTIC")){
 for(int theme=0;theme<5;theme++){
  ui_theme_set_active((ui_theme_id_t)theme);ui_theme_set_density(UI_DENSITY_COMFORTABLE);ui_theme_set_accessibility((ui_accessibility_t){0});reset();
  ui_printer_profiles_show(active_changed,discover);manager_edit_cb(NULL);profile_scroll_render_check(s_editor_popup,true);ui_printer_profiles_close_all();
 }
 lv_display_delete(d);lv_deinit();return 0;
}
for(int size=0;size<3;size++)for(int theme=0;theme<5;theme++)for(int density=0;density<3;density++)for(int large=0;large<2;large++){
viewport=(int[]){1024,640,480}[size];lv_display_set_resolution(d,viewport,size==2?400:600);ui_theme_set_active(theme);ui_theme_set_density(density);ui_theme_set_accessibility((ui_accessibility_t){.large_text=large});reset();int old_saves=saves,old_deletes=deletes,old_changed=changed;
ui_printer_profiles_show(active_changed,discover);nested_checks(theme,!size&&density==1&&large);layout(s_manager_popup);if(ui_theme_is_studio())assert(lv_obj_get_width(s_manager_popup)==viewport-48);for(int i=0;i<4;i++){lv_obj_t*r=s_profile_rows[i];for(unsigned j=0;j<lv_obj_get_child_count(r);j++)inside(lv_obj_get_child(r,j),r);}manager_polish_checks();if(!size&&density==1&&large)snapshot(theme,"manager");
click(s_profile_rows[2]);assert(s_selected_profile==2);manager_polish_checks();manager_select_cb(NULL);assert(active==0&&changed==old_changed);manager_delete_cb(NULL);assert(!s_delete_popup);
click(s_profile_rows[0]);manager_polish_checks();manager_edit_cb(NULL);layout(s_editor_popup);editor_polish_checks();profile_scroll_render_check(s_editor_popup,true);lv_obj_t*fields[]={s_editor_name,s_editor_host,s_editor_port};for(unsigned i=0;i<3;i++){assert(lv_obj_get_height(fields[i])>=56);assert(lv_obj_get_style_text_font(fields[i],0)==UI_FONT_BODY_LARGE);}assert(!strcmp(lv_textarea_get_text(s_editor_name),profiles[0].name));if(!size&&density==1&&large)snapshot(theme,"editor");
int old_discovered=discovered;editor_discover_cb(NULL);assert(discovered==old_discovered+1);lv_textarea_set_text(s_editor_host,"");editor_save_cb(NULL);assert(s_editor_popup&&saves==old_saves);lv_textarea_set_text(s_editor_host,"changed.local");lv_textarea_set_text(s_editor_port,"0");editor_save_cb(NULL);assert(s_editor_popup&&saves==old_saves);lv_textarea_set_text(s_editor_port,"7127");s_editor_secure=true;editor_save_cb(NULL);assert(s_editor_popup&&saves==old_saves);s_editor_secure=false;storage_ok=false;editor_save_cb(NULL);assert(s_editor_popup&&saves==old_saves);storage_ok=true;
ui_printer_profiles_set_discovered_endpoint("discovered.local",7128,"Moonraker");assert(!strcmp(lv_textarea_get_text(s_editor_host),"discovered.local"));assert(!strcmp(lv_textarea_get_text(s_editor_name),profiles[0].name));editor_save_cb(NULL);assert(!s_editor_popup&&saves==old_saves+1&&changed==old_changed+1);assert(!strcmp(profiles[0].host,"discovered.local")&&profiles[0].port==7128);
manager_edit_cb(NULL);lv_textarea_set_text(s_editor_host,"cancelled.local");editor_cancel_cb(NULL);assert(!s_editor_popup&&saves==old_saves+1);assert(strcmp(profiles[0].host,"cancelled.local"));
manager_delete_cb(NULL);assert(s_delete_popup);layout(s_delete_popup);if(!size&&density==1&&large)snapshot(theme,"remove");delete_cancel_cb(NULL);assert(!s_delete_popup&&deletes==old_deletes);manager_delete_cb(NULL);storage_ok=false;delete_confirm_cb(NULL);assert(s_delete_popup&&deletes==old_deletes);storage_ok=true;manager_delete_cb(NULL);delete_confirm_cb(NULL);assert(!s_delete_popup&&deletes==old_deletes+1&&active==1&&changed==old_changed+2);manager_delete_cb(NULL);assert(!s_delete_popup);
ui_printer_profiles_close_all();assert(!s_manager_popup&&!s_manager_list&&!s_editor_popup&&!s_active_changed_cb);reset();ui_printer_profiles_show_for_slot(2,active_changed,discover);assert(s_editor_popup&&s_editor_profile==2);layout(s_editor_popup);editor_test_cb(NULL);assert(!s_editor_test_timer);lv_textarea_set_text(s_editor_host,"test.local");editor_test_cb(NULL);assert(s_editor_test_timer);editor_camera_open_cb(NULL);editor_camera_test_cb(NULL);editor_camera_discover_cb(NULL);assert(discovered_profile==2);config_generation++;editor_camera_test_poll_cb(s_editor_camera_test_timer);editor_camera_discovery_poll_cb(s_editor_camera_discovery_timer);assert(!s_editor_camera_test_timer&&!s_editor_camera_discovery_timer&&strstr(lv_label_get_text(s_editor_camera_status),"changed"));lv_obj_delete(s_editor_popup);assert(!s_editor_popup&&!s_editor_test_timer&&!s_editor_host);manager_edit_cb(NULL);lv_obj_delete(s_manager_popup);assert(!s_editor_popup&&!s_manager_popup&&!s_manager_list);profile_draft_safety();profile_persistence_safety();profile_camera_removal_safety();profile_camera_operation_safety();profile_security_safety();profile_editor_control_safety();profile_discovery_request_safety();
}
profile_modal_layers();lv_display_delete(d);lv_deinit();puts("PASS: camera slot grid/default/selected/action fit and opaque redraw; grouped editor fits panel, host/port alignment and five-digit port; modal native-input blocking/layer order and editor scroll damage skips parent redraws with pinned footer; flat manager rows, separate Active/selected markers, adaptive Add/Edit label and four pinned actions; profile manager/editor/remove layouts in five themes, three densities, both text sizes and three viewports; pinned actions, scroll reach, large fields, selection, discovery, save validation/failure/cancel, remove guards, authentication/security/certificate/camera/keyboard layouts and behavior, numeric/password/length limits, camera slot routing/default/removal and teardown");}
