#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "printer_connection_policy.h"
static moonraker_state_t live;
void moonraker_state_snapshot(moonraker_state_t*out){*out=live;}
#if defined(TEST_PRINTER)
#include "printer_ui_controller.c"
static unsigned sends,motions,recoveries,cancels,objects;
uint32_t moonraker_config_generation(void){return 1;}
void ui_toast_show(ui_status_kind_t k,const char*t,const char*d){(void)k;(void)t;(void)d;}
static bool send(const char*c){assert(c&&c[0]);sends++;return true;}
static void motion(void){motions++;}
static void cancel(void){cancels++;}
static void object(void){objects++;}
void ui_filament_recovery_show(bool (*cb)(const char *)){assert(cb==send);recoveries++;}
static void event(lv_obj_t*b,const char*c){
 (void)b;size_t slot=!strcmp(c,"HOME_ALL")?ACTION_HOME:!strcmp(c,"PAUSE")?ACTION_PAUSE:!strcmp(c,"RESUME")?ACTION_RESUME:!strcmp(c,"CANCEL_OBJECT")?ACTION_OBJECT:ACTION_CANCEL;
 b=s_action_buttons[slot];
 printer_ui_controller_update_action_buttons(s_action_buttons[0],s_action_buttons[1],s_action_buttons[2],s_action_buttons[3],s_action_buttons[4],true,live.printer_state);
 lv_obj_remove_event_cb(b,printer_ui_controller_command_event_cb);lv_obj_add_event_cb(b,printer_ui_controller_command_event_cb,LV_EVENT_CLICKED,(void*)c);lv_obj_send_event(b,LV_EVENT_CLICKED,NULL);
}
int main(void){lv_init();lv_display_t*d=lv_display_create(1024,600);lv_obj_t*buttons[6];for(unsigned i=0;i<6;i++)buttons[i]=lv_button_create(lv_screen_active());printer_ui_controller_init(send,cancel,object,motion);ui_printer_actions_t actions={.home=buttons[0],.pause=buttons[1],.resume=buttons[2],.object=buttons[3],.cancel=buttons[4],.motion=buttons[5]};printer_ui_controller_bind_actions(&actions);
for(unsigned scenario=0;scenario<5;scenario++){
 live=(moonraker_state_t){.moonraker_ok=scenario!=0,.live_data_ok=scenario!=1};snprintf(live.printer_state,sizeof(live.printer_state),"%s",(const char*[]){"printing","printing","error","shutdown","startup"}[scenario]);
 printer_ui_controller_update_action_buttons(buttons[0],buttons[1],buttons[2],buttons[3],buttons[4],true,live.printer_state);for(unsigned i=0;i<5;i++)assert(lv_obj_has_state(buttons[i],LV_STATE_DISABLED));
 for(unsigned i=0;i<5;i++)event(buttons[5],(const char*[]){"HOME_ALL","PAUSE","RESUME","CANCEL_PRINT","CANCEL_OBJECT"}[i]);
 lv_obj_add_event_cb(buttons[5],printer_ui_controller_motion_event_cb,LV_EVENT_CLICKED,NULL);lv_obj_send_event(buttons[5],LV_EVENT_CLICKED,NULL);lv_obj_remove_event_cb(buttons[5],printer_ui_controller_motion_event_cb);assert(!sends&&!motions&&!recoveries&&!cancels&&!objects);
}
live.moonraker_ok=live.live_data_ok=true;strcpy(live.printer_state,"ready");printer_ui_controller_update_action_buttons(buttons[0],buttons[1],buttons[2],buttons[3],buttons[4],true,live.printer_state);assert(!lv_obj_has_state(buttons[0],LV_STATE_DISABLED));event(buttons[5],"HOME_ALL");assert(sends==1);
strcpy(live.printer_state,"printing");event(buttons[5],"HOME_ALL");event(buttons[5],"RESUME");assert(sends==1&&!recoveries);event(buttons[5],"PAUSE");event(buttons[5],"CANCEL_PRINT");event(buttons[5],"CANCEL_OBJECT");assert(sends==2&&cancels==1&&objects==1);
lv_obj_add_event_cb(buttons[5],printer_ui_controller_motion_event_cb,LV_EVENT_CLICKED,NULL);lv_obj_send_event(buttons[5],LV_EVENT_CLICKED,NULL);assert(motions==1);lv_obj_remove_event_cb(buttons[5],printer_ui_controller_motion_event_cb);
strcpy(live.printer_state,"paused");event(buttons[5],"RESUME");assert(recoveries==1);strcpy(live.printer_state,"complete");assert(!printer_idle_reason(&live)[0]);
lv_display_delete(d);lv_deinit();puts("PASS: disconnected/stale printing state, Klipper waiting/error/shutdown/startup, blocked direct events, restored idle/printing/paused actions and active-print tool controls");}
#elif defined(TEST_METADATA)
#include "file_detail_loader.c"
static uint32_t profile=1;static unsigned callbacks,requests,allocations,frees;static bool switch_during_request,switch_during_lock;static void(*worker)(void*);static void*worker_arg;
uint32_t moonraker_config_generation(void){return profile;}
void *test_calloc(size_t n,size_t z,unsigned caps){(void)caps;void*p=calloc(n,z);if(p)allocations++;return p;}
void test_free(void*p){if(p){frees++;free(p);}}
size_t strlcpy(char*d,const char*s,size_t n){size_t size=strlen(s);if(n)snprintf(d,n,"%s",s);return size;}
int xTaskCreatePinnedToCore(void(*fn)(void*),const char*name,unsigned stack,void*arg,unsigned priority,void*task,int core){(void)name;(void)stack;(void)priority;(void)task;(void)core;worker=fn;worker_arg=arg;return pdPASS;}
void vTaskDelete(void*p){assert(!p);}
void vTaskDelay(unsigned ticks){(void)ticks;}
bool bsp_display_lock(unsigned ms){(void)ms;if(switch_during_lock)profile++;return true;}
void bsp_display_unlock(void){}
const char *esp_err_to_name(esp_err_t e){(void)e;return "TIMEOUT";}
void thumbnail_manager_url_encode(const char*a,char*b,size_t n){snprintf(b,n,"%s",a);}
bool moonraker_fetch_file_metadata(const char*h,int port,const char*k,const char*file,char*body,size_t n,int*code,esp_err_t*error){(void)h;(void)port;(void)k;(void)file;requests++;*code=200;*error=ESP_OK;snprintf(body,n,"metadata");if(switch_during_request)profile++;return true;}
bool printer_files_build_metadata_text(const char*f,const char*body,char*thumb,size_t tn,char*metadata,size_t mn){(void)f;(void)body;snprintf(thumb,tn,"preview.png");snprintf(metadata,mn,"Details");return true;}
static void ready(const char*f,bool ok,const char*m,const char*t){assert(f&&ok&&m&&t);callbacks++;}
static void run(void){void*arg=worker_arg;worker_arg=NULL;worker(arg);}
int main(void){assert(file_detail_loader_start("host",7125,"key","job.gcode",ready));run();assert(callbacks==1&&requests==1);assert(file_detail_loader_start("host",7125,"key","job.gcode",ready));profile++;run();assert(callbacks==1&&requests==1);
assert(file_detail_loader_start("host",7125,"key","job.gcode",ready));switch_during_request=true;run();assert(callbacks==1);switch_during_request=false;
assert(file_detail_loader_start("host",7125,"key","job.gcode",ready));switch_during_lock=true;run();assert(callbacks==1);switch_during_lock=false;
assert(file_detail_loader_start("host",7125,"key","job.gcode",ready));file_detail_loader_cancel();run();assert(callbacks==1&&allocations==frees);puts("PASS: metadata profile fence before request, during request and inside display lock; cancel and allocation cleanup");}
#elif defined(TEST_MESH)
#define s profile_state
#include "ui_bed_mesh_profiles.c"
#undef s
#include "ui_bed_mesh.c"
static uint32_t profile=1;static unsigned sent;
uint32_t moonraker_config_generation(void){return profile;}
static bool send(const char*c){assert(c&&c[0]);sent++;return true;}
bool custom_theme_color_override(uint32_t a,uint32_t b,uint32_t c,uint32_t*d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_metric_override(int32_t a,int32_t b,int32_t c,int32_t*d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_accent_override(uint8_t a,uint32_t*b){(void)a;(void)b;return false;}
bool custom_theme_surface_opacity(uint8_t*a){(void)a;return false;}
int main(void){lv_init();lv_display_t*d=lv_display_create(1024,600);s.command=send;s.owner_generation=profile;s.popup=lv_obj_create(lv_screen_active());lv_obj_add_event_cb(s.popup,mesh_deleted,LV_EVENT_DELETE,NULL);s.connection_timer=lv_timer_create(mesh_connection_refresh,250,NULL);s.stats=lv_label_create(s.popup);s.calibrate_button=lv_button_create(s.popup);live=(moonraker_state_t){.moonraker_ok=true,.live_data_ok=true};strcpy(live.printer_state,"ready");mesh_connection_refresh(NULL);assert(!lv_obj_has_state(s.calibrate_button,LV_STATE_DISABLED));
for(unsigned i=0;i<4;i++){live.moonraker_ok=i!=0;live.live_data_ok=i!=1;strcpy(live.printer_state,i==2?"printing":"ready");if(i==3)profile++;mesh_connection_refresh(NULL);assert(lv_obj_has_state(s.calibrate_button,LV_STATE_DISABLED));confirm_calibrate_cb(NULL);assert(!sent);lv_obj_clean(lv_layer_top());}
live.moonraker_ok=live.live_data_ok=true;strcpy(live.printer_state,"ready");s.owner_generation=profile;mesh_connection_refresh(NULL);confirm_calibrate_cb(NULL);assert(sent==1);
ui_bed_mesh_profiles_init(s.popup,send);ui_bed_mesh_profiles_update(&(bed_mesh_snapshot_t){.valid=true});assert(profile_command_ready(NULL));profile_state.actions[0]=lv_button_create(s.popup);watch_profile_ref(profile_state.actions[0]);ui_bed_mesh_profiles_refresh_availability();assert(!lv_obj_has_state(profile_state.actions[0],LV_STATE_DISABLED));profile++;ui_bed_mesh_profiles_refresh_availability();assert(lv_obj_has_state(profile_state.actions[0],LV_STATE_DISABLED));assert(!profile_command_ready(NULL));lv_obj_clean(lv_layer_top());ui_bed_mesh_profiles_close();
for(unsigned theme=0;theme<5;theme++){ui_theme_set_active(theme);ui_theme_set_accessibility((ui_accessibility_t){.large_text=true});ui_bed_mesh_profiles_init(s.popup,send);ui_bed_mesh_profiles_update(&(bed_mesh_snapshot_t){.valid=true});ui_bed_mesh_profiles_show_cb(NULL);assert(profile_state.profile_popup && profile_state.actions[0]);live.moonraker_ok=false;ui_bed_mesh_profiles_refresh_availability();assert(lv_obj_has_state(profile_state.actions[0],LV_STATE_DISABLED)&&strstr(lv_label_get_text(profile_state.connection_label),"Offline"));live.moonraker_ok=true;ui_bed_mesh_profiles_refresh_availability();assert(!lv_obj_has_state(profile_state.actions[0],LV_STATE_DISABLED));snprintf(profile_state.pending_profile_name,sizeof(profile_state.pending_profile_name),"%s","default");show_profile_confirmation(false);assert(profile_state.profile_confirm && profile_state.actions[3]);live.moonraker_ok=false;ui_bed_mesh_profiles_refresh_availability();assert(lv_obj_has_state(profile_state.actions[3],LV_STATE_DISABLED));live.moonraker_ok=true;ui_bed_mesh_profiles_refresh_availability();assert(!lv_obj_has_state(profile_state.actions[3],LV_STATE_DISABLED));lv_obj_delete(profile_state.profile_confirm);assert(!profile_state.profile_confirm&&!profile_state.actions[3]&&!profile_state.pending_profile_name[0]);lv_obj_delete(profile_state.profile_popup);assert(!profile_state.profile_popup&&!profile_state.actions[0]&&!profile_state.connection_label);}
ui_bed_mesh_profiles_close();lv_obj_delete(s.popup);assert(!s.popup&&!s.connection_timer&&!s.buf&&!s.values);memset(&s,0,sizeof(s));lv_display_delete(d);lv_deinit();puts("PASS: Bed Mesh disabled/restored calibration, blocked stale/offline/busy confirmations and profile command ownership");}
#endif
