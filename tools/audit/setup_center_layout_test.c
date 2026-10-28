/* Real LVGL Setup Center; transport and persistence are typed fixtures. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
size_t strlcpy(char*,const char*,size_t);
#include "ui_setup_wizard.c"
#include "keyboard_layout_checks.h"
#include "custom_theme.h"
#include "ui_page_layout_profile.h"
bool custom_theme_color_override(uint32_t a,uint32_t b,uint32_t c,uint32_t*d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_metric_override(int32_t a,int32_t b,int32_t c,int32_t*d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_accent_override(uint8_t a,uint32_t*b){(void)a;(void)b;return false;}
bool custom_theme_surface_opacity(uint8_t*a){(void)a;return false;}
const ui_page_layout_profile_t *custom_theme_page_profile(void){return NULL;}
size_t strlcpy(char*d,const char*s,size_t n){size_t z=strlen(s);if(n){size_t k=z<n-1?z:n-1;memcpy(d,s,k);d[k]=0;}return z;}
static size_t selected_zone;
static int brightness=80;
static uint8_t sleep_minutes=5;
static bool preference_ok=true;
static unsigned preference_writes,appearance_rebuilds;
static void (*preference_hook)(void);
static timezone_config_entry_t zones[]={
 {.label="UTC"},{.label="Atlantic Time (US/Canada)"},{.label="Eastern Time (US/Canada)"},
 {.label="Central Time (US/Canada)"},{.label="Mountain Time (US/Canada)"},{.label="Arizona"},
 {.label="Pacific Time (US/Canada)"},{.label="Alaska"},{.label="Hawaii"},{.label="United Kingdom"},
 {.label="Central Europe"},{.label="India"},{.label="China"},{.label="Japan"},
 {.label="Australia Eastern"},{.label="New Zealand"}};
size_t timezone_config_count(void){return sizeof(zones)/sizeof(zones[0]);}
const timezone_config_entry_t *timezone_config_entry(size_t i){return i==1?NULL:i<timezone_config_count()?&zones[i]:NULL;}
size_t timezone_config_selected_index(void){return selected_zone;}
const char *timezone_config_selected_label(void){return zones[selected_zone].label;}
bool timezone_config_select(size_t i){assert(i<timezone_config_count()&&i!=1);preference_writes++;if(preference_hook)preference_hook();if(preference_ok)selected_zone=i;return preference_ok;}
ui_theme_id_t theme_manager_active(void){return ui_theme_get_active();}
const char *theme_manager_active_label(void){return ui_theme_name(ui_theme_get_active());}
bool theme_manager_select(ui_theme_id_t theme){preference_writes++;if(preference_hook)preference_hook();if(preference_ok)ui_theme_set_active(theme);return preference_ok;}
ui_accessibility_t theme_manager_accessibility(void){return ui_theme_get_accessibility();}
bool theme_manager_set_accessibility(ui_accessibility_t options){preference_writes++;if(preference_hook)preference_hook();if(preference_ok)ui_theme_set_accessibility(options);return preference_ok;}
int ui_settings_brightness_percent(void){return brightness;}
uint8_t ui_settings_sleep_timeout_minutes(void){return sleep_minutes;}
bool ui_settings_restore_display_preferences(int b,uint8_t sleep){assert(b>=10&&b<=100);preference_writes++;if(preference_hook)preference_hook();if(preference_ok){brightness=b;sleep_minutes=sleep;}return preference_ok;}
static void appearance_rebuilt(void){appearance_rebuilds++;}
static bool wifi_ready,configured,camera_ready,storage_ok=true;
static bool setup_complete,progress_ok=true;
static unsigned completed_steps;
static bool camera_result,discovery_result;
static unsigned editor_opened,connections,completions;
static int active_slot,editor_slot;
static bool address_ready=true;
static void (*address_read_hook)(void);
static void (*wifi_read_hook)(void);
static unsigned wifi_read_countdown;
static unsigned default_saves,url_saves,camera_starts,camera_scans;
static size_t saved_camera_slot;
static bool camera_discover_ok=true,camera_test_ok=true,custom_camera_url;
static void (*catalog_read_hook)(void);
static bool wifi_accept=true,default_ok=true,url_ok=true;
static void (*connect_hook)(void),(*default_hook)(void),(*url_hook)(void),(*completion_hook)(void);
static void (*scan_task)(void*);
static moonraker_profile_t profile={.host="printer.local",.port=7125};
const moonraker_profile_t *moonraker_config_profile(int i){(void)i;profile.configured=configured;return &profile;}
int moonraker_config_active_profile_index(void){return active_slot;}
const char *moonraker_config_camera_stream_url(int i){(void)i;return camera_ready?(custom_camera_url?"http://user:PRIVATE-SECRET@camera.local/stream":"http://camera.local/stream"):"";}
bool moonraker_config_set_camera_stream_url(int i,const char*u){(void)i;assert(u[0]);url_saves++;if(url_hook)url_hook();if(url_ok)camera_ready=true;return url_ok;}
bool onboarding_controller_should_show(void){return !setup_complete;}
bool onboarding_controller_step_complete(onboarding_step_t step){return (unsigned)step<ONBOARDING_STEP_COUNT&&(setup_complete||(completed_steps&(1U<<step)));}
bool onboarding_controller_mark_step_complete(onboarding_step_t step){if((unsigned)step>=ONBOARDING_STEP_COUNT||!progress_ok)return false;completed_steps|=1U<<step;return true;}
bool onboarding_controller_mark_complete(void){if(completion_hook)completion_hook();if(storage_ok){completions++;setup_complete=true;}return storage_ok;}
void ui_printer_profiles_show_for_slot(int slot,void(*changed)(void),void(*discover)(void)){assert(slot==active_slot&&changed&&discover);editor_slot=slot;editor_opened++;}
static uint32_t setup_request=41,setup_shown_request,setup_applied_request,setup_closed_request;
static moonraker_discovery_select_request_cb_t setup_select_request;
static bool setup_show_ok=true;
static unsigned setup_scan_starts;
uint32_t ui_printer_profiles_discovery_request(void){return setup_request;}
void ui_printer_profiles_cancel_discovery_request(uint32_t r){if(r==setup_request)setup_request=0;}
bool ui_printer_profiles_apply_discovered_endpoint(uint32_t r,const char*h,int p,const char*i){assert(h[0]&&p>0&&i[0]);setup_applied_request=r;return r==setup_request;}
void moonraker_discovery_close_request(uint32_t r){setup_closed_request=r;}
bool moonraker_discovery_show_in_parent_for_request(lv_obj_t*p,const char*t,moonraker_discovery_close_request_cb_t c,moonraker_discovery_select_request_cb_t s,uint32_t r){assert(p&&t&&c&&s&&r);setup_shown_request=r;setup_select_request=s;return setup_show_ok;}
void moonraker_discovery_show_in_parent(lv_obj_t*p,const char*t,moonraker_discovery_close_cb_t c,moonraker_discovery_select_cb_t s){(void)p;(void)t;(void)c;(void)s;}
bool moonraker_discovery_start(const esp_ip4_addr_t *ip){(void)ip;setup_scan_starts++;return true;}
esp_netif_t *esp_netif_get_handle_from_ifkey(const char*s){(void)s;return &profile;}
esp_err_t esp_netif_get_ip_info(esp_netif_t*n,esp_netif_ip_info_t*i){(void)n;if(address_read_hook){void(*hook)(void)=address_read_hook;address_read_hook=NULL;hook();}i->ip.addr=address_ready?0x7b4400c0:0;return ESP_OK;}
static uint32_t config_generation=1;
uint32_t moonraker_config_generation(void){return config_generation;}
static bool wrong_ssid;
esp_err_t esp_wifi_sta_get_ap_info(wifi_ap_record_t*i){if(wifi_read_hook&&wifi_read_countdown&&!--wifi_read_countdown){void(*hook)(void)=wifi_read_hook;wifi_read_hook=NULL;hook();}memset(i,0,sizeof(*i));snprintf((char*)i->ssid,sizeof(i->ssid),"%s",wrong_ssid?"Previous network":"Workshop_long_network_name_0");return wifi_ready?ESP_OK:ESP_FAIL;}
esp_err_t esp_wifi_scan_start(void*p,bool b){(void)p;(void)b;return ESP_OK;}
esp_err_t esp_wifi_scan_get_ap_num(uint16_t*n){*n=8;return ESP_OK;}
esp_err_t esp_wifi_scan_get_ap_records(uint16_t*n,wifi_ap_record_t*a){for(unsigned i=0;i<*n;i++){snprintf((char*)a[i].ssid,sizeof(a[i].ssid),"Workshop_long_network_name_%u",i);a[i].rssi=-57-(int)i;}return ESP_OK;}
void vTaskDelay(unsigned n){(void)n;}
void vTaskDelete(void*p){(void)p;}
int xTaskCreate(void(*f)(void*),const char*n,unsigned z,void*p,unsigned pr,void*h){(void)n;(void)z;(void)p;(void)pr;(void)h;scan_task=f;return pdPASS;}
size_t camera_catalog_default(int profile){(void)profile;if(catalog_read_hook){void(*hook)(void)=catalog_read_hook;catalog_read_hook=NULL;hook();}return saved_camera_slot;}
bool camera_catalog_get(int p,size_t i,camera_catalog_entry_t*e){(void)p;*e=(camera_catalog_entry_t){.configured=true};snprintf(e->name,sizeof(e->name),"Workshop camera %zu",i);strlcpy(e->stream_url,"http://camera.local/stream",sizeof(e->stream_url));return true;}
bool camera_catalog_set_default(int p,size_t i){(void)p;assert(i<4);default_saves++;if(default_hook)default_hook();if(default_ok)saved_camera_slot=i;return default_ok;}
bool camera_discovery_start(const char*h,int p,const char*k){(void)h;(void)p;(void)k;camera_scans++;return camera_discover_ok;}
bool camera_discovery_busy(void){return false;}
bool camera_discovery_take_result(moonraker_webcam_t*w,bool*f,size_t*n){(void)w;*f=discovery_result;*n=discovery_result?4:0;if(discovery_result)config_generation++;return true;}
bool camera_test_start(const char*u){assert(u[0]);camera_starts++;return camera_test_ok;}
bool camera_test_take_result(bool*o,int*w,int*h,size_t*b){*o=camera_result;*w=900;*h=520;*b=4096;return true;}
static bool connect_wifi(const char*s,const char*p){assert(s[0]);assert(!strcmp(p,"test-password"));connections++;if(connect_hook)connect_hook();assert(s[0]&&!strcmp(p,"test-password"));return wifi_accept;}
static bool wrong_connect(const char*s,const char*p){(void)s;(void)p;assert(!"Repeated Show replaced the original sender");return false;}
static bool contains_text(lv_obj_t *root,const char *text)
{
 if(lv_obj_check_type(root,&lv_label_class)&&strstr(lv_label_get_text(root),text))return true;
 for(unsigned i=0;i<lv_obj_get_child_count(root);i++)if(contains_text(lv_obj_get_child(root,i),text))return true;
 return false;
}
static void click(lv_obj_t *object){lv_obj_send_event(object,LV_EVENT_CLICKED,NULL);}
static void open_password(void)
{
 setup_select_wifi_cb(NULL);center_wifi_cb(NULL);scan_task(NULL);wifi_scan_ready(s_poll);
 click(s_wifi_rows[0]);lv_textarea_set_text(s_password,"test-password");
}
static void open_verified_camera(void)
{
 configured=true;setup_select_camera_cb(NULL);center_camera_cb(NULL);discovery_result=true;
 camera_discovery_poll(s_poll);click(s_camera_rows[0]);camera_result=true;camera_test_poll(s_poll);
 assert(s_camera_use_action&&s_camera_verified);
}
static void nested_connect(void){wifi_save_cb(NULL);setup_select_complete_cb(NULL);}
static void delete_password(void){lv_obj_delete(s_password);}
static void reopen_setup(void){ui_setup_wizard_close();ui_setup_wizard_show(connect_wifi);}
static void nested_camera_save(void){camera_save_cb(NULL);setup_select_wifi_cb(NULL);}
static void nested_finish(void){finish_cb(NULL);}
static void reopen_on_delete(lv_event_t *event){(void)event;ui_setup_wizard_show(connect_wifi);}
static void unrelated_timer(lv_timer_t *timer){(void)timer;}
static void safety_checks(void)
{
 wifi_ready=configured=camera_ready=false;wifi_accept=true;default_ok=url_ok=true;
 ui_setup_wizard_show(connect_wifi);open_password();
 ui_setup_wizard_show(wrong_connect); /* Existing credentials keep their sender. */
 unsigned before=connections;connect_hook=nested_connect;wifi_accept=false;wifi_save_cb(NULL);
 connect_hook=NULL;assert(connections==before+1&&!s_poll&&s_password&&s_setup_step==SETUP_STEP_WIFI);
 assert(strstr(lv_label_get_text(s_status),"Could not start"));
 lv_obj_add_state(s_wifi_connect_action,LV_STATE_DISABLED);wifi_save_cb(NULL);assert(connections==before+1);
 lv_obj_remove_state(s_wifi_connect_action,LV_STATE_DISABLED);
 lv_obj_t *fake=lv_button_create(s_actions);lv_obj_add_event_cb(fake,wifi_save_cb,LV_EVENT_CLICKED,NULL);
 click(fake);assert(connections==before+1);lv_obj_delete(fake);
 lv_obj_delete(s_status);assert(!s_status);wifi_save_cb(NULL);assert(connections==before+2&&!s_poll);
 wifi_accept=true;connect_hook=delete_password;wifi_save_cb(NULL);connect_hook=NULL;
 assert(connections==before+3&&!s_password&&!s_poll&&!lv_keyboard_get_textarea(s_keyboard));
 wifi_save_cb(NULL);assert(connections==before+3);
 open_password();connect_hook=reopen_setup;wifi_save_cb(NULL);connect_hook=NULL;
 assert(s_center&&s_stage&&s_setup_step==SETUP_STEP_WELCOME&&!s_poll&&!s_password);
 open_password();lv_obj_delete(s_wifi_connect_action);before=connections;wifi_save_cb(NULL);assert(connections==before);
 open_password();wifi_save_cb(NULL);assert(s_poll);
 lv_timer_t *unrelated=lv_timer_create(unrelated_timer,60000,NULL),*current=s_poll;
 wifi_return_timer(unrelated);wifi_verify(unrelated);wifi_scan_ready(unrelated);
 assert(s_poll==current);lv_timer_delete(unrelated);
 lv_obj_delete(s_setup_content);assert(!s_setup_content&&!s_poll);ui_setup_wizard_close();
 ui_setup_wizard_show(connect_wifi);open_verified_camera();
 unsigned defaults=default_saves,urls=url_saves;
 lv_obj_add_state(s_camera_use_action,LV_STATE_DISABLED);camera_save_cb(NULL);assert(default_saves==defaults);
 lv_obj_remove_state(s_camera_use_action,LV_STATE_DISABLED);
 default_ok=false;camera_save_cb(NULL);assert(default_saves==defaults+1&&url_saves==urls&&s_camera_use_action);
 assert(strstr(lv_label_get_text(s_status),"default camera"));default_ok=true;url_ok=false;
 default_hook=nested_camera_save;camera_save_cb(NULL);default_hook=NULL;
 assert(default_saves==defaults+2&&url_saves==urls+1&&s_camera_use_action&&s_setup_step==SETUP_STEP_CAMERA);
 assert(strstr(lv_label_get_text(s_status),"stream URL"));url_ok=true;
 default_hook=reopen_setup;camera_save_cb(NULL);default_hook=NULL;
 assert(s_center&&s_setup_step==SETUP_STEP_WELCOME&&!s_poll&&url_saves==urls+1);
 open_verified_camera();lv_obj_delete(s_camera_use_action);defaults=default_saves;camera_save_cb(NULL);
 assert(default_saves==defaults);open_verified_camera();url_hook=reopen_setup;camera_save_cb(NULL);url_hook=NULL;
 assert(s_center&&s_setup_step==SETUP_STEP_WELCOME&&!s_poll);
 setup_select_camera_cb(NULL);center_camera_cb(NULL);camera_discovery_poll(s_poll);
 click(s_camera_rows[0]);unsigned starts=camera_starts;int selected=s_selected_camera;
 click(s_camera_rows[1]);assert(s_selected_camera==selected&&camera_starts==starts);
 camera_result=false;camera_test_poll(s_poll);assert(!s_camera_verified&&!s_camera_use_action);
 camera_save_cb(NULL);assert(default_saves==defaults+1);
 wifi_ready=configured=true;setup_select_complete_cb(NULL);before=completions;
 completion_hook=nested_finish;finish_cb(NULL);completion_hook=NULL;
 assert(completions==before+1&&!s_center&&s_complete_popup);setup_completion_done_cb(NULL);
 ui_setup_wizard_show(connect_wifi);setup_select_complete_cb(NULL);before=completions;
 completion_hook=reopen_setup;finish_cb(NULL);completion_hook=NULL;
 assert(completions==before+1&&s_center&&!s_complete_popup&&s_setup_step==SETUP_STEP_WELCOME);
 setup_select_complete_cb(NULL);lv_obj_delete(s_finish_action);before=completions;finish_cb(NULL);assert(completions==before);
 lv_obj_add_event_cb(s_center,reopen_on_delete,LV_EVENT_DELETE,NULL);ui_setup_wizard_close();
 assert(s_center&&s_setup_content&&s_stage&&s_wifi_connect==connect_wifi);
 ui_setup_wizard_close();assert(!s_center&&!s_poll);
 wifi_save_cb(NULL);camera_save_cb(NULL);finish_cb(NULL);wifi_scan_open();camera_open();
 assert(!s_center&&!s_poll&&!lv_obj_get_child_count(lv_layer_top()));
}
static unsigned setup_timer_count(void)
{
 unsigned count=0;for(lv_timer_t *timer=lv_timer_get_next(NULL);timer;timer=lv_timer_get_next(timer))++count;return count;
}
static void replace_during_appearance_save(void)
{
 preference_hook=NULL;ui_setup_wizard_close();
 ui_setup_wizard_show_with_appearance_callback(connect_wifi,appearance_rebuilt);setup_select_display_cb(NULL);
 lv_obj_t *row=lv_obj_get_child(lv_obj_get_child(s_stage,1),1);click(lv_obj_get_child(row,0));
}
static void appearance_cancel_checks(void)
{
 unsigned rebuilds=appearance_rebuilds;
 for(unsigned mode=0;mode<3;mode++){
  ui_setup_wizard_show_with_appearance_callback(connect_wifi,appearance_rebuilt);
  setup_select_display_cb(NULL);assert(s_setup_step==SETUP_STEP_DISPLAY);
  unsigned writes=preference_writes;
  lv_obj_t *row=lv_obj_get_child(lv_obj_get_child(s_stage,0),1);
  click(lv_obj_get_child(row,0));click(lv_obj_get_child(row,1));
  assert(s_setup_dispatching);
  if(mode==0)ui_setup_wizard_close();
  else if(mode==1)lv_obj_delete(s_center);
  else lv_obj_delete(s_stage);
  /* Cancelling an unsaved deferred choice must immediately release the UI. */
  assert(!s_setup_dispatching);
  lv_tick_inc(50);lv_timer_handler();
  assert(preference_writes==writes&&appearance_rebuilds==rebuilds);
  if(mode==2){setup_select_wifi_cb(NULL);assert(s_stage&&s_setup_step==SETUP_STEP_WIFI);ui_setup_wizard_close();}
  ui_setup_wizard_show_with_appearance_callback(connect_wifi,appearance_rebuilt);
  setup_select_timezone_cb(NULL);assert(s_setup_step==SETUP_STEP_TIMEZONE);
  setup_select_display_cb(NULL);row=lv_obj_get_child(lv_obj_get_child(s_stage,0),1);
  unsigned timers=setup_timer_count();
  click(lv_obj_get_child(row,1));lv_tick_inc(50);lv_timer_handler();
  assert(preference_writes==writes+1&&appearance_rebuilds==++rebuilds);
  assert(s_center&&s_setup_step==SETUP_STEP_DISPLAY&&!s_setup_dispatching&&setup_timer_count()==timers);
  ui_setup_wizard_close();
 }
 /* A stale callback must not consume a newer owner's queued choice. */
 ui_setup_wizard_show_with_appearance_callback(connect_wifi,appearance_rebuilt);setup_select_display_cb(NULL);
 uint32_t old_epoch=s_setup_epoch;lv_obj_t *row=lv_obj_get_child(lv_obj_get_child(s_stage,0),1);click(lv_obj_get_child(row,0));
 ui_setup_wizard_close();ui_setup_wizard_show_with_appearance_callback(connect_wifi,appearance_rebuilt);setup_select_display_cb(NULL);
 row=lv_obj_get_child(lv_obj_get_child(s_stage,0),1);unsigned writes=preference_writes;click(lv_obj_get_child(row,1));
 setup_appearance_async((void*)(uintptr_t)old_epoch);
 assert(s_setup_dispatching&&preference_writes==writes);
 lv_tick_inc(50);lv_timer_handler();assert(preference_writes==writes+1&&appearance_rebuilds==rebuilds+1&&!s_setup_dispatching);
 ui_setup_wizard_close();
 /* Returning persistence must not unlock a replacement's pending rebuild. */
 ui_setup_wizard_show_with_appearance_callback(connect_wifi,appearance_rebuilt);setup_select_display_cb(NULL);
 row=lv_obj_get_child(lv_obj_get_child(s_stage,0),1);writes=preference_writes;rebuilds=appearance_rebuilds;
 click(lv_obj_get_child(row,0));old_epoch=s_setup_epoch;preference_hook=replace_during_appearance_save;
 setup_appearance_async((void*)(uintptr_t)old_epoch);
 assert(preference_writes==writes+1&&s_setup_dispatching&&s_setup_step==SETUP_STEP_DISPLAY);
 setup_select_wifi_cb(NULL);assert(s_setup_step==SETUP_STEP_DISPLAY);
 lv_tick_inc(50);lv_timer_handler();assert(preference_writes==writes+2&&appearance_rebuilds==rebuilds+1&&!s_setup_dispatching);
 ui_setup_wizard_close();
 /* The same ownership holds when an ordinary brightness save replaces Setup. */
 ui_setup_wizard_show_with_appearance_callback(connect_wifi,appearance_rebuilt);setup_select_display_cb(NULL);
 writes=preference_writes;rebuilds=appearance_rebuilds;preference_hook=replace_during_appearance_save;
 lv_obj_send_event(s_brightness_slider,LV_EVENT_RELEASED,NULL);
 assert(preference_writes==writes+1&&s_setup_dispatching&&s_setup_step==SETUP_STEP_DISPLAY);
 setup_select_wifi_cb(NULL);assert(s_setup_step==SETUP_STEP_DISPLAY);
 lv_tick_inc(50);lv_timer_handler();assert(preference_writes==writes+2&&appearance_rebuilds==rebuilds+1&&!s_setup_dispatching);
 ui_setup_wizard_close();
}
static bool nav_complete(int step)
{
 return strstr(lv_label_get_text(lv_obj_get_child(s_nav[step],0)),LV_SYMBOL_OK)!=NULL;
}
static void completion_mark_checks(void)
{
 setup_complete=false;completed_steps=0;progress_ok=preference_ok=true;
 wifi_ready=configured=camera_ready=false;
 ui_setup_wizard_show_with_appearance_callback(connect_wifi,appearance_rebuilt);
 for(int i=0;i<SETUP_STEP_COUNT;i++)assert(!nav_complete(i));
 setup_next_cb(NULL);assert(s_setup_step==SETUP_STEP_WIFI&&nav_complete(SETUP_STEP_WELCOME));
 setup_select_timezone_cb(NULL);preference_ok=false;click(lv_obj_get_child(s_options_list,0));assert(!nav_complete(SETUP_STEP_TIMEZONE));
 preference_ok=true;click(lv_obj_get_child(s_options_list,0));assert(nav_complete(SETUP_STEP_TIMEZONE));
 ui_setup_wizard_close();ui_setup_wizard_show(connect_wifi);assert(nav_complete(SETUP_STEP_TIMEZONE)&&nav_complete(SETUP_STEP_WELCOME));
 setup_select_timezone_cb(NULL);setup_next_cb(NULL);assert(s_setup_step==SETUP_STEP_DISPLAY&&nav_complete(SETUP_STEP_TIMEZONE));
 preference_ok=false;lv_obj_send_event(s_brightness_slider,LV_EVENT_RELEASED,NULL);assert(!nav_complete(SETUP_STEP_DISPLAY));
 setup_next_cb(NULL);assert(s_setup_step==SETUP_STEP_DISPLAY&&!nav_complete(SETUP_STEP_DISPLAY));
 preference_ok=true;progress_ok=false;setup_next_cb(NULL);assert(s_setup_step==SETUP_STEP_DISPLAY&&!nav_complete(SETUP_STEP_DISPLAY));
 assert(contains_text(s_status,"completion"));progress_ok=true;
 setup_next_cb(NULL);assert(s_setup_step==SETUP_STEP_PRINTER&&nav_complete(SETUP_STEP_DISPLAY));
 setup_select_camera_cb(NULL);setup_skip_cb(NULL);assert(!nav_complete(SETUP_STEP_CAMERA)&&!nav_complete(SETUP_STEP_COMPLETE));
 camera_ready=true;setup_select_camera_cb(NULL);assert(nav_complete(SETUP_STEP_CAMERA));
 camera_ready=false;setup_select_complete_cb(NULL);assert(!nav_complete(SETUP_STEP_CAMERA));
 /* A canceled appearance choice cannot manufacture a completed Display step. */
 completed_steps&=~(1U<<ONBOARDING_STEP_DISPLAY);setup_select_display_cb(NULL);
 lv_obj_t *row=lv_obj_get_child(lv_obj_get_child(s_stage,0),1);click(lv_obj_get_child(row,1));ui_setup_wizard_close();lv_tick_inc(50);lv_timer_handler();
 ui_setup_wizard_show_with_appearance_callback(connect_wifi,appearance_rebuilt);assert(!nav_complete(SETUP_STEP_DISPLAY));
 setup_select_display_cb(NULL);row=lv_obj_get_child(lv_obj_get_child(s_stage,0),1);progress_ok=false;
 unsigned rebuilds=appearance_rebuilds;click(lv_obj_get_child(row,1));lv_tick_inc(50);lv_timer_handler();
 assert(s_center&&!nav_complete(SETUP_STEP_DISPLAY)&&appearance_rebuilds==rebuilds+1&&contains_text(s_status,"Appearance saved"));
 progress_ok=true;row=lv_obj_get_child(lv_obj_get_child(s_stage,0),1);click(lv_obj_get_child(row,1));lv_tick_inc(50);lv_timer_handler();assert(nav_complete(SETUP_STEP_DISPLAY));
 wifi_ready=configured=true;setup_select_complete_cb(NULL);assert(!nav_complete(SETUP_STEP_COMPLETE));
 storage_ok=false;finish_cb(NULL);assert(s_center&&!nav_complete(SETUP_STEP_COMPLETE));storage_ok=true;
 finish_cb(NULL);assert(!s_center&&s_complete_popup);setup_completion_done_cb(NULL);
 ui_setup_wizard_show(connect_wifi);assert(nav_complete(SETUP_STEP_COMPLETE)&&nav_complete(SETUP_STEP_WELCOME)&&nav_complete(SETUP_STEP_TIMEZONE)&&nav_complete(SETUP_STEP_DISPLAY));
 assert(nav_complete(SETUP_STEP_WIFI)&&nav_complete(SETUP_STEP_PRINTER)&&!nav_complete(SETUP_STEP_CAMERA));
 ui_setup_wizard_close();
}
static void inside(lv_obj_t*c,lv_obj_t*p){lv_area_t a,b;lv_obj_get_coords(c,&a);lv_obj_get_coords(p,&b);if(!(a.x1>=b.x1&&a.x2<=b.x2&&a.y1>=b.y1&&a.y2<=b.y2)){fprintf(stderr,"outside %d,%d..%d,%d vs %d,%d..%d,%d\n",a.x1,a.y1,a.x2,a.y2,b.x1,b.y1,b.x2,b.y2);abort();}}
static void completion_layout_checks(void)
{
 lv_obj_t *popup=s_complete_popup;assert(popup);lv_obj_update_layout(popup);
 lv_obj_t *title=lv_obj_get_child(popup,0),*body=lv_obj_get_child(popup,1),*footer=lv_obj_get_child(popup,-1);
 inside(popup,lv_screen_active());inside(title,popup);inside(body,popup);inside(footer,popup);
 assert(lv_obj_get_style_border_side(title,0)==LV_BORDER_SIDE_BOTTOM);
 assert(lv_obj_get_style_border_side(footer,0)==LV_BORDER_SIDE_TOP);
 assert(lv_obj_get_scrollbar_mode(body)==LV_SCROLLBAR_MODE_OFF);
 assert(lv_obj_get_child_count(body)==2);
 lv_area_t a,b,c;lv_obj_get_coords(title,&a);lv_obj_get_coords(body,&b);lv_obj_get_coords(footer,&c);assert(a.y2<b.y1&&b.y2<c.y1);
 for(unsigned i=0;i<2;i++){
  lv_obj_t *label=lv_obj_get_child(body,i);assert(lv_label_get_long_mode(label)==LV_LABEL_LONG_WRAP);assert(lv_obj_get_style_text_line_space(label,0)==4);
 }
 lv_obj_get_coords(footer,&a);lv_obj_scroll_to_y(body,LV_COORD_MAX,LV_ANIM_OFF);lv_obj_update_layout(popup);lv_obj_get_coords(footer,&b);assert(!memcmp(&a,&b,sizeof(a)));
 inside(lv_obj_get_child(body,-1),body);lv_obj_scroll_to_y(body,0,LV_ANIM_OFF);
 lv_obj_t *done=lv_obj_get_child(footer,0);inside(done,footer);inside(lv_obj_get_child(done,0),done);assert(lv_obj_get_height(done)>=48&&lv_obj_get_width(done)==160);
 lv_obj_get_coords(done,&a);lv_obj_get_coords(footer,&b);assert(a.x2==b.x2);
}
static void check(void){assert(lv_obj_get_parent(s_center)==lv_screen_active());lv_obj_update_layout(s_center);inside(s_center,lv_screen_active());inside(s_setup_content,s_center);inside(s_stage,s_setup_content);inside(s_actions,s_setup_content);assert(lv_obj_get_height(s_stage)>0);for(unsigned i=0;i<lv_obj_get_child_count(s_actions);i++){lv_obj_t*b=lv_obj_get_child(s_actions,i);inside(b,s_actions);inside(lv_obj_get_child(b,0),b);assert(lv_obj_get_height(b)>=48);}lv_area_t a,b;lv_obj_get_coords(s_actions,&a);lv_obj_scroll_to_y(s_stage,2000,LV_ANIM_OFF);lv_obj_update_layout(s_center);lv_obj_get_coords(s_actions,&b);assert(!memcmp(&a,&b,sizeof(a)));lv_obj_scroll_to_y(s_stage,0,LV_ANIM_OFF);}
static unsigned width=1024;static uint16_t raster[1024*600];
static void flush(lv_display_t*d,const lv_area_t*a,uint8_t*p){uint16_t*v=(uint16_t*)p;for(int y=a->y1;y<=a->y2;y++)for(int x=a->x1;x<=a->x2;x++)raster[y*width+x]=*v++;lv_display_flush_ready(d);}
static void snapshot(unsigned theme,const char*name){const char*folder=getenv("SETUP_SCREENSHOTS");if(!folder||width!=1024)return;lv_tick_inc(250);lv_timer_handler();lv_refr_now(NULL);char path[512];snprintf(path,sizeof(path),"%s/%s-theme%u.ppm",folder,name,theme);FILE*f=fopen(path,"wb");assert(f);fprintf(f,"P6\n1024 600\n255\n");for(unsigned i=0;i<1024*600;i++){uint16_t v=raster[i];unsigned char c[3]={(v>>11)*255/31,((v>>5)&63)*255/63,(v&31)*255/31};fwrite(c,1,3,f);}fclose(f);}
static void setup_profile_discovery_request_safety(void)
{
 unsigned started=setup_scan_starts;setup_request=0;printer_editor_discover_cb();assert(setup_scan_starts==started);
 setup_request=41;setup_show_ok=false;printer_editor_discover_cb();assert(setup_scan_starts==started);
 setup_show_ok=true;printer_editor_discover_cb();assert(setup_scan_starts==started+1&&setup_shown_request==41&&setup_select_request);
 setup_select_request("selected.local",7125,"Selected",41);assert(setup_applied_request==41&&setup_closed_request==41);
 printer_editor_discover_closed(41);assert(!setup_request);
 setup_request=42;printer_editor_discover_closed(41);assert(setup_request==42);setup_select_request("old.local",7125,"Old",41);assert(setup_applied_request==41&&setup_closed_request==41);
}
int main(void){lv_init();lv_display_t*d=lv_display_create(1024,600);static uint8_t buffer[1024*50*2];lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);lv_display_set_buffers(d,buffer,NULL,sizeof(buffer),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(d,flush);
 for(unsigned t=0;t<5;t++)for(unsigned density=0;density<3;density++)for(unsigned large=0;large<2;large++)for(unsigned w=0;w<3;w++){
 width=(unsigned[]){1024,640,480}[w];lv_display_set_resolution(d,width,600);ui_theme_set_active(t);ui_theme_set_density(density);ui_theme_set_accessibility((ui_accessibility_t){.large_text=large});wifi_ready=configured=camera_ready=false;setup_complete=false;completed_steps=0;
 active_slot=0;address_ready=true;ui_setup_wizard_show(connect_wifi);setup_profile_discovery_request_safety();check();assert(contains_text(s_stage,"Choose a network")||width<800);if(width==1024)for(unsigned group=0;group<lv_obj_get_child_count(s_stage);group++)inside(lv_obj_get_child(s_stage,group),s_stage);lv_obj_t*owner=s_center;ui_setup_wizard_show(connect_wifi);assert(owner==s_center);if(density==1&&large)snapshot(t,"welcome");
 for(int i=1;i<SETUP_STEP_COUNT;i++){lv_obj_send_event(s_nav[i],LV_EVENT_CLICKED,NULL);assert(s_setup_step==i);check();for(int nav=0;nav<SETUP_STEP_COUNT;nav++)assert(lv_obj_has_state(s_nav[nav],LV_STATE_CHECKED)==(nav==i));}
 setup_select_wifi_cb(NULL);check();assert(contains_text(s_stage,"No network connected")&&contains_text(s_stage,"Not connected"));
 if(density==1&&large)snapshot(t,"network-disconnected");
 wifi_ready=true;setup_network_refresh_cb(NULL);check();assert(contains_text(s_stage,"Workshop_long_network_name_0")&&contains_text(s_stage,"192.0.68.123"));
 if(density==1&&large)snapshot(t,"network-connected");
 address_ready=false;setup_network_refresh_cb(NULL);assert(contains_text(s_stage,"Waiting for an address"));address_ready=true;wifi_ready=false;
 setup_select_printer_cb(NULL);assert(contains_text(s_stage,"Add printer"));
 configured=true;active_slot=2;profile.secure_transport=true;strlcpy(profile.name,"Workshop printer",sizeof(profile.name));strlcpy(profile.api_key,"DO-NOT-DISPLAY-THIS-KEY",sizeof(profile.api_key));
 setup_select_printer_cb(NULL);check();assert(contains_text(s_stage,"Workshop printer")&&contains_text(s_stage,"https://printer.local:7125")&&contains_text(s_stage,"API key saved")&&!contains_text(s_center,profile.api_key));
 unsigned opened=editor_opened;center_printer_cb(NULL);assert(editor_opened==opened+1&&editor_slot==2);
 if(width==1024)for(unsigned group=0;group<lv_obj_get_child_count(s_stage);group++)inside(lv_obj_get_child(s_stage,group),s_stage);
 if(density==1&&large)snapshot(t,"printer-saved");
 profile.api_key[0]=0;profile.secure_transport=false;setup_select_printer_cb(NULL);assert(contains_text(s_stage,"http://printer.local:7125")&&contains_text(s_stage,"No API key saved"));
 active_slot=0;configured=false;
 setup_select_timezone_cb(NULL);check();assert(lv_obj_get_child_count(s_options_list)==15);
 preference_ok=false;size_t old_zone=selected_zone;click(lv_obj_get_child(s_options_list,1));assert(selected_zone==old_zone&&strstr(lv_label_get_text(s_status),"Could not save")&&!nav_complete(SETUP_STEP_TIMEZONE));
 preference_ok=true;click(lv_obj_get_child(s_options_list,1));assert(selected_zone==2&&lv_obj_has_state(lv_obj_get_child(s_options_list,1),LV_STATE_CHECKED)&&nav_complete(SETUP_STEP_TIMEZONE));
 if(density==1&&large)snapshot(t,"timezone");
 setup_select_display_cb(NULL);check();
 if(width==1024)for(unsigned group=0;group<lv_obj_get_child_count(s_stage);group++)inside(lv_obj_get_child(s_stage,group),s_stage);
 lv_slider_set_value(s_brightness_slider,60,LV_ANIM_OFF);lv_obj_send_event(s_brightness_slider,LV_EVENT_VALUE_CHANGED,NULL);assert(strstr(lv_label_get_text(s_brightness_value),"60%"));
 preference_ok=false;lv_obj_send_event(s_brightness_slider,LV_EVENT_RELEASED,NULL);assert(lv_slider_get_value(s_brightness_slider)==brightness&&!nav_complete(SETUP_STEP_DISPLAY));
 preference_ok=true;lv_slider_set_value(s_brightness_slider,70,LV_ANIM_OFF);lv_obj_send_event(s_brightness_slider,LV_EVENT_RELEASED,NULL);assert(brightness==70&&nav_complete(SETUP_STEP_DISPLAY));
 lv_obj_t *sleep_row=lv_obj_get_child(lv_obj_get_child(s_stage,3),1);uint8_t old_sleep=sleep_minutes;
 preference_ok=false;click(lv_obj_get_child(sleep_row,2));assert(sleep_minutes==old_sleep&&strstr(lv_label_get_text(s_status),"Could not save"));
 preference_ok=true;click(lv_obj_get_child(sleep_row,2));assert(sleep_minutes==15);check();
 if(density==1&&large)snapshot(t,"display");
 click(s_back_action);assert(s_setup_step==SETUP_STEP_TIMEZONE);setup_select_complete_cb(NULL);
 unsigned before=completions;finish_cb(NULL);assert(s_center&&completions==before);
 setup_select_printer_cb(NULL);before=editor_opened;center_printer_cb(NULL);assert(editor_opened==before+1);
 setup_select_wifi_cb(NULL);center_wifi_cb(NULL);assert(s_wifi_scan_busy&&scan_task);wifi_scan_open();scan_task(NULL);wifi_scan_ready(NULL);check();assert(lv_obj_get_child_count(s_wifi_list)==8);
 if(width==1024){lv_area_t stage,list;lv_obj_get_content_coords(s_stage,&stage);lv_obj_get_coords(s_wifi_list,&list);assert(list.y2==stage.y2&&lv_obj_get_height(s_wifi_list)>48);}
 if(density==1&&large)snapshot(t,"networks");
 lv_obj_send_event(lv_obj_get_child(s_wifi_list,0),LV_EVENT_CLICKED,NULL);check();assert(s_password&&s_keyboard&&lv_textarea_get_password_mode(s_password));assert(lv_textarea_get_max_length(s_password)==63);lv_textarea_set_text(s_password,"test-password");before=connections;keyboard_layout_check(s_setup_content,s_keyboard,s_password);if(density==1&&large)snapshot(t,"password");lv_obj_send_event(s_keyboard,LV_EVENT_READY,NULL);assert(connections==before+1&&s_poll);lv_obj_send_event(s_keyboard,LV_EVENT_READY,NULL);assert(connections==before+1);wifi_ready=true;wrong_ssid=true;wifi_verify(NULL);assert(!s_wifi_done&&s_poll);lv_tick_inc(30001);wifi_verify(NULL);assert(!s_wifi_done&&!s_poll&&s_password&&!strcmp(lv_textarea_get_text(s_password),"test-password")&&strstr(lv_label_get_text(s_status),"timed out"));wrong_ssid=false;wifi_save_cb(NULL);assert(s_poll);wifi_verify(NULL);assert(s_wifi_done);wifi_return_timer(s_poll);check();
 setup_select_camera_cb(NULL);check();assert(contains_text(s_stage,"Set up printer"));
 if(density==1&&large)snapshot(t,"camera-no-printer");
 configured=true;setup_select_camera_cb(NULL);check();assert(contains_text(s_stage,"Camera setup is optional."));
 if(width==1024)for(unsigned group=0;group<lv_obj_get_child_count(s_stage);group++)inside(lv_obj_get_child(s_stage,group),s_stage);
 if(density==1&&large)snapshot(t,"camera-optional");
 camera_discover_ok=false;center_camera_cb(NULL);assert(!s_poll&&s_camera_scan_action&&!lv_obj_has_state(s_camera_scan_action,LV_STATE_DISABLED)&&strstr(lv_label_get_text(s_status),"Could not start"));
 if(density==1&&large)snapshot(t,"camera-start-failed");
 camera_discover_ok=true;center_camera_cb(NULL);lv_timer_t *camera_wait=s_poll;unsigned scans=camera_scans;
 assert(s_poll&&lv_obj_has_state(s_camera_scan_action,LV_STATE_DISABLED));center_camera_cb(NULL);assert(s_poll==camera_wait&&camera_scans==scans);
 discovery_result=false;camera_discovery_poll(NULL);check();
 assert(!s_poll&&lv_obj_get_child_count(s_camera_list)==2&&contains_text(s_camera_list,"No cameras found")&&lv_obj_get_child_count(s_actions)==2&&!lv_obj_has_state(s_camera_scan_action,LV_STATE_DISABLED));
 if(density==1&&large)snapshot(t,"camera-empty");
 lv_obj_send_event(lv_obj_get_child(s_actions,1),LV_EVENT_CLICKED,NULL);assert(s_setup_step==SETUP_STEP_COMPLETE&&!camera_ready);check();
 before=completions;finish_cb(NULL);assert(!s_center&&s_complete_popup&&completions==before+1);setup_completion_done_cb(NULL);
 ui_setup_wizard_show(connect_wifi);setup_select_camera_cb(NULL);center_camera_cb(NULL);discovery_result=true;camera_discovery_poll(NULL);check();assert(lv_obj_get_child_count(s_camera_list)==4);if(density==1&&large)snapshot(t,"cameras");lv_obj_send_event(lv_obj_get_child(s_camera_list,0),LV_EVENT_CLICKED,NULL);camera_result=false;camera_test_poll(NULL);assert(s_center&&!lv_obj_has_state(s_camera_scan_action,LV_STATE_DISABLED));if(density==1&&large)snapshot(t,"camera-test-failed");lv_obj_send_event(lv_obj_get_child(s_camera_list,0),LV_EVENT_CLICKED,NULL);camera_result=true;camera_test_poll(NULL);check();if(density==1&&large)snapshot(t,"camera-verified");camera_save_cb(NULL);assert(camera_ready&&nav_complete(SETUP_STEP_CAMERA));check();
 assert(contains_text(s_stage,"Saved camera")&&contains_text(s_stage,"Workshop camera 0")&&lv_obj_get_child_count(s_actions)==1);
 if(width==1024)for(unsigned group=0;group<lv_obj_get_child_count(s_stage);group++)inside(lv_obj_get_child(s_stage,group),s_stage);
 if(density==1&&large)snapshot(t,"camera-saved");
 size_t previous_default=saved_camera_slot;saved_camera_slot=2;custom_camera_url=true;setup_select_camera_cb(NULL);assert(contains_text(s_stage,"Custom camera")&&!contains_text(s_center,"PRIVATE-SECRET"));custom_camera_url=false;saved_camera_slot=previous_default;
 setup_select_camera_cb(NULL);center_camera_cb(NULL);config_generation++;camera_discovery_poll(NULL);assert(!s_poll&&strstr(lv_label_get_text(s_status),"Printer changed")&&!lv_obj_get_child_count(s_camera_list));center_camera_cb(NULL);camera_discovery_poll(NULL);assert(lv_obj_get_child_count(s_camera_list)==4);
 setup_select_complete_cb(NULL);check();assert(strstr(lv_label_get_text(lv_obj_get_child(s_stage,3)),timezone_config_selected_label()));
 assert(contains_text(s_stage,"Display:")&&contains_text(s_stage,"70%")&&contains_text(s_stage,"Sleep 15 min"));
 if(width==1024){for(unsigned row=0;row<3;row++){lv_obj_t *summary=lv_obj_get_child(s_stage,row);for(unsigned cell=0;cell<lv_obj_get_child_count(summary);cell++){lv_obj_t *label=lv_obj_get_child(summary,cell);inside(label,summary);assert(lv_obj_get_height(label)==lv_obj_get_style_text_font(label,0)->line_height);}}assert(contains_text(s_stage,"Workshop_long_")&&contains_text(s_stage,"Workshop printer"));for(unsigned group=0;group<lv_obj_get_child_count(s_stage);group++)inside(lv_obj_get_child(s_stage,group),s_stage);lv_obj_t *display_summary=lv_obj_get_child(s_stage,4);assert(lv_obj_get_height(display_summary)<lv_obj_get_style_text_font(display_summary,0)->line_height*2);}if(density==1&&large)snapshot(t,"review");storage_ok=false;finish_cb(NULL);assert(s_center);storage_ok=true;finish_cb(NULL);assert(!s_center&&!s_setup_content&&!s_status&&!s_poll&&s_complete_popup);lv_obj_update_layout(s_complete_popup);inside(s_complete_popup,lv_screen_active());assert(contains_text(s_complete_popup,"Setup complete")&&contains_text(s_complete_popup,"live connection status"));
 completion_layout_checks();
 lv_obj_t *completion_footer=lv_obj_get_child(s_complete_popup,-1),*done=lv_obj_get_child(completion_footer,0);inside(completion_footer,s_complete_popup);inside(done,completion_footer);inside(lv_obj_get_child(done,0),done);if(density==1&&large)snapshot(t,"completed");unsigned completed_before_done=completions;click(done);assert(!s_complete_popup&&!s_center&&completions==completed_before_done);
 ui_setup_wizard_show(connect_wifi);lv_obj_delete(s_center);assert(!s_center&&!s_setup_content&&!s_status&&!s_keyboard);ui_setup_wizard_show(connect_wifi);later_cb(NULL);assert(!s_center);safety_checks();
 }
 wifi_ready=configured=true;ui_setup_wizard_show(connect_wifi);wifi_read_hook=reopen_setup;wifi_read_countdown=2;setup_select_complete_cb(NULL);assert(s_center&&s_setup_step==SETUP_STEP_WELCOME&&!s_finish_action&&lv_obj_get_child_count(s_actions)==1);ui_setup_wizard_close();
 configured=camera_ready=true;ui_setup_wizard_show(connect_wifi);catalog_read_hook=reopen_setup;setup_select_camera_cb(NULL);assert(s_setup_step==SETUP_STEP_WELCOME&&lv_obj_get_child_count(s_actions)==1);
 setup_select_camera_cb(NULL);center_camera_cb(NULL);discovery_result=true;camera_discovery_poll(NULL);camera_test_ok=false;click(s_camera_rows[1]);assert(!s_poll&&strstr(lv_label_get_text(s_status),"Could not start")&&!lv_obj_has_state(s_camera_scan_action,LV_STATE_DISABLED));camera_test_ok=true;
 unsigned saved_defaults=default_saves,saved_urls=url_saves;click(lv_obj_get_child(s_actions,1));assert(s_setup_step==SETUP_STEP_COMPLETE&&camera_ready&&default_saves==saved_defaults&&url_saves==saved_urls);ui_setup_wizard_close();
 wifi_ready=true;ui_setup_wizard_show(connect_wifi);address_read_hook=reopen_setup;setup_select_wifi_cb(NULL);assert(s_setup_step==SETUP_STEP_WELCOME&&s_stage&&lv_obj_get_child_count(s_actions)==1);ui_setup_wizard_close();
 ui_theme_set_active(UI_THEME_STUDIO_DARK);ui_setup_wizard_show_with_appearance_callback(connect_wifi,appearance_rebuilt);setup_select_display_cb(NULL);
 lv_obj_t *theme_row=lv_obj_get_child(lv_obj_get_child(s_stage,0),1);
 preference_ok=false;click(lv_obj_get_child(theme_row,1));lv_tick_inc(50);lv_timer_handler();assert(ui_theme_get_active()==UI_THEME_STUDIO_DARK&&!appearance_rebuilds);
 preference_ok=true;click(lv_obj_get_child(theme_row,1));lv_tick_inc(50);lv_timer_handler();assert(ui_theme_get_active()==UI_THEME_OPERATOR&&appearance_rebuilds==1&&s_setup_step==SETUP_STEP_DISPLAY);
 lv_obj_t *text_row=lv_obj_get_child(lv_obj_get_child(s_stage,1),1);
 ui_accessibility_t options=ui_theme_get_accessibility();options.high_contrast=true;ui_theme_set_accessibility(options);
 click(lv_obj_get_child(text_row,1));lv_tick_inc(50);lv_timer_handler();assert(ui_theme_get_accessibility().large_text&&ui_theme_get_accessibility().high_contrast&&appearance_rebuilds==2);
 setup_select_timezone_cb(NULL);preference_hook=reopen_setup;click(lv_obj_get_child(s_options_list,0));preference_hook=NULL;assert(s_setup_step==SETUP_STEP_WELCOME);
 setup_select_display_cb(NULL);preference_hook=reopen_setup;lv_obj_send_event(s_brightness_slider,LV_EVENT_RELEASED,NULL);preference_hook=NULL;assert(s_setup_step==SETUP_STEP_WELCOME);
 setup_select_display_cb(NULL);theme_row=lv_obj_get_child(lv_obj_get_child(s_stage,0),1);unsigned writes_before=preference_writes;click(lv_obj_get_child(theme_row,0));ui_setup_wizard_close();lv_tick_inc(50);lv_timer_handler();assert(!s_center&&appearance_rebuilds==2&&preference_writes==writes_before);
 appearance_cancel_checks();completion_mark_checks();
 width=480;lv_display_set_resolution(d,480,280);
 for(unsigned theme=0;theme<5;theme++)for(unsigned density=0;density<3;density++)for(unsigned large=0;large<2;large++){
  ui_theme_set_active(theme);ui_theme_set_density(density);ui_theme_set_accessibility((ui_accessibility_t){.large_text=large});
  show_setup_completion();completion_layout_checks();assert(lv_obj_get_height(s_complete_popup)<=248);
  lv_obj_t *done=lv_obj_get_child(lv_obj_get_child(s_complete_popup,-1),0);click(done);assert(!s_complete_popup);
 }
 lv_display_delete(d);lv_deinit();puts("PASS: persistent completion check marks, failed/canceled writes, accepted defaults, optional camera and Review completion; configured network/printer review values, compact display summary, completion bounds and review-read owner replacement; saved/custom camera summaries and URL redaction, optional/no-printer/empty/failure/retry states, busy scan deduplication and no additional camera save on Continue; active-profile printer editing, SSID/IP/waiting-address summaries, saved-vs-live status and API-key redaction; seven-step Setup layout, sparse timezone catalog/persistence failure, brightness/sleep save failure/retry, deferred theme/text rebuild, direct-delete cancellation, replacement callback ownership and closed-owner cancellation across five themes/densities/text/viewports; Wi-Fi nested dispatch/retry/captured credentials, camera verification ownership/save failures, retired fields/actions/timers, completion and replacement teardown");return 0;}
