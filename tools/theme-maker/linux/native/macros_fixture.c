#include "printer_simulation_catalog.h"
/* Local object/config discovery and send outcomes; all macro UI/validation is firmware. */
#include <stdio.h>
#include "lvgl.h"
#include "cJSON.h"
#include "macro_controller.h"
#include "moonraker.h"
#include "moonraker_config_controller.h"
#include "ui_macros.h"
#include "nvs.h"
#include <string.h>
static uint32_t owner;
static bool known_online,fail_send;
static int fixture_mode;
static unsigned attempts,accepted;
static bool (*sender)(const char *);
static void catalog(void){
 macro_controller_init();bool full=moonraker_config_active_profile_index()==0;
 cJSON *objects=fixture_mode==1?cJSON_CreateArray():printer_simulation_macro_objects(full);
 cJSON_AddItemToArray(objects,cJSON_CreateString("gcode_macro _PRIVATE_HELPER"));
 if(fixture_mode==2)for(int i=0;i<70;i++){char object[96];snprintf(object,sizeof(object),"gcode_macro SAMPLE_%03d",i);cJSON_AddItemToArray(objects,cJSON_CreateString(object));}
 macro_controller_update_from_objects(objects);cJSON_Delete(objects);
 cJSON *config=printer_simulation_macro_config();
 macro_controller_update_parameters(config);cJSON_Delete(config);
}
void host_macros_refresh(void){
 moonraker_state_t state;moonraker_state_snapshot(&state);bool online=state.moonraker_ok&&state.live_data_ok;uint32_t generation=moonraker_config_generation();
 if(owner!=generation||known_online!=online){owner=generation;known_online=online;macro_controller_init();if(online)catalog();else macro_controller_update_from_objects(NULL);}
}
void host_macros_fixture(int mode){if(mode<=2){fixture_mode=mode;owner=0;}else fail_send=mode==3;}
static bool send(const char *command){attempts++;if(fail_send)return false;bool sent=sender&&sender(command);if(sent)accepted++;return sent;}
void host_macros_show(bool (*callback)(const char *)){sender=callback;host_macros_refresh();ui_macros_show(send);}
void host_macros_info(void){macro_controller_status_t status;macro_controller_status(&status);char saved[8][64]={{0}};size_t size=sizeof(saved);nvs_handle_t handle;bool persisted=false;if(nvs_open("netcfg",NVS_READONLY,&handle)==ESP_OK){if(nvs_get_blob(handle,"mac_favs",saved,&size)==ESP_OK)for(unsigned i=0;i<8;i++)if(!strcmp(saved[i],"CLEAN_NOZZLE"))persisted=true;nvs_close(handle);}printf("MACROS %zu %zu %u %u %u %u %u %u\n",status.count,status.total_count,status.generation,status.truncated,attempts,accepted,macro_controller_is_favorite("CLEAN_NOZZLE"),persisted);fflush(stdout);}
