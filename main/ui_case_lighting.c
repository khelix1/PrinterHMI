#include "ui_case_lighting.h"
#include "case_lighting.h"
#include "moonraker_live_websocket.h"
#include "printer_connection_policy.h"
#include "esp_timer.h"
#include "ui_calibration_dialog.h"
#include "ui_value_update.h"
#include <math.h>
#include <stdio.h>

static lv_obj_t *s_popup,*s_status,*s_switch,*s_slider,*s_value;
static lv_timer_t *s_timer;
static uint32_t s_owner,s_epoch;
static int s_last_on=100;
static bool s_dispatching;
static void retire(void)
{
    ++s_epoch;
    if(s_timer)lv_timer_delete(s_timer);
    s_timer=NULL;s_popup=s_status=s_switch=s_slider=s_value=NULL;
}
static void deleted(lv_event_t *event)
{
    lv_obj_t *target=lv_event_get_target_obj(event);
    if(target==s_popup){retire();return;}
    lv_obj_t **fields[]={&s_status,&s_switch,&s_slider,&s_value};
    for(unsigned i=0;i<4;i++)if(target==*fields[i])*fields[i]=NULL;
}
void ui_case_lighting_close(void)
{
    lv_obj_t *popup=s_popup;retire();if(popup)lv_obj_delete(popup);
}
static void close_cb(lv_event_t *event){(void)event;ui_case_lighting_close();}
static const char *ready(case_lighting_snapshot_t *light)
{
    if(s_owner!=moonraker_config_generation())return "Printer changed. Close and reopen Lighting.";
    moonraker_state_t state;moonraker_state_snapshot(&state);
    const char *reason=printer_connection_reason(&state);if(*reason)return reason;
    if(!moonraker_live_websocket_subscribed())return "Waiting for the live printer connection...";
    case_lighting_snapshot(light,esp_timer_get_time());
    if(light->generation!=s_owner||!light->discovered)return "Discovering printer lighting...";
    if(!light->available)return "No case_led output found on this printer.";
    if(!light->configured)return "Waiting for valid case_led configuration...";
    if(!light->known)return "Waiting for the live lighting value...";
    if(light->pending)return "Applying lighting - waiting for live confirmation...";
    return "";
}
static void enabled(lv_obj_t *object,bool enable)
{
    if(!object)return;
    if(enable)lv_obj_remove_state(object,LV_STATE_DISABLED);else lv_obj_add_state(object,LV_STATE_DISABLED);
}
static void refresh(lv_timer_t *timer)
{
    if(!s_popup||s_dispatching||(timer&&timer!=s_timer))return;
    case_lighting_snapshot_t light={0};const char *reason=ready(&light);
    enabled(s_switch,!*reason);enabled(s_slider,!*reason&&light.pwm);
    if(light.known){
        int value=(int)lround(light.value*100);
        bool on=light.value>0;
        if(on)s_last_on=value>0?value:1;
        /* Keep the requested position until live status confirms or fails. */
        if(light.pending){value=light.target;on=value>0;}
        if(s_switch){if(on)lv_obj_add_state(s_switch,LV_STATE_CHECKED);else lv_obj_remove_state(s_switch,LV_STATE_CHECKED);}
        if(s_slider&&!lv_obj_has_state(s_slider,LV_STATE_PRESSED))lv_slider_set_value(s_slider,value,LV_ANIM_OFF);
        if(s_value&&(!s_slider||!lv_obj_has_state(s_slider,LV_STATE_PRESSED))){char text[48];if(on&&!value)snprintf(text,sizeof(text),"Brightness: <1%%");else snprintf(text,sizeof(text),"Brightness: %d%%",value);ui_value_set_text(s_value,text);}
    }
    if(s_status)ui_value_set_text(s_status,*reason?reason:light.error[0]?light.error:light.pwm?"Live case lighting":"On/off output - brightness requires PWM.");
}
static void apply(int percent)
{
    if(s_dispatching||!s_popup)return;
    case_lighting_snapshot_t light={0};if(*ready(&light)){refresh(NULL);return;}
    if(fabs(light.value-percent/100.0)<=0.000001){refresh(NULL);return;}
    uint32_t epoch=s_epoch;
    s_dispatching=true;
    bool sent=moonraker_live_websocket_set_case_led(s_owner,percent);
    s_dispatching=false;
    if(epoch!=s_epoch||!s_popup)return;
    refresh(NULL);
    if(!sent&&s_status)ui_value_set_text(s_status,"Could not send lighting command. Retry when connected.");
}
static void switch_cb(lv_event_t *event)
{
    if(lv_event_get_target_obj(event)!=s_switch||lv_obj_has_state(s_switch,LV_STATE_DISABLED))return;
    apply(lv_obj_has_state(s_switch,LV_STATE_CHECKED)?s_last_on:0);
}
static void slider_cb(lv_event_t *event)
{
    if(lv_event_get_target_obj(event)!=s_slider||lv_obj_has_state(s_slider,LV_STATE_DISABLED))return;
    int percent=lv_slider_get_value(s_slider);
    if(lv_event_get_code(event)==LV_EVENT_VALUE_CHANGED){
        if(s_value){char text[48];snprintf(text,sizeof(text),"Brightness: %d%%",percent);ui_value_set_text(s_value,text);}
    }else if(lv_event_get_code(event)==LV_EVENT_RELEASED)apply(percent);
}
static void watch(lv_obj_t *object){lv_obj_add_event_cb(object,deleted,LV_EVENT_DELETE,NULL);}
void ui_case_lighting_show(void)
{
    if(s_popup){lv_obj_move_foreground(s_popup);return;}
    s_owner=moonraker_config_generation();s_last_on=100;
    s_popup=ui_cal_dialog_create(lv_layer_top(),540,410,UI_POPUP_STANDARD);if(!s_popup)return;
    watch(s_popup);ui_cal_dialog_title_divided(s_popup,"Lighting");
    lv_obj_t *body=ui_cal_dialog_body(s_popup);
    ui_cal_dialog_text(body,"Case light");
    s_switch=lv_switch_create(body);lv_obj_set_size(s_switch,76,40);watch(s_switch);
    lv_obj_add_event_cb(s_switch,switch_cb,LV_EVENT_VALUE_CHANGED,NULL);
    s_value=ui_cal_dialog_text(body,"Brightness: --");watch(s_value);
    /* Give the thumb room at both ends; the track itself stays slim. */
    lv_obj_t *slider_row=lv_obj_create(body);
    ui_apply_surface_role(slider_row,UI_SURFACE_TRANSPARENT);
    lv_obj_set_size(slider_row,LV_PCT(100),48);
    lv_obj_set_style_pad_all(slider_row,0,0);
    lv_obj_set_style_border_width(slider_row,0,0);
    lv_obj_remove_flag(slider_row,LV_OBJ_FLAG_SCROLLABLE);
    s_slider=lv_slider_create(slider_row);
    lv_obj_set_width(s_slider,LV_PCT(88));
    lv_obj_set_height(s_slider,ui_theme_density_metric(10,12,14));
    lv_obj_align(s_slider,LV_ALIGN_CENTER,0,0);
    lv_slider_set_range(s_slider,0,100);watch(s_slider);
    ui_apply_slider_style(s_slider);
    lv_obj_set_ext_click_area(s_slider,10);
    lv_obj_set_style_bg_color(s_switch,UI_ACCENT_BRIGHT,LV_PART_INDICATOR|LV_STATE_CHECKED);
    lv_obj_add_event_cb(s_slider,slider_cb,LV_EVENT_VALUE_CHANGED,NULL);lv_obj_add_event_cb(s_slider,slider_cb,LV_EVENT_RELEASED,NULL);
    s_status=ui_cal_dialog_text(body,"Discovering printer lighting...");watch(s_status);
    ui_cal_dialog_action(s_popup,UI_POPUP_ACTION_CLOSE,"Close",close_cb,NULL,NULL);
    s_timer=lv_timer_create(refresh,250,NULL);refresh(NULL);
}
