#include "printer_simulation.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

bool printer_simulation_start_file(moonraker_state_t *s,const char *path)
{
 if(!s || !path || !path[0] || strlen(path)>=sizeof(s->printer_file) || !s->moonraker_ok || !s->live_data_ok)return false;
 if(strcmp(s->printer_state,"standby") && strcmp(s->printer_state,"ready") && strcmp(s->printer_state,"complete") && strcmp(s->printer_state,"cancelled"))return false;
 for(const unsigned char *p=(const unsigned char *)path;*p;p++)if(*p<32 || *p==127 || *p=='"')return false;
 snprintf(s->printer_file,sizeof(s->printer_file),"%s",path);strcpy(s->printer_state,"printing");
 s->progress=s->print_duration=0;s->current_layer=0;s->total_layer=100;s->current_gcode_z=s->toolhead_z=.2;
 s->nozzle_target=210;s->bed_target=60;s->part_fan_speed=65;return true;
}

bool printer_simulation_apply(moonraker_state_t *state,bool excluded[2],bool *sensor_enabled,const char *command){
 if(!state||!excluded||!sensor_enabled||!command||!command[0])return false;
 double v;
 if(!strncmp(command,"SDCARD_PRINT_FILE FILENAME=\"",28)){
  const char *path=command+28;size_t length=strlen(path);char file[sizeof(state->printer_file)];
  if(length<2 || length-1>=sizeof(file) || path[length-1]!='"')return false;
  memcpy(file,path,length-1);file[length-1]=0;
  if(!printer_simulation_start_file(state,file))return false;
  excluded[0]=excluded[1]=false;
 }
 else if(!strcmp(command,"EXCLUDE_OBJECT NAME=part_1"))excluded[0]=true;
 else if(!strcmp(command,"EXCLUDE_OBJECT NAME=part_2"))excluded[1]=true;
 else if(strstr(command,"SET_FILAMENT_SENSOR ")&&strstr(command,"ENABLE="))*sensor_enabled=strstr(command,"ENABLE=1")!=NULL;
 else if(sscanf(command,"HMI_FILAMENT_HEAT TEMP=%lf",&v)==1){if(!isfinite(v)||v<0||v>300)return false;state->nozzle_target=v;}
 else if(sscanf(command,"SET_HEATER_TEMPERATURE HEATER=extruder TARGET=%lf",&v)==1){if(!isfinite(v)||v<0||v>300)return false;state->nozzle_target=v;}
 else if(sscanf(command,"M104 S%lf",&v)==1){if(!isfinite(v)||v<0||v>300)return false;state->nozzle_target=v;}
 else if(sscanf(command,"M140 S%lf",&v)==1){if(!isfinite(v)||v<0||v>120)return false;state->bed_target=v;}
 else if(sscanf(command,"M106 S%lf",&v)==1){if(!isfinite(v)||v<0||v>255)return false;state->part_fan_speed=v*100/255;}
 else if(sscanf(command,"M220 S%lf",&v)==1){if(!isfinite(v)||v<10||v>300)return false;state->speed_factor=v;}
 else if(sscanf(command,"M221 S%lf",&v)==1){if(!isfinite(v)||v<10||v>300)return false;state->flow_factor=v;}
 else if(!strcmp(command,"DRY_PLA")||!strcmp(command,"DRY_PETG")){state->drybox_selected_program=state->drybox_active_program=!strcmp(command,"DRY_PLA")?1:2;state->heater_target=state->drybox_active_program==1?45:55;state->heater_on=true;state->drybox_fan_speed=80;}
 else if(!strcmp(command,"DRY_HOLD")){state->drybox_active_program=3;state->heater_on=false;state->heater_target=0;state->drybox_fan_speed=40;}
 else if(!strcmp(command,"DRY_RESUME")){if(state->drybox_selected_program){state->drybox_active_program=state->drybox_selected_program;state->heater_target=state->drybox_active_program==1?45:55;state->heater_on=true;state->drybox_fan_speed=80;}}
 else if(!strcmp(command,"DRY_STOP")){state->drybox_active_program=0;state->heater_target=0;state->heater_on=false;state->drybox_fan_speed=0;}
 else if(!strcmp(command,"PAUSE"))strcpy(state->printer_state,"paused");
 else if(!strcmp(command,"RESUME"))strcpy(state->printer_state,"printing");
 else if(!strcmp(command,"G28")||!strcmp(command,"HOME_ALL")){state->toolhead_x=state->toolhead_y=state->toolhead_z=0;snprintf(state->homed_axes,sizeof(state->homed_axes),"xyz");}
 else if(sscanf(command,"SET_GCODE_OFFSET Z_ADJUST=%lf",&v)==1){if(!isfinite(v)||v<-10||v>10)return false;state->z_offset+=v;}
 else if(!strcmp(command,"SET_GCODE_OFFSET Z=0 MOVE=1"))state->z_offset=0;
 else if(strstr(command,"G91\nG1 ")){char axis;if(sscanf(command,"G91\nG1 %c%lf",&axis,&v)==2){double *position=axis=='X'?&state->toolhead_x:axis=='Y'?&state->toolhead_y:axis=='Z'?&state->toolhead_z:NULL;if(!isfinite(v)||fabs(v)>1000)return false;if(position)*position+=v;}}
 else if(!strcmp(command,"M112")){strcpy(state->printer_state,"shutdown");state->nozzle_target=state->bed_target=state->part_fan_speed=state->heater_target=0;state->heater_on=false;state->drybox_active_program=0;state->drybox_fan_speed=0;}
 else if(!strcmp(command,"FIRMWARE_RESTART"))strcpy(state->printer_state,"standby");
 else if(!strcmp(command,"CANCEL_PRINT")){snprintf(state->printer_state,sizeof(state->printer_state),"cancelled");state->nozzle_target=state->bed_target=state->part_fan_speed=0;}
 else return false;
 return true;
}

bool printer_simulation_scenario(moonraker_state_t *s,const char *scenario)
{
 if(!s||!scenario)return false;
 const char *names[]={"idle","printing","paused","complete","offline","error"};
 int index=-1;for(unsigned i=0;i<6;i++)if(!strcmp(scenario,names[i]))index=(int)i;
 if(index<0)return false;
 s->moonraker_ok=s->live_data_ok=index!=4;
 snprintf(s->printer_state,sizeof(s->printer_state),"%s",index==0?"standby":index==4?"offline":scenario);
 if(index==1||index==2){
  snprintf(s->printer_file,sizeof(s->printer_file),"Spool holder.gcode");
  s->progress=.35;s->print_duration=630;s->current_layer=35;s->total_layer=100;
  s->nozzle_target=210;s->bed_target=60;s->part_fan_speed=65;
 }else if(index==3){s->progress=1;s->current_layer=s->total_layer;s->nozzle_target=s->bed_target=s->part_fan_speed=0;}
 else if(index==0){s->printer_file[0]=0;s->progress=s->print_duration=0;s->current_layer=0;s->nozzle_target=s->bed_target=s->part_fan_speed=0;}
 else if(index==5){s->nozzle_target=s->bed_target=s->part_fan_speed=s->heater_target=s->drybox_fan_speed=0;s->heater_on=false;s->drybox_active_program=0;}
 s->live_velocity=index==1?85:0;s->live_flow=index==1?8.4:0;
 return true;
}
static double approach(double current,double target,double rate,double dt)
{
 double distance=target-current,step=rate*dt;
 return fabs(distance)<=step?target:current+copysign(step,distance);
}
void printer_simulation_advance(moonraker_state_t *s,double seconds)
{
 if(!s||!isfinite(seconds)||seconds<=0||seconds>86400||!s->moonraker_ok||!s->live_data_ok)return;
 s->nozzle_temp=approach(s->nozzle_temp,s->nozzle_target>25?s->nozzle_target:25,s->nozzle_target>s->nozzle_temp?3:1,seconds);
 s->bed_temp=approach(s->bed_temp,s->bed_target>25?s->bed_target:25,s->bed_target>s->bed_temp?.6:.2,seconds);
 if(s->capabilities.has_drybox_heater){
  s->chamber_temp=approach(s->chamber_temp,s->heater_on?s->heater_target:25,s->heater_on?.08:.025,seconds);
  s->air_temp=approach(s->air_temp,s->heater_on?fmax(25,s->chamber_temp-3):24,.04,seconds);
  s->humidity=approach(s->humidity,s->heater_on?15:32,s->heater_on?.02:.004,seconds);
 }
 if(!strcmp(s->printer_state,"printing")){
  double factor=s->speed_factor/100.;if(!isfinite(factor)||factor<.1)factor=1;
  s->print_duration+=seconds;s->progress=fmin(1,s->progress+seconds*factor/1800.);
  s->current_layer=(int)ceil(s->progress*s->total_layer);
  s->current_gcode_z=s->toolhead_z=s->current_layer*.2;
  s->live_velocity=85*factor;s->live_flow=8.4*s->flow_factor/100.;
  if(s->progress>=1){snprintf(s->printer_state,sizeof(s->printer_state),"complete");s->live_velocity=s->live_flow=0;s->nozzle_target=s->bed_target=s->part_fan_speed=0;}
 }else s->live_velocity=s->live_flow=0;
}

void printer_simulation_objects(moonraker_exclude_state_t *out,const bool excluded[2])
{
 if(!out || !excluded)return;
 out->available=true;out->truncated=false;out->bed_bounds_valid=true;out->object_count=2;
 out->bed_min_x=out->bed_min_y=0;out->bed_max_x=280;out->bed_max_y=260;out->current_object[0]=0;
 for(unsigned i=0;i<2;i++) {
  moonraker_exclude_object_t *object=&out->objects[i];memset(object,0,sizeof(*object));
  snprintf(object->name,sizeof(object->name),"part_%u",i+1);object->excluded=excluded[i];object->has_center=true;
  object->center=(moonraker_exclude_point_t){80+i*100,120};object->polygon_count=4;
  for(unsigned j=0;j<4;j++)object->polygon[j]=(moonraker_exclude_point_t){60+i*100+(j==1||j==2?40:0),100+(j>=2?40:0)};
  if(!excluded[i] && !out->current_object[0]){object->current=true;snprintf(out->current_object,sizeof(out->current_object),"%s",object->name);}
 }
}
