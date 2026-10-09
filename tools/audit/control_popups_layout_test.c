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
static char sent[128];static unsigned sends;
static void send(const char *s){snprintf(sent,sizeof(sent),"%s",s);sends++;}
static void click(lv_obj_t *o){lv_obj_send_event(o,LV_EVENT_CLICKED,NULL);}
static uint16_t raster[1024*600];static int viewport_width=1024;
static void flush(lv_display_t *d,const lv_area_t *a,uint8_t *p){uint16_t *v=(uint16_t*)p;for(int y=a->y1;y<=a->y2;y++)for(int x=a->x1;x<=a->x2;x++)raster[y*viewport_width+x]=*v++;lv_display_flush_ready(d);}
static void snapshot(int theme,const char *name){const char *folder=getenv("CONTROL_POPUP_SCREENSHOTS");if(!folder)return;lv_refr_now(NULL);char path[512];snprintf(path,sizeof(path),"%s/%s-theme%d.ppm",folder,name,theme);FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n1024 600\n255\n");for(int i=0;i<1024*600;i++){uint16_t v=raster[i];unsigned char c[3]={(v>>11)*255/31,((v>>5)&63)*255/63,(v&31)*255/31};fwrite(c,1,3,f);}fclose(f);}
static void inside(lv_obj_t *child,lv_obj_t *parent){lv_area_t a,b;lv_obj_get_coords(child,&a);lv_obj_get_coords(parent,&b);if(!(a.x1>=b.x1&&a.x2<=b.x2&&a.y1>=b.y1&&a.y2<=b.y2)){fprintf(stderr,"Outside %d,%d..%d,%d vs %d,%d..%d,%d\n",a.x1,a.y1,a.x2,a.y2,b.x1,b.y1,b.x2,b.y2);abort();}}
static lv_obj_t *body(void){return lv_obj_get_child(s_control_popup,1);}
static lv_obj_t *presets(void){return lv_obj_get_child(body(),2);}
static void layout(void){lv_obj_update_layout(s_control_popup);lv_obj_t *footer=lv_obj_get_child(s_control_popup,-1);inside(footer,s_control_popup);for(uint32_t i=0;i<lv_obj_get_child_count(footer);i++)inside(lv_obj_get_child(footer,i),footer);
 lv_obj_t *grid=presets();lv_obj_t *cards=lv_obj_get_child(body(),0);for(uint32_t c=0;c<lv_obj_get_child_count(cards);c++){lv_obj_t *card=lv_obj_get_child(cards,c);assert(lv_obj_get_height(card)<160);for(uint32_t n=0;n<lv_obj_get_child_count(card);n++)inside(lv_obj_get_child(card,n),card);}for(uint32_t i=0;i<lv_obj_get_child_count(grid);i++){lv_obj_t *b=lv_obj_get_child(grid,i);assert(lv_obj_get_width(b)>100&&lv_obj_get_height(b)>=48);inside(b,grid);inside(lv_obj_get_child(b,0),b);lv_area_t a;lv_obj_get_coords(b,&a);for(uint32_t j=0;j<i;j++){lv_area_t c;lv_obj_get_coords(lv_obj_get_child(grid,j),&c);assert(a.x2<c.x1||c.x2<a.x1||a.y2<c.y1||c.y2<a.y1);}}
 lv_area_t before,after;lv_obj_get_coords(footer,&before);lv_obj_scroll_to_y(body(),200,LV_ANIM_OFF);lv_obj_update_layout(s_control_popup);lv_obj_get_coords(footer,&after);assert(!memcmp(&before,&after,sizeof(before)));lv_obj_scroll_to_y(body(),0,LV_ANIM_OFF);
}
static void open_custom(void){lv_obj_t *cards=lv_obj_get_child(body(),0);click(lv_obj_get_child(cards,1));assert(s_custom_temp_popup);lv_obj_update_layout(s_custom_temp_popup);lv_obj_t *footer=lv_obj_get_child(s_custom_temp_popup,-1);inside(footer,s_custom_temp_popup);for(uint32_t i=0;i<lv_obj_get_child_count(footer);i++)inside(lv_obj_get_child(footer,i),footer);assert(lv_obj_get_width(s_custom_temp_textarea)>200);}
int main(void){lv_init();lv_display_t *d=lv_display_create(1024,600);static uint8_t buffer[1024*40*2];lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);lv_display_set_buffers(d,buffer,NULL,sizeof(buffer),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(d,flush);
 for(int width_case=0;width_case<3;width_case++)for(int theme=0;theme<4;theme++)for(int density=0;density<3;density++)for(int large=0;large<2;large++){
  viewport_width=(int[]){1024,640,480}[width_case];lv_display_set_resolution(d,viewport_width,width_case==2?400:600);ui_theme_set_active(theme);ui_theme_set_density(density);ui_theme_set_accessibility((ui_accessibility_t){.large_text=large});
  ui_printer_popups_show_nozzle(send,204,215);layout();if(!width_case&&density==1&&large)snapshot(theme,"nozzle");unsigned before=sends;click(lv_obj_get_child(presets(),1));assert(sends==before+1&&!strcmp(sent,"M104 S200")&&!s_control_popup);
  ui_printer_popups_show_bed(send,59,60);layout();open_custom();if(!width_case&&density==1&&large)snapshot(theme,"keyboard");lv_textarea_set_text(s_custom_temp_textarea,"999");set_custom_temp_cb(NULL);assert(s_custom_temp_popup&&sends==before+1);lv_textarea_set_text(s_custom_temp_textarea,"0");set_custom_temp_cb(NULL);assert(!s_custom_temp_popup&&!s_control_popup&&!strcmp(sent,"M140 S0"));
  ui_printer_popups_show_part_fan(send,50);layout();click(lv_obj_get_child(presets(),3));assert(!s_control_popup&&!strcmp(sent,"M106 S191"));
  ui_printer_popups_show_nozzle(send,-200,-1);layout();open_custom();lv_obj_t *footer=lv_obj_get_child(s_custom_temp_popup,-1);click(lv_obj_get_child(footer,0));assert(!s_custom_temp_popup&&s_control_popup);click(lv_obj_get_child(lv_obj_get_child(s_control_popup,-1),0));assert(!s_control_popup);
 }
 // Dynamic hotend prefixes still address the selected heater, not M104's default.
 lv_display_set_resolution(d,1024,600);viewport_width=1024;s_send_gcode_cb=send;const char *cmds[]={"SET_HEATER_TEMPERATURE HEATER=extruder1 TARGET=215","SET_HEATER_TEMPERATURE HEATER=extruder1 TARGET=0"};const char *labels[]={"215 C","OFF"};double values[]={215,0};show_control_popup("T1 TEMPERATURE",210,215,"C",true,cmds,labels,values,2,"CUSTOM T1 TEMP","SET_HEATER_TEMPERATURE HEATER=extruder1 TARGET=",300);layout();open_custom();lv_textarea_set_text(s_custom_temp_textarea,"230");lv_obj_t *keyboard=lv_obj_get_child(lv_obj_get_child(s_custom_temp_popup,1),2);lv_obj_send_event(keyboard,LV_EVENT_READY,NULL);assert(!strcmp(sent,"SET_HEATER_TEMPERATURE HEATER=extruder1 TARGET=230")&&!s_control_popup&&!s_custom_temp_popup);
 puts("PASS: native popup wrapping/scroll, pinned actions, four themes/densities/text sizes, 1024/640/480 widths, preset/custom/zero/range/keyboard commands and teardown");return 0;}
