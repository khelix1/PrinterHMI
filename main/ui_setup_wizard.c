#include "ui_setup_wizard.h"
/* STEP259_FULL_PRINTER_OPTIONS */
/* STEP258_CAMERA_POLISH */
/* STEP257_PRINTER_DISCOVERY */
/* STEP256_DEDICATED_NAV */
/* STEP254_SETUP_CARD_SAFE */
/* STEP252_SETUP_REPAIRED */
/* STEP251_SETUP_CARD */
/* STEP249_STABLE_SETUP_SCAN */
/* STEP248_SETUP_SCAN */
#include "ui_text.h"

#include "camera_catalog_controller.h"
#include "camera_discovery_controller.h"
#include "camera_test_controller.h"
#include "moonraker_config_controller.h"
#include "moonraker_discovery.h"
#include "moonraker_endpoint_test.h"
#include "onboarding_controller.h"
#include "ui_printer_profiles.h"
#include "ui_popup.h"
#include "ui_theme.h"
#include "ui_studio_layout.h"
#include "ui_text_fit.h"

#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

#include <stdio.h>
#include <string.h>

#define SETUP_MAX_WIFI 8

typedef struct {
    char ssid[33];
    int8_t rssi;
} setup_wifi_ap_t;

static lv_obj_t *s_center;
static lv_obj_t *s_complete_popup;
static lv_obj_t *s_step;
static lv_obj_t *s_status;
static lv_obj_t *s_name;
static lv_obj_t *s_password;
static lv_obj_t *s_keyboard;
static lv_timer_t *s_poll;
static ui_setup_wizard_wifi_connect_cb_t s_wifi_connect;
static setup_wifi_ap_t s_wifi[SETUP_MAX_WIFI];
static size_t s_wifi_count;
static volatile bool s_wifi_scan_done;
static lv_obj_t *s_wifi_list;
static lv_obj_t *s_camera_list;
static char s_selected_ssid[33];
static int s_selected_camera = -1;
static bool s_wifi_done, s_printer_done, s_camera_done;
static uint32_t s_wifi_verify_tick, s_camera_owner;


enum {
    SETUP_STEP_WELCOME = 0,
    SETUP_STEP_WIFI,
    SETUP_STEP_PRINTER,
    SETUP_STEP_CAMERA,
    SETUP_STEP_COMPLETE,
};

static int s_setup_step = SETUP_STEP_WELCOME;
static lv_obj_t *s_setup_content;
static lv_obj_t *s_stage;
static lv_obj_t *s_actions;
static lv_obj_t *s_nav[5];
static lv_obj_t *s_summary;
static volatile bool s_wifi_scan_busy;
static uint32_t s_setup_epoch;
static bool s_setup_dispatching, s_camera_verified;
static lv_obj_t *s_wifi_connect_action, *s_camera_use_action, *s_finish_action;
static lv_obj_t *s_wifi_rows[SETUP_MAX_WIFI], *s_camera_rows[CAMERA_CATALOG_MAX_CAMERAS];
static void delete_poll(void);

static bool setup_current(uint32_t epoch)
{
    return s_center && s_setup_content && s_setup_epoch == epoch;
}
static bool setup_action_current(lv_event_t *event, lv_obj_t *button, bool keyboard)
{
    if (!s_center || !s_setup_content || s_setup_dispatching || !button ||
        lv_obj_has_state(button, LV_STATE_DISABLED)) return false;
    if (!event) return true;
    lv_obj_t *target=lv_event_get_target_obj(event);
    if (target != lv_event_get_current_target_obj(event)) return false;
    return (target == button && lv_event_get_code(event) == LV_EVENT_CLICKED) ||
        (keyboard && target == s_keyboard && lv_event_get_code(event) == LV_EVENT_READY);
}
static void setup_field_deleted(lv_event_t *event)
{
    lv_obj_t *object=lv_event_get_target_obj(event);
    if(object==s_setup_content || object==s_stage || object==s_wifi_list || object==s_camera_list)delete_poll();
    lv_obj_t **fields[]={&s_step,&s_status,&s_name,&s_password,&s_keyboard,&s_wifi_list,
        &s_camera_list,&s_setup_content,&s_stage,&s_actions,&s_summary,
        &s_wifi_connect_action,&s_camera_use_action,&s_finish_action};
    bool retired=false;
    for (unsigned i=0;i<sizeof(fields)/sizeof(fields[0]);++i)
        if (*fields[i]==object) { *fields[i]=NULL; retired=true; }
    for (unsigned i=0;i<5;++i)if(s_nav[i]==object){s_nav[i]=NULL;retired=true;}
    for (unsigned i=0;i<SETUP_MAX_WIFI;++i)if(s_wifi_rows[i]==object){s_wifi_rows[i]=NULL;retired=true;}
    for (unsigned i=0;i<CAMERA_CATALOG_MAX_CAMERAS;++i)if(s_camera_rows[i]==object){s_camera_rows[i]=NULL;retired=true;}
    if(retired)++s_setup_epoch;
}
static void setup_watch(lv_obj_t *object)
{
    if(object)lv_obj_add_event_cb(object,setup_field_deleted,LV_EVENT_DELETE,NULL);
}

static void setup_render_step(int step);
static void setup_select_welcome_cb(lv_event_t *event);
static void setup_select_wifi_cb(lv_event_t *event);
static void setup_select_printer_cb(lv_event_t *event);
static void setup_select_camera_cb(lv_event_t *event);
static void setup_select_complete_cb(lv_event_t *event);
static void wifi_scan_open(void);
static void wifi_save_cb(lv_event_t *event);
static void wifi_scan_open_cb(lv_event_t *event);
static void printer_full_options_cb(lv_event_t *event);
static void printer_editor_changed_cb(void);
static void printer_editor_discover_cb(void);

static void center_wifi_cb(lv_event_t *event);
static void center_printer_cb(lv_event_t *event);
static void center_camera_cb(lv_event_t *event);
static void finish_cb(lv_event_t *event);
static void later_cb(lv_event_t *event);
static void set_status(const char *text)
{
    if (s_status) {
        lv_label_set_text(s_status, text ? text : "");
    }
}
static void setup_refresh_completion_state(void);


static void setup_next_cb(lv_event_t *event)
{
    (void)event;
    if(!s_center || !s_setup_content || s_setup_dispatching)return;
    if (s_setup_step < SETUP_STEP_COMPLETE) {
        setup_render_step(s_setup_step+1);
    }
}

static void setup_skip_cb(lv_event_t *event)
{
    (void)event;
    if(!s_center || !s_setup_content || s_setup_dispatching)return;
    if (s_setup_step < SETUP_STEP_COMPLETE) {
        setup_render_step(s_setup_step+1);
    }
}

static void setup_select_welcome_cb(lv_event_t *event)
{ (void)event; if(!s_setup_dispatching)setup_render_step(SETUP_STEP_WELCOME); }
static void setup_select_wifi_cb(lv_event_t *event)
{ (void)event; if(!s_setup_dispatching)setup_render_step(SETUP_STEP_WIFI); }
static void setup_select_printer_cb(lv_event_t *event)
{ (void)event; if(!s_setup_dispatching)setup_render_step(SETUP_STEP_PRINTER); }
static void setup_select_camera_cb(lv_event_t *event)
{ (void)event; if(!s_setup_dispatching)setup_render_step(SETUP_STEP_CAMERA); }
static void setup_select_complete_cb(lv_event_t *event)
{ (void)event; if(!s_setup_dispatching)setup_render_step(SETUP_STEP_COMPLETE); }

static bool setup_clear_content(void)
{
    uint32_t epoch=++s_setup_epoch;
    lv_obj_t *keyboard=s_keyboard,*content=s_setup_content;
    s_keyboard=NULL;
    /* STEP251_SETUP_CARD: never leave a keyboard above scan rows. */
    s_status = NULL;
    s_stage = s_actions = NULL;
    s_password = NULL;
    s_name = NULL;
    s_wifi_list = NULL;
    s_camera_list = NULL;
    s_wifi_connect_action=s_camera_use_action=s_finish_action=NULL;
    s_camera_verified=false;
    memset(s_wifi_rows,0,sizeof(s_wifi_rows));
    memset(s_camera_rows,0,sizeof(s_camera_rows));
    if(keyboard)lv_obj_delete(keyboard);
    if(!setup_current(epoch) || s_setup_content!=content)return false;
    lv_obj_clean(content);
    return setup_current(epoch);
}

static lv_obj_t *setup_plane(lv_obj_t *parent, lv_flex_flow_t flow)
{
    lv_obj_t *obj=lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_width(obj,LV_PCT(100));
    lv_obj_set_height(obj,LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(obj,flow);
    lv_obj_set_style_pad_row(obj,UI_GAP_ROW,0);
    lv_obj_set_style_pad_column(obj,UI_GAP_ROW,0);
    lv_obj_clear_flag(obj,LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

static lv_obj_t *setup_text(lv_obj_t *parent,const char *text,const lv_font_t *font,lv_color_t color)
{
    lv_obj_t *label=lv_label_create(parent);
    ui_apply_custom_label_style(label,font,color);
    lv_label_set_text(label,text);
    lv_obj_set_width(label,LV_PCT(100));
    setup_watch(label);
    lv_label_set_long_mode(label,LV_LABEL_LONG_WRAP);
    return label;
}

static void setup_fit_button(lv_event_t *event)
{
    lv_obj_t *button=lv_event_get_target(event);
    lv_obj_t *label=lv_obj_get_child(button,0);
    if(!label || !lv_obj_check_type(label,&lv_label_class))return;
    lv_obj_set_width(label,LV_PCT(100));
    ui_text_fit_single_line(label,UI_FONT_BODY);
    lv_label_set_long_mode(label,LV_LABEL_LONG_DOT);
    lv_obj_set_height(label,lv_obj_get_style_text_font(label,0)->line_height);
    lv_obj_center(label);
}

static lv_obj_t *setup_choice(lv_obj_t *parent,const char *text,lv_event_cb_t cb,void *data)
{
    lv_obj_t *row=ui_popup_add_selectable_row(parent,text,0,0,1,48,cb,data);
    lv_obj_set_width(row,LV_PCT(100));
    lv_obj_add_event_cb(row,setup_fit_button,LV_EVENT_SIZE_CHANGED,NULL);
    lv_obj_send_event(row,LV_EVENT_SIZE_CHANGED,NULL);
    return row;
}

static lv_obj_t *setup_action(lv_obj_t *parent,const char *text,ui_popup_action_t kind,lv_event_cb_t cb)
{
    lv_obj_t *button=ui_popup_add_action_at(parent,kind,text,0,0,LV_PCT(100),48,cb,NULL,NULL);
    lv_obj_add_event_cb(button,setup_fit_button,LV_EVENT_SIZE_CHANGED,NULL);
    lv_obj_send_event(button,LV_EVENT_SIZE_CHANGED,NULL);
    return button;
}

static void setup_update_nav(int step)
{
    const char *names[]={"Welcome","Wi-Fi","Printer","Camera","Review"};
    bool ready[]={false,s_wifi_done,s_printer_done,s_camera_done,false};
    for(int i=0;i<5;i++) {
        if(!s_nav[i])continue;
        char text[48];snprintf(text,sizeof(text),"%s %s",ready[i]?LV_SYMBOL_OK:(i==step?LV_SYMBOL_RIGHT:""),names[i]);
        lv_obj_t *label=lv_obj_get_child(s_nav[i],0);
        if(!label || !lv_obj_check_type(label,&lv_label_class))continue;
        lv_label_set_text(label,text);
        ui_text_fit_single_line(label,UI_FONT_BODY);
        lv_label_set_long_mode(label,LV_LABEL_LONG_DOT);
        lv_obj_set_style_bg_color(s_nav[i],UI_CONTROL,0);
        lv_obj_set_style_bg_opa(s_nav[i],i==step?LV_OPA_COVER:LV_OPA_TRANSP,0);
        lv_obj_set_style_border_color(s_nav[i],i==step?UI_ACCENT_BRIGHT:UI_BORDER,0);
    }
    char summary[80];snprintf(summary,sizeof(summary),"%d of 2 essentials ready\nCamera is optional",(int)s_wifi_done+(int)s_printer_done);
    if(s_summary)lv_label_set_text(s_summary,summary);
}

static bool setup_begin_content(const char *title,const char *detail)
{
    if(!setup_clear_content())return false;
    setup_text(s_setup_content,title,ui_theme_is_studio()?&ui_studio_font_32:UI_FONT_TITLE,UI_TEXT);
    s_status=setup_text(s_setup_content,detail,UI_FONT_BODY,UI_TEXT_DIM);
    s_stage=setup_plane(s_setup_content,LV_FLEX_FLOW_COLUMN);
    setup_watch(s_stage);
    lv_obj_set_height(s_stage,0);lv_obj_set_flex_grow(s_stage,1);
    lv_obj_add_flag(s_stage,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(s_stage,LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_stage,LV_SCROLLBAR_MODE_AUTO);
    s_actions=setup_plane(s_setup_content,LV_FLEX_FLOW_ROW_WRAP);
    setup_watch(s_actions);
    return true;
}

static void setup_add_next_buttons(bool allow_skip)
{
    if(allow_skip) {
        lv_obj_t *skip=setup_action(s_actions,"Skip for now",UI_POPUP_ACTION_SECONDARY,setup_skip_cb);
        lv_obj_set_width(skip,LV_PCT(48));lv_obj_set_flex_grow(skip,1);
    }
    lv_obj_t *next=setup_action(s_actions,"Continue",UI_POPUP_ACTION_CONFIRM,setup_next_cb);
    lv_obj_set_width(next,allow_skip?LV_PCT(48):LV_PCT(100));lv_obj_set_flex_grow(next,1);
}

static void setup_review_row(const char *name,bool ready,bool optional)
{
    lv_obj_t *row=setup_plane(s_stage,LV_FLEX_FLOW_ROW);
    lv_obj_set_height(row,64);
    lv_obj_set_style_bg_color(row,UI_CONTROL,0);
    lv_obj_set_style_bg_opa(row,LV_OPA_30,0);
    lv_obj_set_style_radius(row,12,0);
    lv_obj_set_style_pad_all(row,12,0);
    lv_obj_set_flex_align(row,LV_FLEX_ALIGN_SPACE_BETWEEN,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
    lv_obj_t *label=setup_text(row,name,UI_FONT_BODY,UI_TEXT);
    lv_obj_set_width(label,LV_PCT(44));lv_label_set_long_mode(label,LV_LABEL_LONG_DOT);
    lv_obj_t *status=setup_text(row,ready?LV_SYMBOL_OK " Ready":(optional?"Optional":"Needs setup"),UI_FONT_CAPTION,ready?UI_OK_BRIGHT:UI_TEXT_DIM);
    lv_obj_set_width(status,LV_PCT(52));lv_label_set_long_mode(status,LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(status,LV_TEXT_ALIGN_RIGHT,0);
}

static void setup_render_step(int step)
{
    if(!s_setup_content || step<0 || step>SETUP_STEP_COMPLETE)return;
    delete_poll();
    s_setup_step=step;
    setup_refresh_completion_state();
    setup_update_nav(step);
    const char *titles[]={"Welcome to your print cell","Connect your network","Add your printer","Connect a camera","Ready to operate?"};
    const char *details[]={"A few steps connect your display to the print cell.","Scan nearby Wi-Fi networks, choose one, then verify the connection.","Discover your printer or enter its Moonraker endpoint. Security and camera options are included.","Discover and test this printer's cameras. You can add one later.","Review your connections before finishing setup."};
    if(!setup_begin_content(titles[step],details[step]))return;
    if(step==SETUP_STEP_WELCOME || step==SETUP_STEP_COMPLETE) {
        setup_review_row("Wi-Fi",s_wifi_done,false);
        setup_review_row("Printer",s_printer_done,false);
        setup_review_row("Camera",s_camera_done,true);
        setup_text(s_stage,"Your saved settings stay available when you reopen Setup Center.",UI_FONT_CAPTION,UI_TEXT_DIM);
        if(step==SETUP_STEP_WELCOME)setup_add_next_buttons(false);
        else {s_finish_action=setup_action(s_actions,"Finish setup",UI_POPUP_ACTION_CONFIRM,finish_cb);setup_watch(s_finish_action);}
    } else {
        bool ready=step==SETUP_STEP_WIFI?s_wifi_done:step==SETUP_STEP_PRINTER?s_printer_done:s_camera_done;
        setup_text(s_stage,ready?"Already configured. Review it or continue to the next step.":"Start here. You can return to any step using the navigator.",UI_FONT_BODY_LARGE,ready?UI_OK:UI_TEXT);
        setup_action(s_stage,step==SETUP_STEP_WIFI?(ready?"Review Wi-Fi":"Find a network"):step==SETUP_STEP_PRINTER?(ready?"Review printer":"Open printer setup"):(ready?"Review camera":"Discover cameras"),UI_POPUP_ACTION_SECONDARY,step==SETUP_STEP_WIFI?center_wifi_cb:step==SETUP_STEP_PRINTER?center_printer_cb:center_camera_cb);
        setup_add_next_buttons(true);
    }
}

static void center_deleted(lv_event_t *event)
{
    if(lv_event_get_target_obj(event)!=s_center)return;
    ++s_setup_epoch;delete_poll();
    s_center=s_setup_content=s_stage=s_actions=s_status=s_name=s_password=s_keyboard=NULL;
    s_wifi_list=s_camera_list=s_summary=NULL;
    memset(s_nav,0,sizeof(s_nav));
    memset(s_wifi_rows,0,sizeof(s_wifi_rows));memset(s_camera_rows,0,sizeof(s_camera_rows));
    s_wifi_connect_action=s_camera_use_action=s_finish_action=NULL;
    s_wifi_connect=NULL;s_camera_verified=false;
}

static void show_center(void)
{
    if(s_center){lv_obj_move_foreground(s_center);return;}
    setup_refresh_completion_state();
    int width=lv_display_get_horizontal_resolution(NULL)-32;
    int height=lv_display_get_vertical_resolution(NULL)-32;
    if(width>960)width=960;
    if(height>560)height=560;
    s_center=ui_popup_create(lv_screen_active(),width,height,UI_POPUP_STANDARD);
    if(!s_center)return;
    lv_obj_add_event_cb(s_center,center_deleted,LV_EVENT_DELETE,NULL);
    lv_obj_set_flex_flow(s_center,LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(s_center,20,0);
    lv_obj_set_style_pad_row(s_center,16,0);
    setup_text(s_center,"Setup Center",ui_theme_is_studio()?&ui_studio_font_48:UI_FONT_TITLE,UI_TEXT);
    lv_obj_t *body=setup_plane(s_center,LV_FLEX_FLOW_ROW);
    lv_obj_set_height(body,0);lv_obj_set_flex_grow(body,1);
    lv_obj_set_style_pad_column(body,20,0);
    lv_obj_t *nav=setup_plane(body,LV_FLEX_FLOW_COLUMN);
    lv_obj_set_width(nav,width<600?120:180);lv_obj_set_height(nav,LV_PCT(100));
    lv_obj_add_flag(nav,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_scroll_dir(nav,LV_DIR_VER);
    const char *names[]={"Welcome","Wi-Fi","Printer","Camera","Review"};
    lv_event_cb_t callbacks[]={setup_select_welcome_cb,setup_select_wifi_cb,setup_select_printer_cb,setup_select_camera_cb,setup_select_complete_cb};
    for(int i=0;i<5;i++){s_nav[i]=setup_action(nav,names[i],UI_POPUP_ACTION_SECONDARY,callbacks[i]);setup_watch(s_nav[i]);}
    s_summary=setup_text(nav,"",UI_FONT_CAPTION,UI_TEXT_DIM);
    s_setup_content=setup_plane(body,LV_FLEX_FLOW_COLUMN);
    setup_watch(s_setup_content);
    lv_obj_set_width(s_setup_content,0);lv_obj_set_flex_grow(s_setup_content,1);
    lv_obj_set_height(s_setup_content,LV_PCT(100));
    setup_render_step(SETUP_STEP_WELCOME);
    lv_obj_t *footer=setup_plane(s_center,LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(footer,LV_FLEX_ALIGN_END,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
    lv_obj_t *later=setup_action(footer,"Set up later",UI_POPUP_ACTION_CANCEL,later_cb);
    lv_obj_set_width(later,width<600?144:180);
    lv_obj_move_foreground(s_center);
}

static void delete_poll(void) { if (s_poll) lv_timer_delete(s_poll); s_poll = NULL; }

static void destroy_step(void)
{
    delete_poll();
    lv_obj_t *step=s_step;
    s_step = s_status = s_name = s_password = s_keyboard = NULL;
    if(step)lv_obj_delete(step);
}

void ui_setup_wizard_close(void)
{
    lv_obj_t *center=s_center;
    /* Retire all old ownership before any child DELETE callback can reopen. */
    if(center) {
        ++s_setup_epoch;
        s_center=s_setup_content=s_stage=s_actions=s_status=s_name=s_password=s_keyboard=NULL;
        s_wifi_list=s_camera_list=s_summary=NULL;
        s_wifi_connect_action=s_camera_use_action=s_finish_action=NULL;
        memset(s_nav,0,sizeof(s_nav));memset(s_wifi_rows,0,sizeof(s_wifi_rows));
        memset(s_camera_rows,0,sizeof(s_camera_rows));
        s_wifi_connect=NULL;s_camera_verified=false;
    }
    destroy_step();
    if(center)lv_obj_delete(center);
}

static void focus_cb(lv_event_t *event)
{
    if(!event || lv_event_get_target_obj(event)!=s_password || !s_keyboard)return;
    lv_keyboard_set_textarea(s_keyboard,s_password);ui_popup_focus_input(s_password);
}

static void wifi_scan_task(void *ignored)
{
    (void)ignored;
    s_wifi_count = 0;
    s_wifi_scan_done = false;

    /* Do not disconnect the station here. The Network page scans while the
     * Wi-Fi state machine is running; setup must use the same path. */
    vTaskDelay(pdMS_TO_TICKS(150));
    esp_err_t scan_error = esp_wifi_scan_start(NULL, true);
    if (scan_error != ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(200));
        scan_error = esp_wifi_scan_start(NULL, true);
    }

    if (scan_error == ESP_OK) {
        uint16_t count = 0;
        if (esp_wifi_scan_get_ap_num(&count) == ESP_OK && count) {
            wifi_ap_record_t records[SETUP_MAX_WIFI] = {0};
            if (count > SETUP_MAX_WIFI) count = SETUP_MAX_WIFI;
            if (esp_wifi_scan_get_ap_records(&count, records) == ESP_OK) {
                for (uint16_t i = 0; i < count; ++i) {
                    if (!records[i].ssid[0]) continue;
                    strlcpy(s_wifi[s_wifi_count].ssid,
                            (const char *)records[i].ssid,
                            sizeof(s_wifi[s_wifi_count].ssid));
                    s_wifi[s_wifi_count++].rssi = records[i].rssi;
                }
            }
        }
    }

    s_wifi_scan_done = true;
    s_wifi_scan_busy = false;
    vTaskDelete(NULL);
}

static void wifi_password_open(void)
{
    delete_poll();if(!setup_begin_content("Connect to Wi-Fi",s_selected_ssid))return;
    /* Keep the network name with the field; the setup header already names the step. */
    lv_obj_delete(lv_obj_get_child(s_setup_content,0));
    lv_obj_set_parent(s_status,s_stage);
    s_password=ui_popup_add_textarea(s_stage,1,48,LV_ALIGN_TOP_LEFT,0,0,true,true,63,"Wi-Fi password","",NULL);
    setup_watch(s_password);
    lv_obj_set_size(s_password,LV_PCT(100),56);
    ui_apply_text_body(s_password);
    s_keyboard=ui_popup_add_keyboard(s_stage,s_password,1,180,LV_ALIGN_TOP_LEFT,0,0,LV_KEYBOARD_MODE_TEXT_LOWER);
    setup_watch(s_keyboard);
    if(s_keyboard)lv_obj_set_width(s_keyboard,LV_PCT(100));
    if(s_password)lv_obj_add_event_cb(s_password,focus_cb,LV_EVENT_CLICKED,NULL);
    s_wifi_connect_action=setup_action(s_actions,"Connect",UI_POPUP_ACTION_CONFIRM,wifi_save_cb);
    setup_watch(s_wifi_connect_action);
    ui_popup_keyboard_actions(s_keyboard,wifi_save_cb,wifi_scan_open_cb);
    ui_popup_pin_keyboard(s_setup_content,s_keyboard);
}

static void wifi_selected_cb(lv_event_t *event)
{
    if(!event || !s_center || !s_wifi_list || s_setup_dispatching || s_poll)return;
    const uintptr_t index = (uintptr_t)lv_event_get_user_data(event);
    if (index >= s_wifi_count || !setup_action_current(event,s_wifi_rows[index],false)) return;
    strlcpy(s_selected_ssid, s_wifi[index].ssid, sizeof(s_selected_ssid));
    wifi_password_open();
}

static void wifi_scan_ready(lv_timer_t *timer)
{
    if(!s_poll || (timer && timer!=s_poll) || !s_center || !s_wifi_list || s_setup_dispatching)return;
    if (!s_wifi_scan_done) return;
    if (s_setup_step != SETUP_STEP_WIFI) {
        delete_poll();
        return;
    }
    delete_poll();
    if (!s_setup_content) return;
    if (!s_wifi_count) {
        set_status("No networks found. Press SCAN WI-FI to try again.");
        return;
    }
    set_status("Choose a network. Stronger signals have a less negative dBm value.");
    if (!s_wifi_list) return;
    lv_obj_update_layout(s_center);
    lv_obj_clean(s_wifi_list);
    for (size_t i = 0; i < s_wifi_count; ++i) {
        char row[56];
        snprintf(row, sizeof(row), "%.40s  |  %d dBm", s_wifi[i].ssid, (int)s_wifi[i].rssi);
        s_wifi_rows[i]=setup_choice(s_wifi_list,row,wifi_selected_cb,(void *)(uintptr_t)i);
        setup_watch(s_wifi_rows[i]);
    }
}

static void wifi_scan_open_cb(lv_event_t *event)
{
    (void)event;
    wifi_scan_open();
}

static void wifi_scan_open(void)
{
    if(!s_center || !s_setup_content || s_setup_dispatching)return;
    delete_poll();
    if(!setup_begin_content("Nearby networks","Scanning nearby networks..."))return;
    setup_text(s_stage,"Choose a network to enter its password.",UI_FONT_CAPTION,UI_TEXT_DIM);
    s_wifi_list=setup_plane(s_stage,LV_FLEX_FLOW_COLUMN);
    setup_watch(s_wifi_list);
    setup_action(s_actions,"Scan again",UI_POPUP_ACTION_SECONDARY,wifi_scan_open_cb);
    if(s_wifi_scan_busy){s_poll=lv_timer_create(wifi_scan_ready,120,NULL);return;}
    s_wifi_scan_busy=true;
    s_wifi_scan_done = false;
    if (xTaskCreate(wifi_scan_task, "setup_wifi_scan", 4096, NULL, 4, NULL) != pdPASS) {
        s_wifi_scan_busy=false;
        set_status("Unable to start Wi-Fi scan. Try again.");
        return;
    }
    s_poll = lv_timer_create(wifi_scan_ready, 120, NULL);
}

static void wifi_return_timer(lv_timer_t *timer)
{
    if(!timer || timer!=s_poll || !s_center || s_setup_dispatching)return;
    /* This callback is a one-shot transition, never a repeating renderer. */
    if (s_poll == timer) s_poll = NULL;
    lv_timer_delete(timer);
    setup_refresh_completion_state();
    setup_render_step(SETUP_STEP_WIFI);
}

static void wifi_verify(lv_timer_t *timer)
{
    if(!s_poll || (timer && timer!=s_poll) || !s_center || s_setup_dispatching)return;
    uint32_t epoch=s_setup_epoch;
    wifi_ap_record_t info;
    esp_err_t result=esp_wifi_sta_get_ap_info(&info);
    if(!setup_current(epoch))return;
    if (result != ESP_OK ||
        strcmp((const char *)info.ssid, s_selected_ssid)) {
        if (lv_tick_get() - s_wifi_verify_tick >= 30000) {
            delete_poll();
            set_status("Wi-Fi verification timed out. Check the password and tap Connect to retry.");
        }
        return;
    }
    delete_poll();
    s_wifi_done = true;
    set_status("Wi-Fi verified. Returning to Setup Center…");
    s_poll = lv_timer_create(wifi_return_timer, 700, NULL);
}

static void wifi_save_cb(lv_event_t *event)
{
    if(s_poll || !s_password || !s_selected_ssid[0] || !s_wifi_connect ||
        !setup_action_current(event,s_wifi_connect_action,true))return;
    char password[64],ssid[33];
    strlcpy(password,lv_textarea_get_text(s_password),sizeof(password));
    strlcpy(ssid,s_selected_ssid,sizeof(ssid));
    ui_setup_wizard_wifi_connect_cb_t connect=s_wifi_connect;
    uint32_t epoch=s_setup_epoch;
    s_setup_dispatching=true;
    bool accepted=connect(ssid,password);
    volatile char *secret=password;for(size_t i=0;i<sizeof(password);++i)secret[i]=0;
    s_setup_dispatching=false;
    if(!setup_current(epoch))return;
    if (!accepted) { set_status("Could not start the connection. Check the password and try again."); return; }
    set_status("Connecting and verifying Wi-Fi…");
    delete_poll();
    s_wifi_verify_tick = lv_tick_get();
    s_poll = lv_timer_create(wifi_verify, 250, NULL);
}



static void printer_editor_changed_cb(void)
{
    if(!s_center || !s_setup_content || s_setup_dispatching)return;
    setup_refresh_completion_state();
    setup_render_step(SETUP_STEP_PRINTER);
}

static void printer_editor_discover_selected(const char *host,int port,const char *identity,uint32_t request)
{
    (void)ui_printer_profiles_apply_discovered_endpoint(request,host,port,identity);
    moonraker_discovery_close_request(request);
}

static void printer_editor_discover_closed(uint32_t request)
{
    ui_printer_profiles_cancel_discovery_request(request);
}

static void printer_editor_discover_cb(void)
{
    uint32_t request=ui_printer_profiles_discovery_request();
    if(!request)return;
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    esp_netif_ip_info_t ip = {0};
    if (!netif || esp_netif_get_ip_info(netif, &ip) != ESP_OK || ip.ip.addr == 0) {
        set_status("Connect to Wi-Fi before discovering printers.");
        return;
    }
    if(!moonraker_discovery_show_in_parent_for_request(s_setup_content,
        "SETUP: searching for Moonraker printers...",printer_editor_discover_closed,
        printer_editor_discover_selected,request))return;
    if(request!=ui_printer_profiles_discovery_request()){moonraker_discovery_close_request(request);return;}
    (void)moonraker_discovery_start(&ip.ip);
}

static void printer_full_options_cb(lv_event_t *event)
{
    (void)event;
    if(!s_center || !s_setup_content || s_setup_dispatching)return;
    ui_printer_profiles_show_for_slot(0,
                                      printer_editor_changed_cb,
                                      printer_editor_discover_cb);
}

static bool camera_owner_current(void)
{
    if(!s_center || !s_setup_content)return false;
    if (s_camera_owner == moonraker_config_generation()) return true;
    delete_poll();
    s_selected_camera = -1;
    s_camera_verified=false;
    if (s_camera_list) lv_obj_clean(s_camera_list);
    set_status("Printer changed. Discover cameras again for the active printer.");
    return false;
}

static void camera_save_cb(lv_event_t *event)
{
    if (!setup_action_current(event,s_camera_use_action,false) || !s_camera_verified ||
        !camera_owner_current()) return;
    uint32_t epoch=s_setup_epoch,owner=s_camera_owner;
    const int profile = moonraker_config_active_profile_index();
    const int selected=s_selected_camera;
    camera_catalog_entry_t entry;
    s_setup_dispatching=true;
    bool available=selected>=0 && camera_catalog_get(profile,selected,&entry);
    if(!setup_current(epoch) || owner!=moonraker_config_generation() ||
        profile!=moonraker_config_active_profile_index()) {s_setup_dispatching=false;return;}
    if (!available || !entry.configured) {
        s_setup_dispatching=false;
        set_status("Select a discovered camera first.");
        return;
    }
    bool saved=camera_catalog_set_default(profile,selected);
    if(!setup_current(epoch) || owner!=moonraker_config_generation() ||
        profile!=moonraker_config_active_profile_index()) {s_setup_dispatching=false;return;}
    if(!saved) {
        s_setup_dispatching=false;set_status("Could not save the default camera. Tap Use camera to retry.");return;
    }
    saved=moonraker_config_set_camera_stream_url(profile,entry.stream_url);
    s_setup_dispatching=false;
    if(!setup_current(epoch) || profile!=moonraker_config_active_profile_index())return;
    if(!saved) {set_status("Default camera saved, but its stream URL could not be saved. Tap Use camera to retry.");return;}
    s_camera_owner=moonraker_config_generation();
    s_camera_done = true;
    setup_refresh_completion_state();
    setup_render_step(SETUP_STEP_CAMERA);
}

static void camera_actions(bool verified)
{
    if(!s_actions)return;
    s_camera_use_action=NULL;
    lv_obj_clean(s_actions);
    lv_obj_t *action=setup_action(s_actions,verified?"Use camera":"Discover cameras",
        verified?UI_POPUP_ACTION_CONFIRM:UI_POPUP_ACTION_SECONDARY,
        verified?camera_save_cb:center_camera_cb);
    if(verified){s_camera_use_action=action;setup_watch(action);}
    lv_obj_set_width(action,LV_PCT(48));lv_obj_set_flex_grow(action,1);
    lv_obj_t *skip=setup_action(s_actions,"Skip camera",UI_POPUP_ACTION_SECONDARY,setup_skip_cb);
    lv_obj_set_width(skip,LV_PCT(48));lv_obj_set_flex_grow(skip,1);
}

static void camera_test_poll(lv_timer_t *timer)
{
    if(!s_poll || (timer && timer!=s_poll) || s_setup_dispatching)return;
    if (!camera_owner_current()) return;
    uint32_t epoch=s_setup_epoch,owner=s_camera_owner;
    bool ok = false;
    int width = 0;
    int height = 0;
    size_t bytes = 0;
    s_setup_dispatching=true;
    bool received=camera_test_take_result(&ok, &width, &height, &bytes);
    s_setup_dispatching=false;
    if(!setup_current(epoch) || owner!=moonraker_config_generation() || !received)return;
    delete_poll();
    if (!ok) {
        set_status("Camera test failed. Select another camera.");
        return;
    }
    char verified[96];
    snprintf(verified, sizeof(verified), "Camera verified: %d x %d JPEG. Save it for this printer.", width, height);
    set_status(verified);
    s_camera_verified=true;
    camera_actions(true);
}

static void camera_selected_cb(lv_event_t *event)
{
    if(!event || s_poll || s_setup_dispatching || !s_camera_list)return;
    uintptr_t slot=(uintptr_t)lv_event_get_user_data(event);
    if(slot>=CAMERA_CATALOG_MAX_CAMERAS || !setup_action_current(event,s_camera_rows[slot],false))return;
    if (!camera_owner_current()) return;
    uint32_t epoch=s_setup_epoch,owner=s_camera_owner;
    s_selected_camera = (int)slot;
    s_camera_verified=false;
    const int profile = moonraker_config_active_profile_index();
    camera_catalog_entry_t entry;
    s_setup_dispatching=true;
    bool available=camera_catalog_get(profile,s_selected_camera,&entry);
    s_setup_dispatching=false;
    if(!setup_current(epoch) || owner!=moonraker_config_generation() || !available || !entry.configured)return;
    char detail[240]; snprintf(detail, sizeof(detail), "Selected: %s. Testing its stream…", entry.name);
    set_status(detail);
    camera_actions(false);
    epoch=s_setup_epoch;
    s_setup_dispatching=true;
    bool started=camera_test_start(entry.stream_url);
    s_setup_dispatching=false;
    if(!setup_current(epoch) || owner!=moonraker_config_generation())return;
    if (!started) { set_status("Could not start the camera test. Wait and select the camera again."); return; }
    delete_poll();
    s_poll = lv_timer_create(camera_test_poll, 150, NULL);
}

static void camera_discovery_poll(lv_timer_t *timer)
{
    if(!s_poll || (timer && timer!=s_poll) || s_setup_dispatching)return;
    if (!camera_owner_current()) return;
    uint32_t epoch=s_setup_epoch;
    int original_profile=moonraker_config_active_profile_index();
    if (camera_discovery_busy()) return;
    bool found = false;
    size_t count = 0;
    moonraker_webcam_t ignored;
    s_setup_dispatching=true;
    bool received=camera_discovery_take_result(&ignored, &found, &count);
    s_setup_dispatching=false;
    if(!setup_current(epoch) || original_profile!=moonraker_config_active_profile_index() || !received)return;
    /* Importing camera slot 1 advances our own active configuration. */
    s_camera_owner = moonraker_config_generation();
    delete_poll();
    const int profile = moonraker_config_active_profile_index();
    if (!found || !count) {
        set_status("No enabled cameras found for this printer. Scan again or continue without one.");

        return;
    }
    set_status(ui_text("SELECT A CAMERA TO TEST ITS STREAM."));
    if (!s_camera_list) return;
    lv_obj_update_layout(s_center);
    lv_obj_clean(s_camera_list);
    for (int slot = 0; slot < CAMERA_CATALOG_MAX_CAMERAS; ++slot) {
        camera_catalog_entry_t entry;
        if (!camera_catalog_get(profile, slot, &entry) || !entry.configured) continue;
        s_camera_rows[slot]=setup_choice(s_camera_list,entry.name[0]?entry.name:"Camera",camera_selected_cb,(void *)(uintptr_t)slot);
        setup_watch(s_camera_rows[slot]);
    }
}

static void camera_open(void)
{
    if(!s_center || !s_setup_content || s_setup_dispatching)return;
    delete_poll();
    s_camera_owner = moonraker_config_generation();
    s_selected_camera = -1;
    if(!setup_begin_content("Printer cameras","Searching this printer for configured cameras..."))return;
    s_camera_list=setup_plane(s_stage,LV_FLEX_FLOW_COLUMN);
    setup_watch(s_camera_list);
    camera_actions(false);
    const int profile = moonraker_config_active_profile_index();
    const moonraker_profile_t *printer = moonraker_config_profile(profile);
    if (!printer || !printer->configured) {
        set_status("Add and verify a printer first, then return here.");
        return;
    }
    moonraker_profile_t selected=*printer;
    uint32_t epoch=s_setup_epoch,owner=s_camera_owner;
    s_setup_dispatching=true;
    bool started=camera_discovery_start(selected.host,selected.port,selected.api_key);
    s_setup_dispatching=false;
    if(!setup_current(epoch) || owner!=moonraker_config_generation())return;
    if (!started) {
        set_status("Could not start camera discovery.");
        return;
    }
    s_poll = lv_timer_create(camera_discovery_poll, 150, NULL);
}

static bool setup_wifi_is_ready(void)
{
    wifi_ap_record_t info;
    return esp_wifi_sta_get_ap_info(&info) == ESP_OK;
}


static void setup_refresh_completion_state(void)
{
    s_wifi_done = setup_wifi_is_ready();

    const int profile_index = moonraker_config_active_profile_index();
    const moonraker_profile_t *profile = moonraker_config_profile(profile_index);
    s_printer_done = profile && profile->configured;

    const char *camera_url =
        moonraker_config_camera_stream_url(profile_index);
    s_camera_done = camera_url && camera_url[0];
}

static void completion_deleted(lv_event_t *event)
{
    if(lv_event_get_target_obj(event)==s_complete_popup)s_complete_popup=NULL;
}

static void setup_completion_done_cb(lv_event_t *event)
{
    (void)event;
    if (s_complete_popup) {
        lv_obj_t *popup=s_complete_popup;
        s_complete_popup = NULL;
        lv_obj_delete(popup);
    }
}


static void show_setup_completion(void)
{
    int width=lv_display_get_horizontal_resolution(NULL)-32;
    if(width>660)width=660;
    s_complete_popup=ui_popup_create(lv_screen_active(),width,300,UI_POPUP_STANDARD);
    if(!s_complete_popup)return;
    lv_obj_add_event_cb(s_complete_popup,completion_deleted,LV_EVENT_DELETE,NULL);
    lv_obj_set_flex_flow(s_complete_popup,LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(s_complete_popup,20,0);
    lv_obj_set_style_pad_row(s_complete_popup,UI_GAP_ROW,0);
    setup_text(s_complete_popup,"You're all set",UI_FONT_TITLE,UI_TEXT);
    lv_obj_t *body=setup_plane(s_complete_popup,LV_FLEX_FLOW_COLUMN);
    lv_obj_set_height(body,0);lv_obj_set_flex_grow(body,1);lv_obj_add_flag(body,LV_OBJ_FLAG_SCROLLABLE);
    setup_text(body,"Your print cell is ready. Reopen Setup Center from Settings whenever you need it.",UI_FONT_BODY,UI_TEXT);
    setup_text(body,"Wi-Fi and printer settings are saved. A camera remains optional.",UI_FONT_CAPTION,UI_TEXT_DIM);
    setup_action(s_complete_popup,"Done",UI_POPUP_ACTION_CONFIRM,setup_completion_done_cb);
    lv_obj_move_foreground(s_complete_popup);
}


static void finish_cb(lv_event_t *event)
{
    if(!setup_action_current(event,s_finish_action,false))return;
    uint32_t epoch=s_setup_epoch;
    setup_refresh_completion_state();
    if(!setup_current(epoch))return;
    if (!s_wifi_done || !s_printer_done) {
        set_status("Connect Wi-Fi and verify a printer before finishing setup.");
        return;
    }
    s_setup_dispatching=true;
    bool saved=onboarding_controller_mark_complete();
    s_setup_dispatching=false;
    if(!setup_current(epoch))return;
    if (!saved) {
        set_status("Could not save setup completion. Try again.");
        return;
    }
    ui_setup_wizard_close();
    if(s_center || s_complete_popup)return;
    show_setup_completion();
}
static void later_cb(lv_event_t *event) { (void)event; ui_setup_wizard_close(); }

static void center_wifi_cb(lv_event_t *event) { (void)event; wifi_scan_open(); }
static void center_printer_cb(lv_event_t *event) { printer_full_options_cb(event); }
static void center_camera_cb(lv_event_t *event) { (void)event; camera_open(); }



void ui_setup_wizard_show(ui_setup_wizard_wifi_connect_cb_t wifi_connect_cb)
{
    if(s_center){lv_obj_move_foreground(s_center);return;}
    if(s_complete_popup){lv_obj_t *popup=s_complete_popup;s_complete_popup=NULL;lv_obj_delete(popup);if(s_center || s_complete_popup)return;}
    setup_refresh_completion_state();
    s_wifi_connect = wifi_connect_cb;
    show_center();
}
