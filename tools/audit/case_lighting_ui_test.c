#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "lvgl.h"
#include "custom_theme.h"
#include "ui_case_lighting.c"
#include "cJSON.h"
static uint32_t generation=1;
static int64_t clock_us;
static bool connected=true,send_ok=true;
static unsigned sends;
static int sent_percent;
static moonraker_state_t live;
uint32_t moonraker_config_generation(void){return generation;}
int64_t esp_timer_get_time(void){return clock_us;}
void moonraker_state_snapshot(moonraker_state_t *out){*out=live;}
bool moonraker_live_websocket_subscribed(void){return connected;}
bool moonraker_live_websocket_set_case_led(uint32_t owner,int percent){assert(owner==generation);sends++;sent_percent=percent;char cmd[96];unsigned id=case_lighting_begin(owner,percent,clock_us,cmd,sizeof(cmd));if(!send_ok)case_lighting_failed(id,owner);return send_ok;}
bool custom_theme_color_override(uint32_t a,uint32_t b,uint32_t c,uint32_t*d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_metric_override(int32_t a,int32_t b,int32_t c,int32_t*d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_accent_override(uint8_t a,uint32_t*b){(void)a;(void)b;return false;}
bool custom_theme_surface_opacity(uint8_t*a){(void)a;return false;}
static void prepare(bool pwm)
{
 case_lighting_reset(generation);cJSON*r=cJSON_Parse("[\"output_pin case_led\"]");case_lighting_objects(r,generation);cJSON_Delete(r);
 r=cJSON_Parse(pwm?"{\"output_pin case_led\":{\"pwm\":true}}":"{\"output_pin case_led\":{}}");case_lighting_config(r,generation);cJSON_Delete(r);
 const char *json="{\"result\":{\"status\":{\"output_pin case_led\":{\"value\":0}}}}";case_lighting_status_json(json,strlen(json),generation);
}
static void inside(lv_obj_t *child,lv_obj_t *parent){lv_area_t a,b;lv_obj_get_coords(child,&a);lv_obj_get_coords(parent,&b);assert(a.x1>=b.x1&&a.y1>=b.y1&&a.x2<=b.x2&&a.y2<=b.y2);}
static void run(void)
{
 memset(&live,0,sizeof(live));live.moonraker_ok=live.live_data_ok=true;strcpy(live.printer_state,"printing");connected=send_ok=true;prepare(true);
 ui_case_lighting_show();lv_obj_t *popup=s_popup;assert(popup&&s_switch&&s_slider&&s_value&&s_status);ui_case_lighting_show();assert(s_popup==popup);
 lv_obj_update_layout(popup);inside(popup,lv_layer_top());inside(lv_obj_get_child(popup,2),popup);
 assert(!lv_obj_has_state(s_switch,LV_STATE_DISABLED)&&!lv_obj_has_state(s_slider,LV_STATE_DISABLED));
 unsigned before=sends;lv_slider_set_value(s_slider,35,LV_ANIM_OFF);lv_obj_send_event(s_slider,LV_EVENT_VALUE_CHANGED,NULL);assert(sends==before&&strstr(lv_label_get_text(s_value),"35%"));lv_obj_send_event(s_slider,LV_EVENT_RELEASED,NULL);assert(sends==before+1&&sent_percent==35&&lv_obj_has_state(s_switch,LV_STATE_DISABLED));
 assert(lv_slider_get_value(s_slider)==35&&strstr(lv_label_get_text(s_value),"35%"));
 for(unsigned poll=0;poll<4;poll++){refresh(NULL);assert(lv_slider_get_value(s_slider)==35);}
 lv_obj_update_layout(popup);assert(lv_obj_get_height(s_slider)<=14);inside(s_slider,lv_obj_get_parent(s_slider));
 lv_obj_send_event(s_slider,LV_EVENT_RELEASED,NULL);assert(sends==before+1);
 const char *old="{\"method\":\"notify_status_update\",\"params\":[{\"output_pin case_led\":{\"value\":0}},1]}";
 case_lighting_status_json(old,strlen(old),generation);refresh(NULL);assert(lv_slider_get_value(s_slider)==35);
 const char *echo="{\"method\":\"notify_status_update\",\"params\":[{\"output_pin case_led\":{\"value\":0.35}},1]}";
 case_lighting_status_json(echo,strlen(echo),generation);refresh(NULL);assert(lv_slider_get_value(s_slider)==35&&!lv_obj_has_state(s_slider,LV_STATE_DISABLED));
 lv_slider_set_value(s_slider,65,LV_ANIM_OFF);lv_obj_send_event(s_slider,LV_EVENT_RELEASED,NULL);assert(lv_slider_get_value(s_slider)==65);

 clock_us+=5000001;refresh(NULL);assert(lv_slider_get_value(s_slider)==35);assert(strstr(lv_label_get_text(s_status),"confirmation")&&!lv_obj_has_state(s_switch,LV_STATE_DISABLED));
 send_ok=false;lv_obj_remove_state(s_switch,LV_STATE_CHECKED);lv_obj_send_event(s_switch,LV_EVENT_VALUE_CHANGED,NULL);assert(strstr(lv_label_get_text(s_status),"Could not send"));send_ok=true;
 generation++;before=sends;refresh(NULL);assert(strstr(lv_label_get_text(s_status),"Printer changed"));lv_obj_send_event(s_switch,LV_EVENT_VALUE_CHANGED,NULL);assert(sends==before);
 lv_obj_delete(s_popup);assert(!s_popup&&!s_timer&&!s_switch&&!s_status);ui_case_lighting_close();
 prepare(false);ui_case_lighting_show();assert(lv_obj_has_state(s_slider,LV_STATE_DISABLED)&&!lv_obj_has_state(s_switch,LV_STATE_DISABLED));
 connected=false;refresh(NULL);assert(lv_obj_has_state(s_switch,LV_STATE_DISABLED));connected=true;
 lv_obj_delete(s_value);lv_obj_delete(s_slider);assert(!s_value&&!s_slider);refresh(NULL);ui_case_lighting_close();assert(!s_timer);
 case_lighting_reset(generation);ui_case_lighting_show();assert(lv_obj_has_state(s_switch,LV_STATE_DISABLED));ui_case_lighting_close();
}
int main(void)
{
 lv_init();lv_display_t *display=lv_display_create(1024,600);
 for(int theme=0;theme<5;theme++)for(int density=0;density<3;density++)for(int large=0;large<2;large++){
  ui_theme_set_active((ui_theme_id_t)theme);ui_theme_set_density((ui_density_id_t)density);ui_theme_set_accessibility((ui_accessibility_t){.large_text=large});run();
 }
 lv_display_delete(display);lv_deinit();puts("PASS: Lighting layout, printing access, slider release-only dispatch, pending/timeout/retry, profile change, digital/offline/unknown guards and direct/partial teardown across themes/densities/text sizes");
}
