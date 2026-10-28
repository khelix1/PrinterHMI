#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "custom_theme.h"
#include "ui_printer_popups.c"
bool custom_theme_color_override(uint32_t a,uint32_t b,uint32_t c,uint32_t *d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_metric_override(int32_t a,int32_t b,int32_t c,int32_t *d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_accent_override(uint8_t a,uint32_t *b){(void)a;(void)b;return false;}
bool custom_theme_surface_opacity(uint8_t *a){(void)a;return false;}
static moonraker_state_t state;
static moonraker_filament_state_t sensors;
static uint32_t generation=1;
static unsigned sends,mode,state_reads,sensor_reads;
static bool accepted=true;
static char sent[128];
uint32_t moonraker_config_generation(void){return generation;}
void moonraker_state_snapshot(moonraker_state_t *out){state_reads++;*out=state;}
void moonraker_filament_state_snapshot(moonraker_filament_state_t *out){sensor_reads++;*out=sensors;}
static void legacy_send(const char *command){++sends;snprintf(sent,sizeof(sent),"%s",command);}
static void click(lv_obj_t *o){assert(o);lv_obj_send_event(o,LV_EVENT_CLICKED,NULL);}
static void open(void){ui_printer_popups_show_filament_sensors(NULL,&sensors);assert(s_filament_list_popup);}
static bool send_checked(const char *command){
 ++sends;snprintf(sent,sizeof(sent),"%s",command);
 unsigned during=mode;mode=0;
 if(during==1)click(s_filament_toggle_buttons[0]);
 if(during==2)close_filament_list_cb(NULL);
 if(during==3){close_filament_list_cb(NULL);open();}
 if(during==4){++generation;close_filament_list_cb(NULL);open();}
 if(during==5)++generation;
 return accepted;
}
static void reset(void){
 close_filament_list_cb(NULL);++generation;accepted=true;mode=0;
 memset(s_filament_requests,0,sizeof(s_filament_requests));
 memset(&state,0,sizeof(state));state.live_data_ok=state.moonraker_ok=true;strcpy(state.printer_state,"standby");
 memset(&sensors,0,sizeof(sensors));sensors.discovered=true;sensors.sensor_count=sensors.total_count=4;
 for(unsigned i=0;i<4;i++){snprintf(sensors.sensors[i].object_name,sizeof(sensors.sensors[i].object_name),"filament_switch_sensor sensor%u",i);sensors.sensors[i].enabled=sensors.sensors[i].status_known=sensors.sensors[i].filament_detected=true;}
 ui_printer_popups_set_control_sender(send_checked);
}
static unsigned timers(void){unsigned n=0;for(lv_timer_t*t=lv_timer_get_next(NULL);t;t=lv_timer_get_next(t))++n;return n;}
static uint16_t raster[1024*600];static int width=1024;
static void flush(lv_display_t *d,const lv_area_t *a,uint8_t *p){uint16_t *v=(uint16_t*)p;for(int y=a->y1;y<=a->y2;y++)for(int x=a->x1;x<=a->x2;x++)raster[y*width+x]=*v++;lv_display_flush_ready(d);}
static void snapshot(int theme,const char *name){const char *folder=getenv("SENSOR_STATUS_SCREENSHOTS");if(!folder)return;lv_refr_now(NULL);char path[512];snprintf(path,sizeof(path),"%s/%s-theme%d.ppm",folder,name,theme);FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n1024 600\n255\n");for(int i=0;i<1024*600;i++){uint16_t v=raster[i];unsigned char c[3]={(v>>11)*255/31,((v>>5)&63)*255/63,(v&31)*255/31};fwrite(c,1,3,f);}fclose(f);}
static void unrelated_poll(lv_timer_t *timer){(void)timer;}
static void safety(void){
 reset();unsigned base=timers();open();assert(timers()==base+1);unsigned before=sends;
 lv_timer_t *other=lv_timer_create(unrelated_poll,60000,NULL);
 unsigned state_before=state_reads,sensors_before=sensor_reads;
 refresh_filament_list(other);assert(state_reads==state_before&&sensor_reads==sensors_before);
 refresh_filament_list(s_filament_refresh_timer);assert(state_reads==state_before+1&&sensor_reads==sensors_before+1);
 lv_timer_delete(other);
 accepted=false;click(s_filament_toggle_buttons[0]);assert(sends==before+1&&filament_request(sensors.sensors[0].object_name,generation)<0);
 assert(strstr(lv_label_get_text(s_filament_status_labels[0]),"Send failed"));assert(!lv_obj_has_state(s_filament_toggle_buttons[0],LV_STATE_DISABLED));
 refresh_filament_list(NULL);assert(strstr(lv_label_get_text(s_filament_status_labels[0]),"Send failed"));
 accepted=true;click(s_filament_toggle_buttons[0]);assert(sends==before+2&&!strcmp(sent,"SET_FILAMENT_SENSOR SENSOR=sensor0 ENABLE=0"));
 click(s_filament_toggle_buttons[0]);assert(sends==before+2);assert(!strcmp(lv_label_get_text(s_filament_toggle_labels[0]),"Updating"));
 close_filament_list_cb(NULL);assert(timers()==base&&!s_filament_send&&!s_filament_row_count&&!s_filament_toggle_buttons[0]);open();click(s_filament_toggle_buttons[0]);assert(sends==before+2);
 for(unsigned i=0;i<12;i++){refresh_filament_list(NULL);}
 assert(lv_obj_has_state(s_filament_toggle_buttons[0],LV_STATE_DISABLED));
 lv_tick_inc(5000);refresh_filament_list(NULL);assert(strstr(lv_label_get_text(s_filament_status_labels[0]),"unconfirmed"));click(s_filament_toggle_buttons[0]);assert(sends==before+3);
 sensors.sensors[0].enabled=false;refresh_filament_list(NULL);assert(!lv_obj_has_state(s_filament_toggle_buttons[0],LV_STATE_DISABLED));assert(!strcmp(lv_label_get_text(s_filament_toggle_labels[0]),"Enable"));
 reset();open();moonraker_filament_sensor_t swap=sensors.sensors[0];sensors.sensors[0]=sensors.sensors[2];sensors.sensors[2]=swap;click(s_filament_toggle_buttons[0]);assert(!strcmp(sent,"SET_FILAMENT_SENSOR SENSOR=sensor0 ENABLE=0"));
 for(unsigned during=1;during<=5;during++){reset();open();mode=during;before=sends;click(s_filament_toggle_buttons[0]);assert(sends==before+1&&!s_filament_dispatching);
  if(during==2)assert(!s_filament_list_popup&&!s_filament_refresh_timer&&!s_filament_send);
  else {refresh_filament_list(NULL);click(s_filament_toggle_buttons[0]);assert(sends==before+(during==4?2:1));}
 }
 reset();open();before=sends;state.live_data_ok=false;click(s_filament_toggle_buttons[0]);assert(sends==before);state.live_data_ok=true;state.moonraker_ok=false;click(s_filament_toggle_buttons[0]);assert(sends==before);
 reset();open();before=sends;++generation;click(s_filament_toggle_buttons[0]);assert(sends==before);open();assert(s_filament_owner==generation);click(s_filament_toggle_buttons[0]);assert(sends==before+1);
 reset();open();before=sends;sensors.discovered=false;click(s_filament_toggle_buttons[0]);assert(sends==before);
 reset();ui_printer_popups_set_control_sender(NULL);ui_printer_popups_show_filament_sensors(legacy_send,&sensors);s_send_gcode_cb=NULL;before=sends;click(s_filament_toggle_buttons[0]);assert(sends==before+1);
 reset();open();before=sends;sensors.sensor_count=0;click(s_filament_toggle_buttons[0]);assert(sends==before);
 reset();strcpy(sensors.sensors[0].object_name,"filament_switch_sensor bad name");open();before=sends;click(s_filament_toggle_buttons[0]);assert(sends==before);
 reset();open();ui_printer_popups_set_control_sender(NULL);s_send_gcode_cb=NULL;before=sends;click(s_filament_toggle_buttons[0]);assert(sends==before);
 reset();open();lv_obj_t*first=s_filament_list_popup;lv_timer_t*t=s_filament_refresh_timer;open();assert(s_filament_list_popup==first&&s_filament_refresh_timer==t);lv_obj_delete(first);assert(!s_filament_refresh_timer&&!s_filament_send&&!s_filament_toggle_buttons[0]);
 reset();close_filament_list_cb(NULL);
}
int main(void){
 lv_init();lv_display_t*d=lv_display_create(1024,600);static uint8_t buffer[1024*40*2];lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);lv_display_set_buffers(d,buffer,NULL,sizeof(buffer),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(d,flush);
 for(unsigned theme=0;theme<5;theme++)for(unsigned density=0;density<3;density++)for(unsigned large=0;large<2;large++){
  ui_theme_set_active(theme);ui_theme_set_density(density);ui_theme_set_accessibility((ui_accessibility_t){.large_text=large});safety();
  reset();open();accepted=false;click(s_filament_toggle_buttons[0]);lv_obj_update_layout(s_filament_list_popup);
  if(density==1&&large)snapshot(theme,"send-failed");
  lv_obj_t*body=lv_obj_get_child(s_filament_list_popup,1);lv_obj_t*footer=lv_obj_get_child(s_filament_list_popup,-1);lv_area_t a,b;lv_obj_get_coords(footer,&a);lv_obj_scroll_to_y(body,400,LV_ANIM_OFF);lv_obj_update_layout(s_filament_list_popup);lv_obj_get_coords(footer,&b);assert(!memcmp(&a,&b,sizeof(a)));
  for(unsigned i=0;i<4;i++){lv_obj_t*l=s_filament_status_labels[i],*info=lv_obj_get_parent(l);assert(lv_obj_get_width(l)<=lv_obj_get_width(info));assert(lv_obj_get_height(s_filament_toggle_buttons[i])>=48);}
  close_filament_list_cb(NULL);
 }
 puts("PASS: filament sensor checked dispatch, rejected retry, name binding, accepted-request reopen guard, elapsed timeout, live acknowledgement, reentrant/close/reopen/switch callbacks, offline/unsafe/missing guards, timer cleanup and all five themes");return 0;
}
