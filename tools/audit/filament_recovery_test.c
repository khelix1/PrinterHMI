#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "custom_theme.h"
#include "printer_ui_controller.h"
#include "ui_filament_recovery.c"
static moonraker_state_t fixture;
static moonraker_filament_state_t sensors;
static uint32_t generation = 1;
static bool catalog_available = true, command_ok = true;
static char sent[96];
static int send_count;
bool custom_theme_color_override(uint32_t a,uint32_t b,uint32_t c,uint32_t *d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_metric_override(int32_t a,int32_t b,int32_t c,int32_t *d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_accent_override(uint8_t a,uint32_t *b){(void)a;(void)b;return false;}
bool custom_theme_surface_opacity(uint8_t *a){(void)a;return false;}
void moonraker_state_snapshot(moonraker_state_t *s){*s=fixture;}
void moonraker_filament_state_snapshot(moonraker_filament_state_t *s){*s=sensors;}
uint32_t moonraker_config_generation(void){return generation;}
void macro_controller_status(macro_controller_status_t *s){memset(s,0,sizeof(*s));s->discovered=catalog_available;s->count=catalog_available?4:0;}
bool macro_controller_get(size_t i,char *s,size_t n){if(i>=4)return false;snprintf(s,n,"%s",commands[i]);return true;}
static bool send(const char *s){send_count++;snprintf(sent,sizeof(sent),"%s",s);return command_ok;}
static uint16_t raster[1024*600];
static void flush(lv_display_t *d,const lv_area_t *a,uint8_t *p){uint16_t *v=(uint16_t*)p;for(int y=a->y1;y<=a->y2;y++)for(int x=a->x1;x<=a->x2;x++)raster[y*1024+x]=*v++;lv_display_flush_ready(d);}
static void snapshot(int theme){const char *folder=getenv("RECOVERY_SCREENSHOTS");if(!folder)return;lv_refr_now(NULL);char path[512];snprintf(path,sizeof(path),"%s/theme%d.ppm",folder,theme);FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n1024 600\n255\n");for(int i=0;i<1024*600;i++){uint16_t v=raster[i];unsigned char c[3]={(v>>11)*255/31,((v>>5)&63)*255/63,(v&31)*255/31};fwrite(c,1,3,f);}fclose(f);}

static void fixture_reset(void){
 memset(&fixture,0,sizeof(fixture));fixture.moonraker_ok=fixture.live_data_ok=true;strcpy(fixture.printer_state,"paused");strcpy(fixture.homed_axes,"xyz");fixture.nozzle_temp=fixture.nozzle_target=205;
 memset(&sensors,0,sizeof(sensors));sensors.discovered=true;sensors.sensor_count=1;sensors.sensors[0].enabled=sensors.sensors[0].status_known=sensors.sensors[0].filament_detected=true;
 catalog_available=command_ok=true;generation=1;send_count=0;
}
static void click(lv_obj_t *b){lv_obj_send_event(b,LV_EVENT_CLICKED,NULL);}
static void enabled(unsigned i,bool expected){refresh(NULL);assert(!lv_obj_has_state(buttons[i],LV_STATE_DISABLED)==expected);}
int main(void){
 lv_init();lv_display_t *d=lv_display_create(1024,600);static uint8_t buffer[1024*20*4];lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);lv_display_set_buffers(d,buffer,NULL,sizeof(buffer),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(d,flush);
 for(int theme=0;theme<4;theme++)for(int density=0;density<3;density++)for(int large=0;large<2;large++){
  ui_theme_set_active(theme);ui_theme_set_density(density);ui_accessibility_t a={.large_text=large};ui_theme_set_accessibility(a);
  fixture_reset();ui_filament_recovery_show(send);lv_obj_update_layout(popup);if(density==1&&large==1)snapshot(theme);
  for(int i=0;i<5;i++){assert(lv_obj_get_width(buttons[i])>=180);assert(lv_obj_get_height(buttons[i])>=48);enabled(i,true);}
  click(buttons[0]);assert(strcmp(sent,"HMI_FILAMENT_HEAT TEMP=205")==0);enabled(4,false);lv_tick_inc(3000);enabled(4,true);
  selected_temperature=240;click(buttons[0]);assert(strcmp(sent,"HMI_FILAMENT_HEAT TEMP=240")==0);lv_tick_inc(3000);
  fixture.nozzle_temp=100;enabled(1,false);enabled(2,false);enabled(3,false);enabled(0,true);enabled(4,true);
  fixture.nozzle_temp=205;sensors.sensors[0].filament_detected=false;enabled(4,false);enabled(1,true);int before=send_count;click(buttons[4]);assert(send_count==before);
  sensors.sensors[0].filament_detected=true;click(buttons[1]);assert(strcmp(sent,"HMI_FILAMENT_UNLOAD")==0);lv_tick_inc(3000);enabled(2,false);lv_tick_inc(6000);enabled(2,true);
  catalog_available=false;enabled(0,false);enabled(1,false);enabled(4,true);catalog_available=true;
  fixture.live_data_ok=false;for(int i=0;i<5;i++)enabled(i,false);fixture.live_data_ok=true;
  generation++;for(int i=0;i<5;i++)enabled(i,false);ui_filament_recovery_close();assert(!popup&&!refresh_timer);
 }
 fixture_reset();ui_filament_recovery_show(send);command_ok=false;click(buttons[0]);assert(!action_pending);assert(strstr(lv_label_get_text(message_label),"could not"));ui_filament_recovery_close();
 fixture_reset();printer_ui_controller_init(send,NULL,NULL,NULL);lv_obj_t *b=lv_button_create(lv_screen_active());lv_obj_add_event_cb(b,printer_ui_controller_command_event_cb,LV_EVENT_CLICKED,"RESUME");click(b);assert(popup&&send_count==0);ui_filament_recovery_close();lv_obj_delete(b);
 fixture_reset();ui_filament_recovery_show(send);strcpy(fixture.homed_axes,"xy");enabled(1,false);enabled(4,false);ui_filament_recovery_close();
 fixture_reset();ui_filament_recovery_show(send);strcpy(fixture.printer_state,"printing");int before=send_count;click(buttons[4]);assert(send_count==before);ui_filament_recovery_close();
 puts("PASS: recovery events, temperature, macro discovery, sensors, cooldown, printer/offline fencing, all themes/densities/accessibility");return 0;
}
