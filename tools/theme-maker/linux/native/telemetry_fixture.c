/* Synthetic history only. Rendering, sampling, gaps and hold behavior are firmware-owned. */
#include <math.h>
#include <stdio.h>
#include "telemetry_history.h"
void host_telemetry_seed(const moonraker_state_t *state,int64_t now_us){
 telemetry_history_reset();
 for(int i=0;i<300;i++){
  moonraker_state_t sample=*state;double phase=i*.12;
  sample.nozzle_temp=sample.nozzle_target+sin(phase)*2;
  sample.bed_temp=sample.bed_target+sin(phase*.3)*.7;
  sample.air_temp=28+sin(phase*.15)*1.5;sample.chamber_temp=29+sin(phase*.2)*2;
  sample.humidity=18+sin(phase*.1)*3;sample.live_velocity=85+sin(phase)*35;
  sample.live_flow=8.4+sin(phase*.7)*3;sample.part_fan_speed=65;
  telemetry_history_sample(&sample,now_us-(int64_t)(299-i)*TELEMETRY_HISTORY_SAMPLE_INTERVAL_US);
 }
}
void host_telemetry_fixture(moonraker_state_t *state,int mode){
 if(mode==1){state->hotend_count=2;snprintf(state->active_hotend,sizeof(state->active_hotend),"extruder");
  for(int i=0;i<2;i++){snprintf(state->hotends[i].object_name,sizeof(state->hotends[i].object_name),i?"extruder1":"extruder");state->hotends[i].temperature=i?185:210;state->hotends[i].target=i?190:210;}
 }else if(mode==2){state->capabilities.has_heated_bed=state->capabilities.has_part_fan=state->capabilities.has_drybox_center_sensor=state->capabilities.has_drybox_environment_sensor=state->capabilities.has_drybox_fan=false;
 }else if(mode==3){state->moonraker_ok=state->live_data_ok=false;
 }else if(mode==4){state->moonraker_ok=true;state->live_data_ok=false;
 }else if(mode==5){state->moonraker_ok=state->live_data_ok=true;state->nozzle_temp=230;state->bed_temp=70;state->live_velocity=42;state->live_flow=3;state->air_temp=35;state->chamber_temp=45;state->humidity=30;
 }else if(mode==6){telemetry_history_reset();}
}
