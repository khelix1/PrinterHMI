#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "custom_theme.h"
#include "ui_printer_live_status.c"
bool custom_theme_color_override(uint32_t a,uint32_t b,uint32_t c,uint32_t *d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_metric_override(int32_t a,int32_t b,int32_t c,int32_t *d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_accent_override(uint8_t a,uint32_t *b){(void)a;(void)b;return false;}
bool custom_theme_surface_opacity(uint8_t *a){(void)a;return false;}
static moonraker_state_t live;
static uint32_t generation=1;
static lv_obj_t *page;
static unsigned sends,notes,mode,notice_mode;
static bool accepted=true;
static char sent[32],notice[160];
uint32_t moonraker_config_generation(void){return generation;}
void moonraker_state_snapshot(moonraker_state_t*out){*out=live;}
moonraker_filament_status_t moonraker_filament_state_status(const moonraker_filament_state_t*s,size_t*p,size_t*e){(void)s;if(p)*p=0;if(e)*e=0;return MOONRAKER_FILAMENT_READY;}
static void close_page(void){if(page){lv_obj_delete(page);page=NULL;}}
void ui_toast_show(ui_status_kind_t kind,const char*title,const char*detail){(void)kind;assert(title&&detail);notes++;snprintf(notice,sizeof(notice),"%s",detail);if(notice_mode){notice_mode=0;close_page();}}
static void open_page(void);
static void release(lv_obj_t*slider,int value){assert(slider);lv_slider_set_value(slider,value,LV_ANIM_OFF);lv_obj_send_event(slider,LV_EVENT_RELEASED,NULL);}
static bool send(const char *command){
 sends++;snprintf(sent,sizeof(sent),"%s",command);unsigned during=mode;mode=0;
 if(during==1)release(s_speed_factor_slider,120);
 if(during==2)close_page();
 if(during==3){close_page();open_page();if(s_speed_factor_slider)release(s_speed_factor_slider,120);}
 if(during==4){generation++;close_page();open_page();if(s_speed_factor_slider)release(s_speed_factor_slider,120);}
 if(during==5)lv_obj_delete(s_speed_factor_value);
 if(during==6)generation++;
 if(during==7)studio_tuning_close(NULL);
 return accepted;
}
static void open_page(void){
 page=lv_obj_create(lv_screen_active());lv_obj_set_size(page,1024,600);lv_obj_t *speed,*flow,*layer,*filament;
 ui_printer_live_status_create(page,&speed,&flow,&layer,&filament,NULL,live.speed_factor,live.flow_factor,send);
 if(ui_theme_is_studio())studio_tuning_open(NULL);
}
static void refresh(void){
 refresh_factor_slider(s_speed_factor_slider,s_speed_factor_value,live.speed_factor,&s_pending_speed_factor);
 refresh_factor_slider(s_flow_factor_slider,s_flow_factor_value,live.flow_factor,&s_pending_flow_factor);
}
static void reset(void){close_page();generation++;accepted=true;mode=notice_mode=0;s_pending_speed_factor=s_pending_flow_factor=-1;memset(s_factor_accepted,0,sizeof(s_factor_accepted));live=(moonraker_state_t){.moonraker_ok=true,.live_data_ok=true,.speed_factor=100,.flow_factor=100};strcpy(live.printer_state,"printing");open_page();}
static void flush(lv_display_t*d,const lv_area_t*a,uint8_t*p){(void)a;(void)p;lv_display_flush_ready(d);}
static void checks(void){
 reset();unsigned before=sends,nt=notes;accepted=false;release(s_speed_factor_slider,120);assert(sends==before+1&&notes==nt+1&&strstr(notice,"Selected value kept"));refresh();assert(lv_slider_get_value(s_speed_factor_slider)==120&&s_pending_speed_factor==120&&!s_factor_accepted[0]);
 close_page();open_page();refresh();assert(lv_slider_get_value(s_speed_factor_slider)==120);accepted=true;release(s_speed_factor_slider,120);assert(sends==before+2&&s_factor_accepted[0]&&!strcmp(sent,"M220 S120"));release(s_speed_factor_slider,120);assert(sends==before+2);
 close_page();open_page();release(s_speed_factor_slider,120);assert(sends==before+2);live.speed_factor=120;refresh();assert(s_pending_speed_factor==-1);
 release(s_flow_factor_slider,110);assert(!strcmp(sent,"M221 S110"));release(s_flow_factor_slider,115);assert(!strcmp(sent,"M221 S115"));nt=notes;lv_tick_inc(5000);refresh();assert(s_pending_flow_factor==-1&&lv_slider_get_value(s_flow_factor_slider)==100&&notes==nt+1&&strstr(notice,"unconfirmed"));refresh();assert(notes==nt+1);
 reset();release(s_speed_factor_slider,125);generation++;refresh();assert(s_pending_speed_factor==-1&&lv_obj_has_state(s_speed_factor_slider,LV_STATE_DISABLED));before=sends;release(s_speed_factor_slider,130);assert(sends==before);close_page();live.speed_factor=90;open_page();assert(lv_slider_get_value(s_speed_factor_slider)==90);
 for(unsigned during=1;during<=6;during++){reset();mode=during;before=sends;nt=notes;accepted=false;release(s_speed_factor_slider,120);assert(sends==before+1&&!s_tuning_dispatching);
  if(during==1)assert(notes==nt+1);else assert(notes==nt);
  if(during==2)assert(!s_tuning_owner&&!s_speed_factor_slider&&!s_send_gcode_cb&&!s_studio_tuning_popup);
  if(during==5)assert(!s_speed_factor_value);
  if(during==4||during==6)assert(s_pending_speed_factor==-1);
 }
 reset();mode=2;before=sends;release(s_speed_factor_slider,120);assert(!page&&s_pending_speed_factor==120&&s_factor_accepted[0]);open_page();release(s_speed_factor_slider,120);assert(sends==before+1);
 if(ui_theme_is_studio()){reset();mode=7;before=sends;release(s_speed_factor_slider,120);assert(!s_studio_tuning_popup&&!s_speed_factor_slider);studio_tuning_open(NULL);release(s_speed_factor_slider,120);assert(sends==before+1);}
 reset();before=sends;live.live_data_ok=false;release(s_speed_factor_slider,120);assert(sends==before);refresh();assert(lv_obj_has_state(s_speed_factor_slider,LV_STATE_DISABLED));live.live_data_ok=true;live.moonraker_ok=false;release(s_speed_factor_slider,120);assert(sends==before);live.moonraker_ok=true;refresh();assert(!lv_obj_has_state(s_speed_factor_slider,LV_STATE_DISABLED));
 reset();before=sends;lv_slider_set_value(s_speed_factor_slider,120,LV_ANIM_OFF);lv_obj_send_event(s_speed_factor_slider,LV_EVENT_VALUE_CHANGED,NULL);assert(sends==before&&!strcmp(lv_label_get_text(s_speed_factor_value),"120%"));
 lv_obj_t*unowned=lv_slider_create(page);lv_obj_add_event_cb(unowned,tuning_slider_event_cb,LV_EVENT_RELEASED,(void*)PRINTER_TUNING_SPEED);release(unowned,90);assert(sends==before);
 lv_obj_add_event_cb(s_flow_factor_slider,tuning_slider_event_cb,LV_EVENT_RELEASED,(void*)99);release(s_flow_factor_slider,110);assert(sends==before+1);
 reset();accepted=false;notice_mode=1;release(s_speed_factor_slider,120);assert(!page&&!s_tuning_owner);open_page();accepted=true;release(s_speed_factor_slider,120);live.speed_factor=100;lv_tick_inc(5000);notice_mode=1;refresh();assert(!page&&!s_tuning_owner);
 reset();release(s_speed_factor_slider,120);close_page();lv_tick_inc(5000);notice_mode=1;open_page();assert(!page&&!s_tuning_owner);
 reset();lv_obj_t*old=page;page=NULL;open_page();lv_obj_t*current=s_speed_factor_slider,*owner=s_tuning_owner;
 ui_printer_live_status_refresh(old,NULL,NULL,NULL,NULL,0,0,80,90,0,0,true,NULL);
 assert(lv_slider_get_value(current)==100);
 lv_obj_delete(old);assert(s_speed_factor_slider==current&&s_tuning_owner==owner);before=sends;release(current,125);assert(sends==before+1);
 close_page();
}
int main(void){lv_init();lv_display_t*d=lv_display_create(1024,600);static uint8_t buffer[1024*40*2];lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);lv_display_set_buffers(d,buffer,NULL,sizeof(buffer),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(d,flush);
 for(unsigned theme=0;theme<5;theme++)for(unsigned density=0;density<3;density++)for(unsigned large=0;large<2;large++){ui_theme_set_active(theme);ui_theme_set_density(density);ui_theme_set_accessibility((ui_accessibility_t){.large_text=large});checks();}
 puts("PASS: Speed/Flow rejected input and retry, captured-printer requests, accepted duplicate guard across recreation, telemetry acknowledgement, bounded timeout, callback teardown/reopen/switch, retired fields, safe notification teardown, overlapping page replacement and all five themes");return 0;
}
