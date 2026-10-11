#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "dashboard_live_controller.c"
#include "ui_theme.h"
static moonraker_state_t snapshot;
static char banner[64],file_text[160],progress_text[24],readings[7][64];
static bool machine_online,filament_online;
void moonraker_state_snapshot(moonraker_state_t *out){*out=snapshot;}
void moonraker_filament_state_snapshot(moonraker_filament_state_t *out){memset(out,0,sizeof(*out));}
void ui_dashboard_set_banner(const char *s,const char*f,const char*e,const char*p){(void)e;snprintf(banner,sizeof(banner),"%s",s);snprintf(file_text,sizeof(file_text),"%s",f);snprintf(progress_text,sizeof(progress_text),"%s",p);}
void ui_dashboard_set_machine(const char*n,const char*b,const char*c,const char*h,const char*s,const char*f,const char*fan){const char*values[]={n,b,c,h,s,f,fan};for(unsigned i=0;i<7;i++)snprintf(readings[i],sizeof(readings[i]),"%s",values[i]);}
void ui_dashboard_set_active_hotend(const char*n,const char*v){(void)n;assert(!strcmp(v,readings[0]));}
void ui_dashboard_set_machine_connection(bool online){machine_online=online;}
void ui_dashboard_set_filament(bool online,const moonraker_filament_state_t*s){assert(s);filament_online=online;}
int main(void){lv_init();
 for(unsigned theme=0;theme<5;theme++){
  ui_theme_set_active(theme);
  const char*states[]={"shutdown","error","printing","paused","--","ready"};
  for(unsigned i=0;i<6;i++)for(unsigned reachable=0;reachable<2;reachable++){
   snapshot=(moonraker_state_t){.moonraker_ok=reachable,.live_data_ok=false,.nozzle_temp=205,.nozzle_target=210,.bed_temp=60,.bed_target=60,.air_temp=30,.humidity=45,.live_velocity=80,.live_flow=4,.part_fan_speed=50,.progress=.72,.print_duration=9000};
   strcpy(snapshot.printer_state,states[i]);strcpy(snapshot.printer_file,"another-print.gcode");
   dashboard_live_controller_push_banner(true);dashboard_live_controller_push_machine();
   assert(!strstr(banner,"READY")&&!strstr(banner,"PRINTING")&&!strstr(banner,"PAUSED"));
   if(!reachable)assert(strstr(banner,"OFFLINE"));
   else if(i==0)assert(strstr(banner,"SHUTDOWN"));
   for(unsigned j=0;j<7;j++)assert(strstr(readings[j],"--"));
   assert(!machine_online&&!filament_online&&!strcmp(file_text,"No active print")&&!strcmp(progress_text,"--%"));
  }
  snapshot.moonraker_ok=snapshot.live_data_ok=true;strcpy(snapshot.printer_state,"printing");
  dashboard_live_controller_push_banner(true);dashboard_live_controller_push_machine();
  assert(strstr(banner,"PRINTING")&&strstr(readings[0],"205")&&machine_online&&filament_online&&!strcmp(progress_text,"72%"));
 }
 lv_deinit();
 puts("PASS: actual Dashboard banner/machine writers in all five themes; unavailable stale temperatures/job hidden, shutdown stays shutdown, transport-only readiness blocked, reconnect restores readings");
}
