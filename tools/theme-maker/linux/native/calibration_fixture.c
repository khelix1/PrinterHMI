/* Offline discovery and delayed Console replies; calibration UI/controllers are shared. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lvgl.h"
#include "cJSON.h"
#include "moonraker.h"
#include "moonraker_config_controller.h"
#include "printer_connection_policy.h"
#include "calibration_capability_controller.h"
#include "calibration_session_controller.h"
#include "motion_diagnostics_controller.h"
#include "console_controller.h"
#include "ui_calibration.h"
static uint32_t owner,pending_owner,pending_session;
static bool known_online;
static int mode,pending_mode,axis_samples;
static calibration_session_kind_t pending_kind;
static lv_timer_t *pending_timer;
static unsigned attempts,accepted;
static bool (*sender)(const char *);
void host_calibration_home(void);
void host_calibration_step(double);
static void result(lv_timer_t *timer){
 pending_timer=NULL;lv_timer_delete(timer);calibration_session_snapshot_t session;calibration_session_controller_snapshot(&session);
 if(pending_owner!=moonraker_config_generation()||session.generation!=pending_session||session.kind!=pending_kind)return;
 if(pending_mode==3){console_controller_add(CONSOLE_ENTRY_ERROR,"Simulated calibration failed. Retry when ready.");return;}
 switch(pending_kind){
 case CALIBRATION_SESSION_PID:console_controller_add(CONSOLE_ENTRY_RESPONSE,"// PID parameters: pid_Kp=22.2 pid_Ki=1.08 pid_Kd=114.0%s",pending_mode==7?"":"; SAVE_CONFIG to persist");break;
 case CALIBRATION_SESSION_PROBE_ACCURACY:console_controller_add(CONSOLE_ENTRY_RESPONSE,"// probe accuracy results: maximum 1.509000, minimum 1.501000, range 0.008000, average 1.505000, median 1.505000, standard deviation 0.002000");break;
 case CALIBRATION_SESSION_SCREWS_TILT:
  console_controller_add(CONSOLE_ENTRY_RESPONSE,"// front_left (base) : x=30.0, y=30.0, z=1.50000");
  console_controller_add(CONSOLE_ENTRY_RESPONSE,"// front_right : x=240.0, y=30.0, z=1.52000 : adjust CW 00:10");
  console_controller_add(CONSOLE_ENTRY_RESPONSE,"// rear_right : x=240.0, y=230.0, z=1.49000 : adjust CCW 00:05");break;
 case CALIBRATION_SESSION_INPUT_SHAPER:console_controller_add(CONSOLE_ENTRY_RESPONSE,"// shaper_type_x=mzv shaper_freq_x=39.6, shaper_type_y=2hump_ei shaper_freq_y=66.0; SAVE_CONFIG to persist");break;
 case CALIBRATION_SESSION_RESONANCE_TEST:console_controller_add(CONSOLE_ENTRY_RESPONSE,"// resonance data written to local sample (no real file)");break;
 case CALIBRATION_SESSION_ACCELEROMETER_CHECK:console_controller_add(CONSOLE_ENTRY_RESPONSE,"// axes noise for xyz-axis accelerometer: 0.010000, 0.012000, 0.009000");break;
 case CALIBRATION_SESSION_PROBE_Z:console_controller_add(CONSOLE_ENTRY_RESPONSE,"// probe: adjust TESTZ, then ACCEPT");break;
 case CALIBRATION_SESSION_AXIS_TWIST:console_controller_add(CONSOLE_ENTRY_RESPONSE,"// axis twist: adjust TESTZ at the first sample, then ACCEPT");break;
 case CALIBRATION_SESSION_CUSTOM:console_controller_add(CONSOLE_ENTRY_RESPONSE,"// Custom calibration sample complete; SAVE_CONFIG to persist");break;
 default:break;
 }
}
void host_calibration_refresh(void){
 moonraker_state_t state;moonraker_state_snapshot(&state);bool online=state.moonraker_ok&&state.live_data_ok;uint32_t generation=moonraker_config_generation();
 if(owner!=generation||known_online!=online){owner=generation;known_online=online;calibration_session_controller_init();calibration_session_controller_reset();calibration_capability_controller_reset();
  if(online&&moonraker_config_active_profile_index()==0){cJSON *status=cJSON_Parse("{\"gcode\":{\"commands\":{\"AXIS_TWIST_COMPENSATION_CALIBRATE\":{},\"PROBE_ACCURACY\":{}}}}");calibration_capability_controller_merge_status(status);cJSON_Delete(status);}
 }calibration_session_controller_poll();
}
void host_calibration_fixture(int value,moonraker_state_t *state){if(value==4)state->homed_axes[0]=0;else if(value==5)strcpy(state->homed_axes,"xyz");else if(value==6){cJSON *status=cJSON_Parse("{\"tmc2209 stepper_x\":{\"temperature\":87,\"drv_status\":{\"otpw\":true}}}");motion_diagnostics_controller_merge_status(status);cJSON_Delete(status);}else mode=value;}
static bool send(const char *command){
 attempts++;moonraker_state_t state;moonraker_state_snapshot(&state);if(mode==1||printer_idle_reason(&state)[0])return false;
 if(!sender||!sender(command))return false;
 accepted++;
 if(!strncmp(command,"G28",3))host_calibration_home();
 if(!strncmp(command,"TESTZ Z=",8)){host_calibration_step(strtod(command+8,NULL));console_controller_add(CONSOLE_ENTRY_RESPONSE,"// probe: manual adjustment accepted");return true;}
 if(!strcmp(command,"ABORT")){console_controller_add(CONSOLE_ENTRY_RESPONSE,"// Calibration aborted (simulated)");return true;}
 calibration_session_snapshot_t session;calibration_session_controller_snapshot(&session);
 if(!strcmp(command,"ACCEPT")){if(session.kind==CALIBRATION_SESSION_AXIS_TWIST&&++axis_samples<3)console_controller_add(CONSOLE_ENTRY_RESPONSE,"// axis twist: next manual sample");else console_controller_add(CONSOLE_ENTRY_RESPONSE,"// %s calibration complete; SAVE_CONFIG to persist",session.kind==CALIBRATION_SESSION_AXIS_TWIST?"axis twist":"probe z_offset");return true;}
 if(strstr(command,"AXIS_TWIST_COMPENSATION_CALIBRATE"))axis_samples=0;
 if(!strncmp(command,"SET_VELOCITY_LIMIT",18)){const char *keys[]={"VELOCITY=","ACCEL=","SQUARE_CORNER_VELOCITY=","MINIMUM_CRUISE_RATIO="},*fields[]={"max_velocity","max_accel","square_corner_velocity","minimum_cruise_ratio"};cJSON *status=cJSON_CreateObject(),*toolhead=cJSON_AddObjectToObject(status,"toolhead");for(unsigned i=0;i<4;i++){char token[64];snprintf(token,sizeof(token)," %s",keys[i]);const char *value=strstr(command,token);if(value)cJSON_AddNumberToObject(toolhead,fields[i],strtod(value+strlen(token),NULL));}motion_diagnostics_controller_merge_status(status);cJSON_Delete(status);return true;}
 if(!strcmp(command,"G28")||strstr(command,"Z_TILT_ADJUST")||strstr(command,"QUAD_GANTRY_LEVEL"))return true;
 if(session.kind!=CALIBRATION_SESSION_KIND_NONE&&strcmp(command,"SAVE_CONFIG")&&mode!=2){if(pending_timer)lv_timer_delete(pending_timer);pending_owner=owner;pending_session=session.generation;pending_kind=session.kind;pending_mode=mode;pending_timer=lv_timer_create(result,600,NULL);}
 return true;
}
void host_calibration_show(void (*mesh)(void),bool (*callback)(const char *)){sender=callback;host_calibration_refresh();ui_calibration_show(mesh,send);}
void host_calibration_info(void){calibration_capabilities_t caps;calibration_capability_controller_snapshot(&caps);calibration_session_snapshot_t session;calibration_session_controller_snapshot(&session);printf("CALIBRATION %u %u %u %u %u %u %u %u\n",attempts,accepted,session.kind,session.status,session.completed,session.save_available,session.generation,(unsigned)caps.tool_count);fflush(stdout);}
