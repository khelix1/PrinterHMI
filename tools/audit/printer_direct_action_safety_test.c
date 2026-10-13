#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "custom_theme.h"
#include "printer_ui_controller.c"
#include "ui_printer_actions.h"
bool custom_theme_color_override(uint32_t a,uint32_t b,uint32_t c,uint32_t *d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_metric_override(int32_t a,int32_t b,int32_t c,int32_t *d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_accent_override(uint8_t a,uint32_t *b){(void)a;(void)b;return false;}
bool custom_theme_surface_opacity(uint8_t *a){(void)a;return false;}
static moonraker_state_t live;
static uint32_t generation=1;
static ui_printer_actions_t actions;
static lv_obj_t *panel;
static unsigned sends,notes,recoveries,cancels,objects,motions,mode;
static bool accept=true;
static char sent[32],notice[128];
uint32_t moonraker_config_generation(void){return generation;}
void moonraker_state_snapshot(moonraker_state_t*out){*out=live;}
void ui_toast_show(ui_status_kind_t kind,const char*title,const char*detail){(void)kind;assert(title&&detail);notes++;snprintf(notice,sizeof(notice),"%s",detail);}
void ui_filament_recovery_show(bool(*send)(const char*)){assert(send);recoveries++;}
static void cancel(void){cancels++;}
static void object(void){objects++;}
static void motion(void){motions++;}
static void click(lv_obj_t*b){assert(b);lv_obj_send_event(b,LV_EVENT_CLICKED,NULL);}
static void open(void);
static void close(void){if(panel){lv_obj_delete(panel);panel=NULL;}memset(&actions,0,sizeof(actions));}
static bool send(const char*command){
 sends++;snprintf(sent,sizeof(sent),"%s",command);unsigned during=mode;mode=0;
 if(during==1)click(actions.home);
 if(during==2)close();
 if(during==3){close();open();click(actions.home);}
 if(during==4){generation++;close();open();click(actions.home);}
 if(during==5)generation++;
 return accept;
}
static void open(void){
 printer_ui_controller_init(send,cancel,object,motion);
 panel=lv_obj_create(lv_screen_active());lv_obj_set_size(panel,1024,100);
 ui_printer_actions_create(panel,&actions,printer_ui_controller_command_event_cb,printer_ui_controller_motion_event_cb);
 printer_ui_controller_bind_actions(&actions);
}
static void refresh(void){printer_ui_controller_update_action_buttons(actions.home,actions.pause,actions.resume,actions.object,actions.cancel,true,live.printer_state);}
static void reset(void){close();generation++;accept=true;mode=0;memset(s_direct_requests,0,sizeof(s_direct_requests));live=(moonraker_state_t){.moonraker_ok=true,.live_data_ok=true};strcpy(live.printer_state,"standby");open();refresh();}
static void flush(lv_display_t*d,const lv_area_t*a,uint8_t*p){(void)a;(void)p;lv_display_flush_ready(d);}
static void checks(void){
 reset();unsigned before=sends,nt=notes;
 accept=false;click(actions.home);assert(sends==before+1&&notes==nt+1&&strstr(notice,"Send failed"));refresh();assert(!lv_obj_has_state(actions.home,LV_STATE_DISABLED));
 accept=true;click(actions.home);assert(sends==before+2&&!strcmp(sent,"HOME_ALL"));click(actions.home);assert(sends==before+2);refresh();assert(lv_obj_has_state(actions.home,LV_STATE_DISABLED));
 close();open();refresh();click(actions.home);assert(sends==before+2);lv_tick_inc(5000);refresh();click(actions.home);assert(sends==before+3);
 reset();strcpy(live.printer_state,"printing");refresh();before=sends;click(actions.home);assert(sends==before);accept=false;click(actions.pause);assert(sends==before+1&&strstr(notice,"Send failed"));accept=true;click(actions.pause);click(actions.pause);assert(sends==before+2&&!strcmp(sent,"PAUSE"));refresh();assert(lv_obj_has_state(actions.pause,LV_STATE_DISABLED));
 strcpy(live.printer_state,"paused");refresh();assert(!s_direct_requests[ACTION_PAUSE].pending);unsigned r=recoveries;click(actions.resume);assert(recoveries==r+1);strcpy(live.printer_state,"printing");refresh();click(actions.pause);assert(sends==before+3);
 unsigned c=cancels,o=objects,m=motions;click(actions.cancel);click(actions.object);click(actions.motion);assert(cancels==c+1&&objects==o+1&&motions==m+1);
 for(unsigned during=1;during<=5;during++){reset();before=sends;nt=notes;mode=during;click(actions.home);assert(sends==before+1&&!s_action_dispatching);
  if(during==1)assert(notes==nt+1);else assert(notes==nt);
  if(during==2){for(unsigned i=0;i<ACTION_COUNT;i++)assert(!s_action_buttons[i]);}
  else {refresh();click(actions.home);assert(sends==before+(during==4?2:1));}
 }
 reset();before=sends;generation++;click(actions.home);click(actions.motion);assert(sends==before);refresh();assert(lv_obj_has_state(actions.home,LV_STATE_DISABLED));
 reset();before=sends;live.live_data_ok=false;click(actions.home);assert(sends==before);live.live_data_ok=true;live.moonraker_ok=false;click(actions.home);assert(sends==before);
 reset();before=sends;lv_obj_send_event(actions.home,LV_EVENT_PRESSED,NULL);assert(sends==before);
 lv_obj_t*b=lv_button_create(panel);lv_obj_add_event_cb(b,printer_ui_controller_command_event_cb,LV_EVENT_CLICKED,"HOME_ALL");click(b);assert(sends==before);
 lv_obj_add_event_cb(actions.resume,printer_ui_controller_command_event_cb,LV_EVENT_CLICKED,"HOME_ALL");click(actions.resume);assert(sends==before);
 reset();before=sends;s_send_gcode_cb=NULL;click(actions.home);assert(sends==before);
 reset();unsigned epoch=s_action_epoch;lv_obj_delete(actions.home);assert(!s_action_buttons[ACTION_HOME]&&s_action_epoch>epoch);actions.home=NULL;close();
}
int main(void){lv_init();lv_display_t*d=lv_display_create(1024,600);static uint8_t pixels[1024*40*2];lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);lv_display_set_buffers(d,pixels,NULL,sizeof(pixels),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(d,flush);
 for(unsigned theme=0;theme<5;theme++)for(unsigned density=0;density<3;density++)for(unsigned large=0;large<2;large++){ui_theme_set_active(theme);ui_theme_set_density(density);ui_theme_set_accessibility((ui_accessibility_t){.large_text=large});checks();}
 puts("PASS: Home/Pause rejected retry, accepted cooldown across page recreation, pause acknowledgement, repeated/reentrant events, owned button and printer guards, callback teardown/reopen/switch, notification fencing, existing Resume/Object/Stop/Toolhead paths and all five themes");return 0;
}
