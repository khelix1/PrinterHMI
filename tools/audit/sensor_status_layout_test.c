#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "custom_theme.h"
#include "ui_printer_popups.c"
#include "dashboard_status.inc"
bool custom_theme_color_override(uint32_t a,uint32_t b,uint32_t c,uint32_t *d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_metric_override(int32_t a,int32_t b,int32_t c,int32_t *d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_accent_override(uint8_t a,uint32_t *b){(void)a;(void)b;return false;}
bool custom_theme_surface_opacity(uint8_t *a){(void)a;return false;}
static moonraker_filament_state_t fixture;
static uint32_t generation=1;
uint32_t moonraker_config_generation(void){return generation;}
void moonraker_filament_state_snapshot(moonraker_filament_state_t *o){*o=fixture;}
static unsigned sends,invalidations;static char sent[128];
static void send(const char *cmd){sends++;snprintf(sent,sizeof(sent),"%s",cmd);}
static void click(lv_obj_t *o){lv_obj_send_event(o,LV_EVENT_CLICKED,NULL);}
static void invalidated(lv_event_t *e){(void)e;invalidations++;}
static uint16_t raster[1024*600];static int width=1024;
static void flush(lv_display_t *d,const lv_area_t *a,uint8_t *p){uint16_t *v=(uint16_t*)p;for(int y=a->y1;y<=a->y2;y++)for(int x=a->x1;x<=a->x2;x++)raster[y*width+x]=*v++;lv_display_flush_ready(d);}
static void snapshot(int theme,const char *name){const char *folder=getenv("SENSOR_STATUS_SCREENSHOTS");if(!folder)return;lv_refr_now(NULL);char path[512];snprintf(path,sizeof(path),"%s/%s-theme%d.ppm",folder,name,theme);FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n1024 600\n255\n");for(int i=0;i<1024*600;i++){uint16_t v=raster[i];unsigned char c[3]={(v>>11)*255/31,((v>>5)&63)*255/63,(v&31)*255/31};fwrite(c,1,3,f);}fclose(f);}
static void inside(lv_obj_t *child,lv_obj_t *parent){lv_area_t a,b;lv_obj_get_coords(child,&a);lv_obj_get_coords(parent,&b);if(!(a.x1>=b.x1&&a.x2<=b.x2&&a.y1>=b.y1&&a.y2<=b.y2)){fprintf(stderr,"outside %d,%d..%d,%d vs %d,%d..%d,%d\n",a.x1,a.y1,a.x2,a.y2,b.x1,b.y1,b.x2,b.y2);abort();}}
static void layout(lv_obj_t *popup){lv_obj_update_layout(popup);lv_obj_t *footer=lv_obj_get_child(popup,-1),*body=lv_obj_get_child(popup,1);inside(footer,popup);inside(body,popup);for(uint32_t i=0;i<lv_obj_get_child_count(footer);i++){lv_obj_t *b=lv_obj_get_child(footer,i);assert(lv_obj_get_height(b)>=48);inside(b,footer);inside(lv_obj_get_child(b,0),b);}lv_area_t before,after;lv_obj_get_coords(footer,&before);lv_obj_scroll_to_y(body,300,LV_ANIM_OFF);lv_obj_update_layout(popup);lv_obj_get_coords(footer,&after);assert(!memcmp(&before,&after,sizeof(before)));lv_obj_scroll_to_y(body,0,LV_ANIM_OFF);}
static void reset(void){memset(&fixture,0,sizeof(fixture));fixture.discovered=true;fixture.sensor_count=4;fixture.total_count=6;fixture.truncated=true;for(int i=0;i<4;i++){snprintf(fixture.sensors[i].object_name,sizeof(fixture.sensors[i].object_name),"filament_%s_sensor long_sensor_name_for_filament_path_%d",i==2?"motion":"switch",i);fixture.sensors[i].enabled=i!=3;fixture.sensors[i].status_known=i!=2;fixture.sensors[i].filament_detected=i==0;}generation++;}
int main(void){lv_init();lv_display_t *d=lv_display_create(1024,600);static uint8_t pixels[1024*50*2];lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);lv_display_set_buffers(d,pixels,NULL,sizeof(pixels),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(d,flush);lv_display_add_event_cb(d,invalidated,LV_EVENT_INVALIDATE_AREA,NULL);
 for(int narrow=0;narrow<3;narrow++)for(int theme=0;theme<4;theme++)for(int density=0;density<3;density++)for(int large=0;large<2;large++){
  width=(int[]){1024,640,480}[narrow];lv_display_set_resolution(d,width,narrow==2?400:600);ui_theme_set_active(theme);ui_theme_set_density(density);ui_theme_set_accessibility((ui_accessibility_t){.large_text=large});reset();ui_printer_popups_show_filament_sensors(send,&fixture);assert(s_filament_list_popup);layout(s_filament_list_popup);
  for(int i=0;i<4;i++){lv_obj_t *row=lv_obj_get_parent(s_filament_toggle_buttons[i]);inside(s_filament_toggle_buttons[i],row);inside(s_filament_toggle_labels[i],s_filament_toggle_buttons[i]);lv_obj_t *info=lv_obj_get_child(row,0);inside(info,row);for(uint32_t n=0;n<lv_obj_get_child_count(info);n++)inside(lv_obj_get_child(info,n),info);}
  if(!narrow&&density==1&&large){snapshot(theme,"sensors");}lv_refr_now(d);invalidations=0;for(int i=0;i<100;i++)refresh_filament_list(NULL);assert(!invalidations);
  unsigned before=sends;click(s_filament_toggle_buttons[0]);assert(sends==before+1&&strstr(sent,"SENSOR=long_sensor_name_for_filament_path_0 ENABLE=0"));assert(s_filament_toggle_pending[0]);click(s_filament_toggle_buttons[0]);assert(sends==before+1);fixture.sensors[0].enabled=false;refresh_filament_list(NULL);assert(!s_filament_toggle_pending[0]&&!strcmp(lv_label_get_text(s_filament_toggle_labels[0]),"ENABLE"));
  moonraker_filament_sensor_t swap=fixture.sensors[0];fixture.sensors[0]=fixture.sensors[1];fixture.sensors[1]=swap;refresh_filament_list(NULL);click(s_filament_toggle_buttons[0]);assert(strstr(sent,"SENSOR=long_sensor_name_for_filament_path_0 ENABLE=1"));for(int i=0;i<10;i++)refresh_filament_list(NULL);assert(!s_filament_toggle_pending[0]);
  fixture.sensor_count=1;refresh_filament_list(NULL);assert(strstr(lv_label_get_text(s_filament_status_labels[0]),"UNAVAILABLE"));before=sends;click(s_filament_toggle_buttons[0]);assert(sends==before);generation++;refresh_filament_list(NULL);assert(strstr(lv_label_get_text(s_filament_status_labels[1]),"PRINTER CHANGED"));click(s_filament_toggle_buttons[1]);assert(sends==before);click(lv_obj_get_child(lv_obj_get_child(s_filament_list_popup,-1),0));assert(!s_filament_list_popup&&!s_filament_refresh_timer&&!s_filament_row_count);
  char file[256];memset(file,'x',255);file[0]='j';file[1]='/';file[255]=0;ui_printer_popups_show_printer_status("paused",file,"42%","02:15:00","01:30:00",204,205,60,60,true);assert(s_dashboard_status_popup);layout(s_dashboard_status_popup);lv_obj_t *body=lv_obj_get_child(s_dashboard_status_popup,1),*label=lv_obj_get_child(body,0);assert(strstr(lv_label_get_text(label),file));assert(strstr(lv_label_get_text(label),"Moonraker: CONNECTED"));if(!narrow&&density==1&&large)snapshot(theme,"status");click(lv_obj_get_child(lv_obj_get_child(s_dashboard_status_popup,-1),0));assert(!s_dashboard_status_popup);
 }
 reset();ui_printer_popups_show_filament_sensors(send,&fixture);lv_obj_delete(s_filament_list_popup);assert(!s_filament_refresh_timer&&!s_filament_row_count);ui_dashboard_status_popup_show(NULL,NULL);lv_obj_delete(s_dashboard_status_popup);assert(!s_dashboard_status_popup);ui_dashboard_status_popup_show("STATUS","Reopened");ui_dashboard_status_popup_close();assert(!s_dashboard_status_popup);
 reset();strcpy(fixture.sensors[0].object_name,"filament_switch_sensor unsafe;name");ui_printer_popups_show_filament_sensors(send,&fixture);unsigned before=sends;click(s_filament_toggle_buttons[0]);assert(sends==before&&s_dashboard_status_popup);ui_dashboard_status_popup_close();close_filament_list_cb(NULL);
 puts("PASS: sensor/status layouts, all themes/densities/text sizes, long names/files, scrolling/footer bounds, quiet refresh, named toggles, acknowledgement/timeout, reorder/removal/profile guards and deletion");return 0;}
