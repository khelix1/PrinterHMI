/* Linux-only LVGL runtime. UI implementation is compiled from main/, never copied.
 * The only machine backend is the in-memory fixture below. No transport APIs. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include "lvgl.h"
#include "custom_theme.h"
#include "moonraker.h"
#include "ui_theme.h"
#include "ui_printer_popups.h"
#include "ui_printer_toolhead.h"
#include "routes.h"
void host_tuning_show(bool (*send)(const char *));
void host_tuning_close(void);
static moonraker_state_t machine;
static uint32_t raster[1024*600];
static uint8_t draw_buffer[1024*50*4];
static lv_indev_t *pointer,*keyboard;
static lv_indev_data_t pointer_data;
static uint32_t key_code;
static bool key_pressed;
static char last_command[512];
static bool palette_enabled;
static uint32_t palette[64];
static int metrics[3]={16,24,16};
static bool metric_enabled[3];
/* These unrelated pages are not instantiated by this host yet. */
void ui_filament_recovery_close(void){}
void ui_dashboard_status_popup_close(void){}
uint32_t moonraker_config_generation(void){return 1;}
void moonraker_state_snapshot(moonraker_state_t *out){*out=machine;}
bool custom_theme_color_override(uint32_t a,uint32_t b,uint32_t c,uint32_t *out){if(!palette_enabled)return false;for(unsigned i=0;i<ROUTE_COUNT;i++)if(routes[i].a==a&&routes[i].b==b&&routes[i].c==c){*out=palette[i];return true;}return false;}
bool custom_theme_metric_override(int32_t a,int32_t b,int32_t c,int32_t *out){const int route[3][3]={{12,20,22},{10,18,20},{10,20,24}};for(int i=0;i<3;i++)if(metric_enabled[i]&&route[i][0]==a&&route[i][1]==b&&route[i][2]==c){*out=metrics[i];return true;}return false;}
bool custom_theme_accent_override(uint8_t tone,uint32_t *out){if(!palette_enabled)return false;const char *name=tone>=2?"accent_bright":"accent";for(unsigned i=0;i<ROUTE_COUNT;i++)if(!strcmp(routes[i].name,name)){*out=palette[i];return true;}return false;}
bool custom_theme_surface_opacity(uint8_t *out){(void)out;return false;}
static void send_command(const char *command){
 snprintf(last_command,sizeof(last_command),"%s",command);double v;
 if(sscanf(command,"M104 S%lf",&v)==1)machine.nozzle_target=v;
 else if(sscanf(command,"M140 S%lf",&v)==1)machine.bed_target=v;
 else if(sscanf(command,"M106 S%lf",&v)==1)machine.part_fan_speed=v*100/255;
 else if(sscanf(command,"M220 S%lf",&v)==1)machine.speed_factor=v;
 else if(sscanf(command,"M221 S%lf",&v)==1)machine.flow_factor=v;
 else if(!strcmp(command,"G28")){machine.toolhead_x=machine.toolhead_y=machine.toolhead_z=0;}
 else if(sscanf(command,"SET_GCODE_OFFSET Z_ADJUST=%lf",&v)==1)machine.z_offset+=v;
 else if(!strcmp(command,"SET_GCODE_OFFSET Z=0 MOVE=1"))machine.z_offset=0;
 else if(strstr(command,"G91\nG1 ")){char axis;if(sscanf(command,"G91\nG1 %c%lf",&axis,&v)==2){double *position=axis=='X'?&machine.toolhead_x:axis=='Y'?&machine.toolhead_y:axis=='Z'?&machine.toolhead_z:NULL;if(position)*position+=v;}}
 else if(!strcmp(command,"CANCEL_PRINT"))snprintf(machine.printer_state,sizeof(machine.printer_state),"cancelled");
}
static bool checked_send(const char *command){send_command(command);return true;}
static void flush(lv_display_t *display,const lv_area_t *area,uint8_t *pixels){size_t w=(size_t)(area->x2-area->x1+1);for(int y=area->y1;y<=area->y2;y++)memcpy(raster+y*1024+area->x1,pixels+(size_t)(y-area->y1)*w*4,w*4);lv_display_flush_ready(display);}
static void read_pointer(lv_indev_t *device,lv_indev_data_t *data){(void)device;*data=pointer_data;}
static lv_obj_t *find_class(lv_obj_t *root,const lv_obj_class_t *cls){for(unsigned i=lv_obj_get_child_count(root);i>0;i--){lv_obj_t *child=lv_obj_get_child(root,i-1);lv_obj_t *found=find_class(child,cls);if(found)return found;}return lv_obj_check_type(root,cls)?root:NULL;}
static void read_key(lv_indev_t *device,lv_indev_data_t *data){(void)device;data->key=key_code;data->state=key_pressed?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED;}
static void tick(unsigned amount){static uint64_t previous;struct timespec now;clock_gettime(CLOCK_MONOTONIC,&now);uint64_t ms=(uint64_t)now.tv_sec*1000+now.tv_nsec/1000000;unsigned delta=previous?(unsigned)(ms-previous):amount;previous=ms;lv_tick_inc(delta);lv_timer_handler();lv_refr_now(NULL);}
static void frame(void){
 tick(30);
 unsigned modal=lv_obj_get_child_count(lv_layer_top());
 /* Header precedes exactly 1024*600 little-endian BGRA pixels. */
 printf("FRAME %u %.0f %.0f %.0f %zu %.4f %.4f %.4f %.4f %.0f %.0f\n",modal,machine.nozzle_target,machine.bed_target,machine.part_fan_speed,strlen(last_command),machine.toolhead_x,machine.toolhead_y,machine.toolhead_z,machine.z_offset,machine.speed_factor,machine.flow_factor);
 fwrite(last_command,1,strlen(last_command),stdout);fwrite(raster,1,sizeof(raster),stdout);fflush(stdout);last_command[0]=0;
}
static void close_all(void){host_tuning_close();ui_printer_toolhead_close();ui_printer_popups_close_all();lv_obj_clean(lv_layer_top());pointer_data.state=LV_INDEV_STATE_RELEASED;}
int main(void){
 setvbuf(stdout,NULL,_IOFBF,65536);lv_init();lv_display_t *display=lv_display_create(1024,600);lv_display_set_color_format(display,LV_COLOR_FORMAT_ARGB8888);lv_display_set_buffers(display,draw_buffer,NULL,sizeof(draw_buffer),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(display,flush);
 ui_theme_set_active(UI_THEME_STUDIO_DARK);ui_theme_set_density(UI_DENSITY_COMFORTABLE);lv_obj_set_style_bg_opa(lv_screen_active(),LV_OPA_TRANSP,0);lv_obj_set_style_bg_opa(lv_layer_bottom(),LV_OPA_TRANSP,0);lv_obj_set_style_bg_opa(lv_layer_top(),LV_OPA_TRANSP,0);
 lv_group_t *group=lv_group_create();pointer=lv_indev_create();lv_indev_set_type(pointer,LV_INDEV_TYPE_POINTER);lv_indev_set_read_cb(pointer,read_pointer);keyboard=lv_indev_create();lv_indev_set_type(keyboard,LV_INDEV_TYPE_KEYPAD);lv_indev_set_read_cb(keyboard,read_key);lv_indev_set_group(keyboard,group);
 machine.moonraker_ok=machine.live_data_ok=true;strcpy(machine.printer_state,"printing");strcpy(machine.homed_axes,"xyz");machine.toolhead_position_valid=true;machine.nozzle_temp=210;machine.nozzle_target=210;machine.bed_temp=60;machine.bed_target=60;machine.part_fan_speed=65;machine.speed_factor=machine.flow_factor=100;
 ui_printer_popups_set_control_sender(checked_send);ui_printer_toolhead_set_sender(checked_send);
 char line[2048];while(fgets(line,sizeof(line),stdin)){
  char name[64];int x=0,y,down;unsigned key;double a,b,c;
  if(!strncmp(line,"quit",4))break;
  if(sscanf(line,"color %63s %x",name,&key)==2){for(unsigned i=0;i<ROUTE_COUNT;i++)if(!strcmp(name,routes[i].name)){palette[i]=key;break;}continue;}
  if(sscanf(line,"metric %63s %d",name,&x)==2){const char *names[]={"radius_card","radius_button","radius_panel"};for(int i=0;i<3;i++)if(!strcmp(names[i],name)&&x>=0&&x<=64){metrics[i]=x;metric_enabled[i]=true;}continue;}
  double offset;
  if(sscanf(line,"position %lf %lf %lf %lf",&a,&b,&c,&offset)==4){machine.toolhead_x=a;machine.toolhead_y=b;machine.toolhead_z=c;machine.z_offset=offset;continue;}
  if(sscanf(line,"factors %lf %lf",&a,&b)==2){machine.speed_factor=a;machine.flow_factor=b;continue;}
  if(sscanf(line,"targets %lf %lf %lf",&a,&b,&c)==3){machine.nozzle_target=a;machine.bed_target=b;machine.part_fan_speed=c;continue;}
  if(sscanf(line,"open %63s %d",name,&x)>=1&&!strncmp(line,"open ",5)){
   close_all();palette_enabled=x==1;
   if(!strcmp(name,"nozzle"))ui_printer_popups_show_nozzle(send_command,machine.nozzle_temp,machine.nozzle_target);
   else if(!strcmp(name,"bed"))ui_printer_popups_show_bed(send_command,machine.bed_temp,machine.bed_target);
   else if(!strcmp(name,"fan"))ui_printer_popups_show_part_fan(send_command,machine.part_fan_speed);
   else if(!strcmp(name,"stop"))ui_printer_popups_show_cancel(send_command);
   else if(!strcmp(name,"tune"))host_tuning_show(checked_send);
   else if(!strcmp(name,"toolhead")){static lv_obj_t *a,*b,*c;static double step=1;ui_printer_toolhead_show(&a,&b,&c,&step,send_command);}
   frame();continue;
  }
  if(sscanf(line,"pointer %d %d %d",&x,&y,&down)==3){pointer_data.point.x=x<0?0:x>1023?1023:x;pointer_data.point.y=y<0?0:y>599?599:y;pointer_data.state=down?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED;lv_indev_read(pointer);frame();continue;}
  if(sscanf(line,"key %u",&key)==1){lv_obj_t *input=find_class(lv_layer_top(),&lv_textarea_class);lv_obj_t *kb=find_class(lv_layer_top(),&lv_keyboard_class);if(key==LV_KEY_ENTER&&kb)lv_obj_send_event(kb,LV_EVENT_READY,NULL);else if(key==LV_KEY_ESC&&kb)lv_obj_send_event(kb,LV_EVENT_CANCEL,NULL);else if(key==LV_KEY_ESC)close_all();else if(input)lv_obj_send_event(input,LV_EVENT_KEY,&key);frame();continue;}
  if(!strncmp(line,"close",5))close_all();
  frame();
 }
 close_all();lv_display_delete(display);lv_deinit();return 0;
}
