/* Offline object discovery/status and endstop replies. UI and controllers stay shared. */
#include <stdio.h>
#include <string.h>
#include "cJSON.h"
#include "device_catalog_controller.h"
#include "endstop_status_controller.h"
#include "motion_diagnostics_controller.h"
#include "moonraker.h"
#include "moonraker_config_controller.h"
#include "lvgl.h"
static uint32_t owner;
static bool known_online;
static int fixture_mode,query_mode;
static uint32_t pending_id,pending_owner;
static int pending_mode;
static unsigned requests;
static lv_timer_t *pending_timer;
static void catalog(void){
 device_catalog_controller_init();
 cJSON *objects=cJSON_CreateArray();
 const char *names[]={"extruder","heater_bed","fan","heater_generic drybox_heater","fan_generic drybox_fan","temperature_sensor drybox_center","sht3x drybox_env","filament_switch_sensor runout","toolhead","tmc2209 stepper_x","tmc2209 stepper_y","tmc2209 stepper_z","output_pin case_light","neopixel status","power printer","probe","bed_mesh","gcode_macro START_PRINT","screws_tilt_adjust","z_tilt","axis_twist_compensation","input_shaper","adxl345","bltouch"};
 if(fixture_mode!=1){
  if(moonraker_config_active_profile_index()==0){for(unsigned i=0;i<sizeof(names)/sizeof(names[0]);i++)cJSON_AddItemToArray(objects,cJSON_CreateString(names[i]));}
  else {for(unsigned i=0;i<3;i++)cJSON_AddItemToArray(objects,cJSON_CreateString(names[i]));cJSON_AddItemToArray(objects,cJSON_CreateString("toolhead"));}
 }
 if(fixture_mode==2){for(int i=0;i<110;i++){char name[80];snprintf(name,sizeof(name),"temperature_sensor sample_%03d",i);cJSON_AddItemToArray(objects,cJSON_CreateString(name));}}
 device_catalog_controller_update_from_objects(objects);motion_diagnostics_controller_init();motion_diagnostics_controller_update_objects(objects);cJSON_Delete(objects);
 cJSON *status=cJSON_Parse("{\"extruder\":{\"temperature\":210,\"target\":210},\"tmc2209 stepper_x\":{\"run_current\":0.65,\"hold_current\":0.2},\"tmc2209 stepper_y\":{\"run_current\":0.8,\"hold_current\":0.25},\"tmc2209 stepper_z\":{\"run_current\":0.6,\"hold_current\":0.2},\"output_pin case_light\":{\"value\":1},\"neopixel status\":{\"color_data\":[[0.2,0.8,0.3,0]]}}");
 device_catalog_controller_merge_status(status);motion_diagnostics_controller_merge_status(status);cJSON_Delete(status);
 cJSON *config=cJSON_Parse("{\"printer\":{\"kinematics\":\"cartesian\"},\"stepper_x\":{\"rotation_distance\":\"40\"},\"stepper_y\":{\"rotation_distance\":\"40\"},\"stepper_z\":{\"rotation_distance\":\"8\"}}");motion_diagnostics_controller_update_config(config);cJSON_Delete(config);
 status=cJSON_Parse("{\"toolhead\":{\"max_velocity\":200,\"max_accel\":2500,\"square_corner_velocity\":5,\"minimum_cruise_ratio\":0.5}}");motion_diagnostics_controller_merge_status(status);cJSON_Delete(status);
 status=cJSON_Parse("{\"tmc2209 stepper_x\":{\"temperature\":45.5,\"drv_status\":{}},\"tmc2209 stepper_y\":{\"temperature\":43,\"drv_status\":{}},\"tmc2209 stepper_z\":{\"temperature\":40,\"drv_status\":{}}}");motion_diagnostics_controller_merge_status(status);cJSON_Delete(status);
}
void host_devices_refresh(void){
 moonraker_state_t state;moonraker_state_snapshot(&state);bool online=state.moonraker_ok&&state.live_data_ok;
 uint32_t generation=moonraker_config_generation();
 if(owner!=generation||known_online!=online){owner=generation;known_online=online;if(online)catalog();else {device_catalog_controller_reset();motion_diagnostics_controller_reset();}}
 if(online){cJSON *status=cJSON_CreateObject(),*hotend=cJSON_AddObjectToObject(status,"extruder");cJSON_AddNumberToObject(hotend,"temperature",state.nozzle_temp);cJSON_AddNumberToObject(hotend,"target",state.nozzle_target);device_catalog_controller_merge_status(status);cJSON_Delete(status);}
}
void host_devices_fixture(int mode){
 if(mode<=2){fixture_mode=mode;owner=0;}
 else if(mode==3){cJSON *status=cJSON_Parse("{\"tmc2209 stepper_x\":{\"run_current\":0.9,\"hold_current\":0.3},\"output_pin case_light\":{\"value\":0}}");device_catalog_controller_merge_status(status);motion_diagnostics_controller_merge_status(status);cJSON_Delete(status);}
 else query_mode=mode-10;
}
bool moonraker_live_websocket_connected(void){moonraker_state_t state;moonraker_state_snapshot(&state);return state.moonraker_ok&&state.live_data_ok;}
static void reply(lv_timer_t *timer){
 uint32_t id=pending_id,generation=pending_owner;pending_timer=NULL;lv_timer_delete(timer);
 if(generation!=moonraker_config_generation())return;
 cJSON *root=cJSON_CreateObject();cJSON_AddNumberToObject(root,"id",id);
 cJSON *result=cJSON_AddObjectToObject(root,"result");
 if(pending_mode==3){cJSON_AddObjectToObject(root,"error");}
 else if(pending_mode!=4){cJSON_AddStringToObject(result,"x",pending_mode==1?"TRIGGERED":"open");cJSON_AddStringToObject(result,"y","open");cJSON_AddStringToObject(result,"z","TRIGGERED");}
 endstop_status_controller_merge(root,generation);cJSON_Delete(root);
}
bool moonraker_live_websocket_request_endstops(uint32_t id){
 requests++;if(query_mode==2)return false;
 if(pending_timer){lv_timer_delete(pending_timer);pending_timer=NULL;}
 if(query_mode==5)return true;
 pending_mode=query_mode;pending_id=id;pending_owner=moonraker_config_generation();pending_timer=lv_timer_create(reply,150,NULL);return true;
}
void host_devices_info(void){device_catalog_status_t status;device_catalog_controller_status(&status);endstop_status_snapshot_t stops;endstop_status_controller_snapshot(&stops);printf("DEVICES %zu %zu %u %u %u %u %zu %u %u\n",status.stored_count,status.total_object_count,status.generation,status.value_generation,status.truncated,requests,stops.count,stops.valid,stops.waiting);fflush(stdout);}
