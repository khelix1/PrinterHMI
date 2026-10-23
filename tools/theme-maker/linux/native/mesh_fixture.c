/* Process-local mesh/profile service. Parsing, rendering and action guards are firmware. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "bed_mesh_controller.h"
#include "moonraker.h"
#include "moonraker_config_controller.h"
#include "printer_connection_policy.h"
#include "ui_bed_mesh.h"
#include "console_controller.h"
typedef struct {char name[64];double phase;} sample_profile_t;
typedef struct {bool initialized,valid;moonraker_profile_t endpoint;sample_profile_t saved[16];size_t count;char active[64];double phase;} sample_mesh_t;
static sample_mesh_t stores[MOONRAKER_CONFIG_MAX_PROFILES];
static uint32_t owner;
static bool known_online,initialized,dirty=true,fail_send;
static int fixture_mode;
static unsigned attempts,accepted;
static bool (*sender)(const char *);
void host_mesh_sender(bool (*callback)(const char *)){sender=callback;}
static sample_mesh_t *current(void){
 int index=moonraker_config_active_profile_index();sample_mesh_t *store=&stores[index];const moonraker_profile_t *endpoint=moonraker_config_profile(index);
 if(!store->initialized||strcmp(store->endpoint.host,endpoint->host)||store->endpoint.port!=endpoint->port||store->endpoint.secure_transport!=endpoint->secure_transport){
  memset(store,0,sizeof(*store));store->initialized=store->valid=true;store->endpoint=*endpoint;store->count=index?1:3;strcpy(store->active,"default");store->phase=index?1.8:0;
  const char *names[]={"default","petg_plate","textured_plate"};for(size_t i=0;i<store->count;i++){strcpy(store->saved[i].name,names[i]);store->saved[i].phase=store->phase+i*.7;}
 }return store;
}
static void publish(bool online){
 sample_mesh_t *store=current();cJSON *root=cJSON_CreateObject(),*mesh=cJSON_AddObjectToObject(root,"bed_mesh"),*profiles=cJSON_AddObjectToObject(mesh,"profiles");
 cJSON_AddStringToObject(mesh,"profile_name",online?store->active:"");
 if(online&&fixture_mode!=1)for(size_t i=0;i<store->count;i++)cJSON_AddObjectToObject(profiles,store->saved[i].name);
 cJSON *matrix=cJSON_AddArrayToObject(mesh,"mesh_matrix");
 if(online&&store->valid&&fixture_mode!=1){int count=fixture_mode==2?35:7;for(int y=0;y<count;y++){cJSON *row=cJSON_CreateArray();for(int x=0;x<count;x++){double value=.09*sin(x*.6+store->phase)+.05*cos(y*.5+store->phase)+(x-y)*.002;cJSON_AddItemToArray(row,cJSON_CreateNumber(value));}cJSON_AddItemToArray(matrix,row);}}
 cJSON *minimum=cJSON_AddArrayToObject(mesh,"mesh_min"),*maximum=cJSON_AddArrayToObject(mesh,"mesh_max");cJSON_AddItemToArray(minimum,cJSON_CreateNumber(10));cJSON_AddItemToArray(minimum,cJSON_CreateNumber(10));cJSON_AddItemToArray(maximum,cJSON_CreateNumber(270));cJSON_AddItemToArray(maximum,cJSON_CreateNumber(250));
 bed_mesh_controller_merge_status(root);cJSON_Delete(root);
}
void host_mesh_refresh(void){
 if(!initialized){bed_mesh_controller_init();initialized=true;}
 moonraker_state_t state;moonraker_state_snapshot(&state);bool online=state.moonraker_ok&&state.live_data_ok;uint32_t generation=moonraker_config_generation();
 if(owner!=generation||known_online!=online||dirty){owner=generation;known_online=online;dirty=false;publish(online);if(ui_bed_mesh_is_open())ui_bed_mesh_refresh();}
}
void host_mesh_fixture(int mode){if(mode<=2){fixture_mode=mode;dirty=true;}else fail_send=mode==3;}
bool host_mesh_send(const char *command){
 attempts++;moonraker_state_t state;moonraker_state_snapshot(&state);if(fail_send||printer_idle_reason(&state)[0])return false;
 sample_mesh_t *store=current();char operation[16]={0},name[64]={0};
 bool calibration=!strcmp(command,"BED_MESH_CALIBRATE");int parsed=sscanf(command,"BED_MESH_PROFILE %15[^=]=%63[a-zA-Z0-9_.-]",operation,name);size_t index=store->count;
 if(!calibration){if(parsed!=2)return false;for(size_t i=0;i<store->count;i++)if(!strcmp(store->saved[i].name,name)){index=i;break;}
  if((!strcmp(operation,"LOAD")||!strcmp(operation,"REMOVE"))&&index==store->count)return false;
  if(!strcmp(operation,"SAVE")&&index==store->count&&store->count==16)return false;
 }
 console_controller_add_command(command);if(!sender||!sender(command))return false;accepted++;
 if(calibration){store->phase+=.35;store->active[0]=0;store->valid=true;fixture_mode=0;}
 else if(!strcmp(operation,"LOAD")){store->phase=store->saved[index].phase;strcpy(store->active,name);store->valid=true;}
 else if(!strcmp(operation,"SAVE")){if(index==store->count)store->count++;strcpy(store->saved[index].name,name);store->saved[index].phase=store->phase;strcpy(store->active,name);}
 else if(!strcmp(operation,"REMOVE")){memmove(&store->saved[index],&store->saved[index+1],(store->count-index-1)*sizeof(store->saved[0]));store->count--;}
 dirty=true;return true;
}
void host_mesh_info(void){bed_mesh_snapshot_t mesh;float values[BED_MESH_MAX_ROWS*BED_MESH_MAX_COLS];bed_mesh_profile_name_t names[BED_MESH_MAX_PROFILES];bed_mesh_controller_snapshot(&mesh,values,sizeof(values)/sizeof(values[0]),names,BED_MESH_MAX_PROFILES);printf("MESH %u %u %u %zu %u %u %u\n",mesh.valid,mesh.rows,mesh.cols,mesh.profile_count,mesh.truncated,attempts,accepted);fflush(stdout);}
