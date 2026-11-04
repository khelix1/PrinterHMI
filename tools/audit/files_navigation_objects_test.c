#include <stdbool.h>
#include "ui_shell.h"
#include "moonraker.h"
#include "printer_simulation.h"
#include "printer_file_controller.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static bool s_got_ip=true,accept_start;
static char moonraker_status[160];
static int nav,created,restored,closed,feedback,submitted;
static const char *moonraker_config_host(void){return "test.invalid";}
static int moonraker_config_port(void){return 7125;}
static const char *moonraker_config_api_key(void){return "test-key";}
static void close_printer_file_detail_popup(void){++closed;}
static void ui_files_print_feedback(const char *text){assert(text && text[0]);++feedback;}
void ui_shell_set_active_nav(int index){nav=index;}
static void ui_dashboard_create(void){++created;}
static void dashboard_restore_active_profile_preview(void){++restored;}
bool printer_file_controller_start_file(bool network,const char *host,int port,const char *key,const char *filename,char *status,size_t size){
 assert(!strcmp(filename,"folder/reviewed.gcode"));++submitted;assert(!strcmp(host,"test.invalid") && port==7125 && !strcmp(key,"test-key"));
 if(!network || !accept_start){snprintf(status,size,"Print not started");return false;}
 return true;
}
#include "dashboard_route.inc"
#include "live_start.inc"
static bool demo;
static unsigned locked,locks;
static moonraker_state_t g_moonraker_state;
static moonraker_filament_state_t *g_moonraker_filament_state;
static moonraker_exclude_state_t *g_moonraker_exclude_state;
static int64_t s_websocket_status_us;
bool demo_mode_enabled(void){return demo;}
static void state_lock(void){assert(!locked);locked=1;++locks;}
static void state_unlock(void){assert(locked);locked=0;}
static int64_t esp_timer_get_time(void){return 1234;}
#include "demo_publish.inc"
int main(void){
 nav=UI_SHELL_PAGE_FILES;accept_start=false;printer_file_detail_start_bridge("folder/reviewed.gcode");assert(nav==UI_SHELL_PAGE_FILES && !created && !closed && feedback==1);
 accept_start=true;s_got_ip=false;printer_file_detail_start_bridge("folder/reviewed.gcode");assert(nav==UI_SHELL_PAGE_FILES && !created && !closed && feedback==2);
 s_got_ip=true;printer_file_detail_start_bridge("folder/reviewed.gcode");assert(nav==UI_SHELL_PAGE_DASHBOARD && created==1 && restored==1 && closed==1 && submitted==3);
 g_moonraker_filament_state=calloc(1,sizeof(*g_moonraker_filament_state));g_moonraker_exclude_state=calloc(1,sizeof(*g_moonraker_exclude_state));assert(g_moonraker_filament_state&&g_moonraker_exclude_state);
 moonraker_state_t state={.moonraker_ok=true,.live_data_ok=true,.capabilities={.has_exclude_object=true}};
 moonraker_filament_state_t filament={0};bool excluded[2]={false,false};
 moonraker_state_publish_demo(&state,&filament,excluded);assert(!locks&&!g_moonraker_state.moonraker_ok);
 demo=true;moonraker_state_publish_demo(&state,&filament,excluded);assert(locks==1 && !locked && s_websocket_status_us==1234 && g_moonraker_state.capabilities.has_exclude_object && moonraker_exclude_objects_available());
 moonraker_exclude_state_t *snapshot=calloc(1,sizeof(*snapshot));assert(snapshot);moonraker_exclude_state_snapshot(snapshot);
 assert(snapshot->available && snapshot->object_count==2 && snapshot->bed_bounds_valid && snapshot->objects[0].polygon_count==4 && !strcmp(snapshot->objects[0].name,"part_1") && snapshot->objects[0].current);
 excluded[0]=true;moonraker_state_publish_demo(&state,&filament,excluded);moonraker_exclude_state_snapshot(snapshot);assert(snapshot->objects[0].excluded && snapshot->objects[1].current && !strcmp(snapshot->current_object,"part_2") && moonraker_exclude_objects_available());
 excluded[1]=true;moonraker_state_publish_demo(&state,&filament,excluded);assert(!moonraker_exclude_objects_available());
 excluded[0]=excluded[1]=false;moonraker_state_publish_demo(&state,&filament,excluded);assert(moonraker_exclude_objects_available());
 free(snapshot);free(g_moonraker_filament_state);free(g_moonraker_exclude_state);return 0;
}
