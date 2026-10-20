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
#include "timezone_config.h"
#include "theme_manager.h"
#include "ui_settings.h"

#include "camera_catalog_controller.h"
#include "camera_discovery_controller.h"
#include "camera_test_controller.h"
#include "moonraker_config_controller.h"
#include "moonraker_discovery.h"
#include "moonraker_endpoint_test.h"
#include "onboarding_controller.h"
#include "ui_printer_profiles.h"
#include "ui_popup.h"
#include "ui_form_dialog.h"
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
    SETUP_STEP_TIMEZONE,
    SETUP_STEP_DISPLAY,
    SETUP_STEP_PRINTER,
    SETUP_STEP_CAMERA,
    SETUP_STEP_COMPLETE,
    SETUP_STEP_COUNT,
};

static int s_setup_step = SETUP_STEP_WELCOME;
static lv_obj_t *s_setup_content;
static lv_obj_t *s_stage;
static lv_obj_t *s_actions;
static lv_obj_t *s_nav[SETUP_STEP_COUNT];
static lv_obj_t *s_summary, *s_back_action, *s_options_list;
static lv_obj_t *s_brightness_slider, *s_brightness_value;
static void (*s_appearance_changed)(void);
static unsigned s_appearance_choice;
static volatile bool s_wifi_scan_busy;
static uint32_t s_setup_epoch;
static bool s_setup_dispatching, s_camera_verified;
static lv_obj_t *s_wifi_connect_action, *s_camera_use_action, *s_camera_scan_action, *s_finish_action;
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
        &s_camera_list,&s_options_list,&s_brightness_slider,&s_brightness_value,&s_back_action,&s_setup_content,&s_stage,&s_actions,&s_summary,
        &s_wifi_connect_action,&s_camera_use_action,&s_camera_scan_action,&s_finish_action};
    bool retired=false;
    for (unsigned i=0;i<sizeof(fields)/sizeof(fields[0]);++i)
        if (*fields[i]==object) { *fields[i]=NULL; retired=true; }
    for (unsigned i=0;i<SETUP_STEP_COUNT;++i)if(s_nav[i]==object){s_nav[i]=NULL;retired=true;}
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
static void setup_select_timezone_cb(lv_event_t *event);
static void setup_select_display_cb(lv_event_t *event);
static lv_obj_t *setup_results_list(lv_obj_t *parent);
static void setup_timezone_content(void);
static void setup_display_content(void);
static bool setup_network_content(void);
static void setup_printer_content(void);
static bool setup_camera_content(void);
static void camera_actions(bool verified);
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
static void setup_back_cb(lv_event_t *event)
{
    if(setup_action_current(event,s_back_action,false) && s_setup_step>0)
        setup_render_step(s_setup_step-1);
}
static void setup_select_timezone_cb(lv_event_t *event)
{ (void)event; if(!s_setup_dispatching)setup_render_step(SETUP_STEP_TIMEZONE); }
static void setup_select_display_cb(lv_event_t *event)
{ (void)event; if(!s_setup_dispatching)setup_render_step(SETUP_STEP_DISPLAY); }
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
    s_camera_list = s_options_list = s_brightness_slider = s_brightness_value = NULL;
    s_wifi_connect_action=s_camera_use_action=s_camera_scan_action=s_finish_action=NULL;
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
    if(ui_theme_is_studio()) {
        ui_apply_surface_role(row,UI_SURFACE_TRANSPARENT);
        ui_apply_divider_border(row,LV_BORDER_SIDE_BOTTOM);
        lv_obj_set_style_pad_hor(row,12,0);lv_obj_set_style_pad_ver(row,8,0);
    }
    lv_obj_set_style_text_align(lv_obj_get_child(row,0),LV_TEXT_ALIGN_LEFT,0);
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
    const char *names[]={"Welcome","Network","Time zone","Display","Printer","Camera","Review"};
    bool ready[]={false,s_wifi_done,false,false,s_printer_done,s_camera_done,false};
    if(s_back_action) { if(step==0)lv_obj_add_state(s_back_action,LV_STATE_DISABLED);else lv_obj_remove_state(s_back_action,LV_STATE_DISABLED); }
    for(int i=0;i<SETUP_STEP_COUNT;i++) {
        if(!s_nav[i])continue;
        char text[80];snprintf(text,sizeof(text),"%s %s",ready[i]?LV_SYMBOL_OK:(i==step?LV_SYMBOL_RIGHT:""),names[i]);
        lv_obj_t *label=lv_obj_get_child(s_nav[i],0);
        if(!label || !lv_obj_check_type(label,&lv_label_class))continue;
        lv_label_set_text(label,text);
        ui_text_fit_single_line(label,UI_FONT_BODY);
        lv_label_set_long_mode(label,LV_LABEL_LONG_WRAP);
        lv_obj_set_height(label,LV_SIZE_CONTENT);
        ui_button_set_filter_active(s_nav[i],i==step);
    }
    char summary[80];snprintf(summary,sizeof(summary),"%d of 2 essentials ready\nCamera is optional",(int)s_wifi_done+(int)s_printer_done);
    if(s_summary)lv_label_set_text(s_summary,summary);
}

static bool setup_begin_content(const char *title,const char *detail)
{
    if(!setup_clear_content())return false;
    setup_text(s_setup_content,title,UI_FONT_TITLE,UI_TEXT);
    s_status=setup_text(s_setup_content,detail,UI_FONT_BODY,UI_TEXT_DIM);
    s_stage=setup_plane(s_setup_content,LV_FLEX_FLOW_COLUMN);
    setup_watch(s_stage);
    lv_obj_set_height(s_stage,0);lv_obj_set_flex_grow(s_stage,1);
    lv_obj_add_flag(s_stage,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(s_stage,LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_stage,LV_SCROLLBAR_MODE_OFF);
    s_actions=setup_plane(s_setup_content,LV_FLEX_FLOW_ROW_WRAP);
    ui_apply_divider_border(s_actions,LV_BORDER_SIDE_TOP);
    lv_obj_set_style_pad_top(s_actions,8,0);
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

static void setup_review_row(const char *name,const char *detail,bool ready,bool optional,const char *saved_label)
{
    lv_obj_t *row=setup_plane(s_stage,LV_FLEX_FLOW_ROW);
    lv_obj_set_height(row,48);
    if(ui_theme_is_studio())ui_apply_divider_border(row,LV_BORDER_SIDE_BOTTOM);
    else {
        lv_obj_set_style_bg_color(row,UI_CONTROL,0);
        lv_obj_set_style_bg_opa(row,LV_OPA_30,0);
        lv_obj_set_style_radius(row,12,0);
    }
    lv_obj_set_style_pad_all(row,8,0);
    lv_obj_set_flex_align(row,LV_FLEX_ALIGN_SPACE_BETWEEN,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
    bool wide=lv_display_get_horizontal_resolution(NULL)>=800;
    lv_obj_t *label=setup_text(row,name,UI_FONT_BODY,UI_TEXT);
    lv_obj_set_width(label,wide?LV_PCT(18):LV_PCT(40));lv_obj_set_height(label,UI_FONT_BODY->line_height);lv_label_set_long_mode(label,LV_LABEL_LONG_DOT);
    if(wide) {
        lv_obj_t *value=setup_text(row,detail,UI_FONT_BODY,ready?UI_TEXT:UI_TEXT_DIM);
        lv_obj_set_width(value,LV_PCT(50));lv_obj_set_height(value,UI_FONT_BODY->line_height);lv_label_set_long_mode(value,LV_LABEL_LONG_DOT);
    }
    lv_obj_t *status=setup_text(row,ready?saved_label:(optional?"Optional":"Needs setup"),UI_FONT_CAPTION,ready?UI_OK_BRIGHT:UI_TEXT_DIM);
    lv_obj_set_width(status,wide?LV_PCT(26):LV_PCT(54));lv_obj_set_height(status,UI_FONT_CAPTION->line_height);lv_label_set_long_mode(status,LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(status,LV_TEXT_ALIGN_RIGHT,0);
}


/* Preference writes use the same transactional services as Settings. */
static void setup_timezone_pick(lv_event_t *event)
{
    if(!event || !s_options_list || s_setup_step!=SETUP_STEP_TIMEZONE || s_setup_dispatching)return;
    lv_obj_t *button=lv_event_get_current_target_obj(event);
    if(lv_event_get_target_obj(event)!=button || lv_obj_get_parent(button)!=s_options_list)return;
    size_t index=(size_t)(uintptr_t)lv_event_get_user_data(event);
    uint32_t epoch=s_setup_epoch;s_setup_dispatching=true;
    bool saved=timezone_config_select(index);s_setup_dispatching=false;
    if(!setup_current(epoch))return;
    if(!saved){set_status("Could not save the time zone. Try again.");return;}
    for(unsigned i=0;i<lv_obj_get_child_count(s_options_list);++i) {
        lv_obj_t *row=lv_obj_get_child(s_options_list,i);
        /* Catalog indexes are stored on the row; missing entries never shift them. */
        ui_button_set_filter_active(row,(size_t)(uintptr_t)lv_obj_get_user_data(row)==index);
    }
    set_status(timezone_config_selected_label());
}
static void setup_timezone_content(void)
{
    s_options_list=setup_results_list(s_stage);setup_watch(s_options_list);
    size_t selected=timezone_config_selected_index();
    for(size_t i=0;i<timezone_config_count();++i) {
        const timezone_config_entry_t *entry=timezone_config_entry(i);if(!entry)continue;
        lv_obj_t *row=setup_choice(s_options_list,entry->label,setup_timezone_pick,(void*)(uintptr_t)i);
        lv_obj_set_user_data(row,(void*)(uintptr_t)i);
        ui_button_set_filter_active(row,i==selected);
    }
    set_status(timezone_config_selected_label());
}
static void setup_appearance_async(void *data)
{
    uint32_t epoch=(uint32_t)(uintptr_t)data;
    if(!setup_current(epoch))return;
    ui_setup_wizard_wifi_connect_cb_t wifi=s_wifi_connect;
    void (*changed)(void)=s_appearance_changed;
    unsigned choice=s_appearance_choice;
    bool saved;
    if(choice<2)saved=theme_manager_select(choice==0?UI_THEME_STUDIO_DARK:UI_THEME_OPERATOR);
    else {
        ui_accessibility_t options=theme_manager_accessibility();options.large_text=(choice==3);
        saved=theme_manager_set_accessibility(options);
    }
    s_setup_dispatching=false;
    if(!setup_current(epoch))return;
    if(!saved){set_status("Could not save appearance. Try again.");return;}
    ui_setup_wizard_close();
    if(s_center)return;
    if(changed)changed();
    if(s_center)return;
    ui_setup_wizard_show_with_appearance_callback(wifi,changed);
    if(s_center)setup_render_step(SETUP_STEP_DISPLAY);
}
static void setup_appearance_pick(lv_event_t *event)
{
    if(!event || s_setup_step!=SETUP_STEP_DISPLAY || s_setup_dispatching || !s_stage)return;
    lv_obj_t *button=lv_event_get_current_target_obj(event);
    if(lv_event_get_target_obj(event)!=button || lv_obj_has_state(button,LV_STATE_DISABLED))return;
    s_appearance_choice=(unsigned)(uintptr_t)lv_event_get_user_data(event);
    /* Commit and rebuild together after the button's event stack unwinds.
     * A closed/replaced Setup invalidates the queued change before it writes. */
    if(lv_async_call(setup_appearance_async,(void*)(uintptr_t)s_setup_epoch)!=LV_RESULT_OK) {
        set_status("Could not refresh appearance. Try again.");return;
    }
    s_setup_dispatching=true;
}
static bool setup_save_display(int brightness,uint8_t sleep)
{
    uint32_t epoch=s_setup_epoch;s_setup_dispatching=true;
    bool saved=ui_settings_restore_display_preferences(brightness,sleep);s_setup_dispatching=false;
    if(!setup_current(epoch))return false;
    set_status(saved?"Display preferences saved.":"Could not save display preferences. Try again.");
    return saved;
}
static void setup_brightness_cb(lv_event_t *event)
{
    if(!event || s_setup_dispatching || !s_brightness_slider || !s_brightness_value ||
        lv_event_get_target_obj(event)!=s_brightness_slider)return;
    int value=lv_slider_get_value(s_brightness_slider);
    if(lv_event_get_code(event)==LV_EVENT_RELEASED) {
        uint32_t epoch=s_setup_epoch;
        if(!setup_save_display(value,ui_settings_sleep_timeout_minutes()) && setup_current(epoch))
            lv_slider_set_value(s_brightness_slider,ui_settings_brightness_percent(),LV_ANIM_OFF);
        if(!setup_current(epoch) || !s_brightness_value || !s_brightness_slider)return;
        value=lv_slider_get_value(s_brightness_slider);
    }
    lv_label_set_text_fmt(s_brightness_value,"Brightness\n%d%%",value);
}
static void setup_sleep_pick(lv_event_t *event)
{
    if(!event || s_setup_dispatching || s_setup_step!=SETUP_STEP_DISPLAY)return;
    lv_obj_t *button=lv_event_get_current_target_obj(event);
    if(lv_event_get_target_obj(event)!=button)return;
    uint32_t epoch=s_setup_epoch;
    uint8_t sleep=(uint8_t)(uintptr_t)lv_event_get_user_data(event);
    if(setup_save_display(ui_settings_brightness_percent(),sleep) && setup_current(epoch))
        setup_render_step(SETUP_STEP_DISPLAY);
}
static lv_obj_t *setup_preference_pair(const char *title)
{
    lv_obj_t *group=setup_plane(s_stage,LV_FLEX_FLOW_ROW);
    lv_obj_set_height(group,48);
    lv_obj_set_flex_align(group,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
    lv_obj_t *label=setup_text(group,title,UI_FONT_CAPTION,UI_TEXT_DIM);
    lv_obj_set_width(label,LV_PCT(25));
    lv_obj_t *row=setup_plane(group,LV_FLEX_FLOW_ROW);
    lv_obj_set_width(row,0);lv_obj_set_flex_grow(row,1);lv_obj_set_height(row,48);
    lv_obj_set_flex_align(row,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
    return row;
}
static void setup_display_content(void)
{
    lv_obj_t *row=setup_preference_pair("Theme");
    const char *themes[]={"Studio dark","Operator"};
    for(unsigned i=0;i<2;++i) {
        lv_obj_t *button=setup_choice(row,themes[i],setup_appearance_pick,(void*)(uintptr_t)i);
        lv_obj_set_width(button,LV_PCT(48));lv_obj_set_flex_grow(button,1);
        ui_button_set_filter_active(button,theme_manager_active()==(i?UI_THEME_OPERATOR:UI_THEME_STUDIO_DARK));
    }
    row=setup_preference_pair("Text size");
    for(unsigned i=0;i<2;++i) {
        lv_obj_t *button=setup_choice(row,i?"Larger":"Standard",setup_appearance_pick,(void*)(uintptr_t)(i+2));
        lv_obj_set_width(button,LV_PCT(48));lv_obj_set_flex_grow(button,1);
        ui_button_set_filter_active(button,theme_manager_accessibility().large_text==(bool)i);
    }
    row=setup_preference_pair("Brightness");
    s_brightness_value=lv_obj_get_child(lv_obj_get_parent(row),0);
    lv_label_set_text_fmt(s_brightness_value,"Brightness\n%d%%",ui_settings_brightness_percent());
    s_brightness_slider=lv_slider_create(row);setup_watch(s_brightness_slider);
    ui_apply_slider_style(s_brightness_slider);
    lv_obj_set_width(s_brightness_slider,LV_PCT(90));lv_obj_set_height(s_brightness_slider,20);
    lv_obj_set_style_pad_left(row,12,0);lv_obj_set_style_pad_right(row,12,0);

    lv_slider_set_range(s_brightness_slider,10,100);
    lv_slider_set_value(s_brightness_slider,ui_settings_brightness_percent(),LV_ANIM_OFF);
    lv_obj_add_event_cb(s_brightness_slider,setup_brightness_cb,LV_EVENT_VALUE_CHANGED,NULL);
    lv_obj_add_event_cb(s_brightness_slider,setup_brightness_cb,LV_EVENT_RELEASED,NULL);
    row=setup_preference_pair("Sleep after inactivity");
    lv_obj_set_flex_flow(row,LV_FLEX_FLOW_ROW_WRAP);
    const char *labels[]={"Never","5 min","15 min","30 min"};
    const uint8_t values[]={0,5,15,30};
    for(unsigned i=0;i<4;++i) {
        lv_obj_t *button=setup_choice(row,labels[i],setup_sleep_pick,(void*)(uintptr_t)values[i]);
        lv_obj_set_width(button,LV_PCT(23));lv_obj_set_flex_grow(button,1);
        ui_button_set_filter_active(button,ui_settings_sleep_timeout_minutes()==values[i]);
    }
}

static void setup_fit_actions(lv_obj_t *parent)
{
    for(unsigned i=0;i<lv_obj_get_child_count(parent);++i) {
        lv_obj_t *child=lv_obj_get_child(parent,i);
        if(lv_obj_check_type(child,&lv_button_class))lv_obj_send_event(child,LV_EVENT_SIZE_CHANGED,NULL);
        else setup_fit_actions(child);
    }
}

/* Read-only facts use the same flat/surfaced row treatment as Review. */
static void setup_connection_fact(const char *title,const char *value)
{
    lv_obj_t *row=setup_plane(s_stage,LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(row,2,0);lv_obj_set_style_pad_hor(row,8,0);lv_obj_set_style_pad_ver(row,6,0);
    if(ui_theme_is_studio())ui_apply_divider_border(row,LV_BORDER_SIDE_BOTTOM);
    else {
        lv_obj_set_style_bg_color(row,UI_CONTROL,0);lv_obj_set_style_bg_opa(row,LV_OPA_30,0);
        lv_obj_set_style_radius(row,12,0);
    }
    setup_text(row,title,UI_FONT_CAPTION,UI_TEXT_DIM);
    setup_text(row,value,UI_FONT_BODY,UI_TEXT);
}
static void setup_network_refresh_cb(lv_event_t *event)
{ (void)event; if(!s_setup_dispatching)setup_render_step(SETUP_STEP_WIFI); }
static bool setup_network_content(void)
{
    uint32_t epoch=s_setup_epoch;
    wifi_ap_record_t ap={0};esp_netif_ip_info_t ip={0};
    s_setup_dispatching=true;
    bool connected=esp_wifi_sta_get_ap_info(&ap)==ESP_OK;
    esp_netif_t *netif=esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    bool has_ip=connected && netif && esp_netif_get_ip_info(netif,&ip)==ESP_OK && ip.ip.addr!=0;
    s_setup_dispatching=false;
    if(!setup_current(epoch))return false;
    char address[32];
    if(has_ip)snprintf(address,sizeof(address),IPSTR,IP2STR(&ip.ip));
    else snprintf(address,sizeof(address),"%s",connected?"Waiting for an address":"Not connected");
    setup_connection_fact("Wi-Fi network",connected?(const char*)ap.ssid:"No network connected");
    setup_connection_fact("Display address",address);
    lv_obj_t *row=setup_plane(s_stage,LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_t *find=setup_action(row,connected?"Change network":"Find a network",UI_POPUP_ACTION_SECONDARY,center_wifi_cb);
    lv_obj_set_width(find,LV_PCT(48));lv_obj_set_flex_grow(find,1);
    lv_obj_t *refresh=setup_action(row,"Refresh status",UI_POPUP_ACTION_SECONDARY,setup_network_refresh_cb);
    lv_obj_set_width(refresh,LV_PCT(48));lv_obj_set_flex_grow(refresh,1);
    set_status(connected?"Wi-Fi is connected. You can keep this network or choose another.":"Connect this display to the same network as your printer.");
    return true;
}
static void setup_printer_content(void)
{
    int index=moonraker_config_active_profile_index();
    const moonraker_profile_t *source=moonraker_config_profile(index);
    moonraker_profile_t profile={0};if(source)profile=*source;
    if(profile.configured) {
        char endpoint[112];
        snprintf(endpoint,sizeof(endpoint),"%s://%s:%d",profile.secure_transport?"https":"http",profile.host,profile.port);
        setup_connection_fact("Active printer",profile.name[0]?profile.name:"Printer");
        setup_connection_fact("Moonraker endpoint",endpoint);
        setup_text(s_stage,profile.api_key[0]?"API key saved":"No API key saved",UI_FONT_CAPTION,UI_TEXT_DIM);
        set_status("Profile saved. Live connection status is available on the Printer page.");
    } else {
        setup_text(s_stage,"Find Moonraker on your network or enter its address in printer setup.",UI_FONT_BODY_LARGE,UI_TEXT);
        setup_text(s_stage,"The shared editor includes discovery, connection testing and security settings.",UI_FONT_BODY,UI_TEXT_DIM);
    }
    setup_action(s_stage,profile.configured?"Edit printer":"Add printer",UI_POPUP_ACTION_SECONDARY,center_printer_cb);
}

static bool setup_camera_content(void)
{
    const int index=moonraker_config_active_profile_index();
    const moonraker_profile_t *source=moonraker_config_profile(index);
    moonraker_profile_t printer={0};if(source)printer=*source;
    if(!printer.configured) {
        setup_text(s_stage,"Camera setup is optional.",UI_FONT_BODY_LARGE,UI_TEXT);
        setup_text(s_stage,"Add your printer first to discover its cameras. You can also continue without one.",UI_FONT_BODY,UI_TEXT_DIM);
        setup_action(s_stage,"Set up printer",UI_POPUP_ACTION_SECONDARY,setup_select_printer_cb);
        return true;
    }
    if(s_camera_done) {
        uint32_t epoch=s_setup_epoch,generation=moonraker_config_generation();
        camera_catalog_entry_t camera={0};
        s_setup_dispatching=true;
        size_t selected=camera_catalog_default(index);
        bool found=selected<CAMERA_CATALOG_MAX_CAMERAS && camera_catalog_get(index,selected,&camera);
        s_setup_dispatching=false;
        if(!setup_current(epoch) || generation!=moonraker_config_generation() || index!=moonraker_config_active_profile_index())return false;
        const char *url=moonraker_config_camera_stream_url(index);
        /* Custom URLs need not match a catalog slot. Never expose a URL that
         * could contain credentials, or claim a mismatched slot is selected. */
        bool matches=found && camera.configured && url && !strcmp(camera.stream_url,url);
        setup_connection_fact("Saved camera",matches && camera.name[0]?camera.name:"Custom camera");
        setup_connection_fact("Printer",printer.name[0]?printer.name:"Printer");
        set_status("Camera settings are saved. Discover and test to check its stream.");
    } else {
        setup_text(s_stage,"Camera setup is optional.",UI_FONT_BODY_LARGE,UI_TEXT);
        setup_connection_fact("Printer",printer.name[0]?printer.name:"Printer");
        setup_text(s_stage,"Discover a camera and test its stream before selecting Use camera.",UI_FONT_CAPTION,UI_TEXT_DIM);
    }
    setup_action(s_stage,s_camera_done?"Discover and test":"Discover cameras",UI_POPUP_ACTION_SECONDARY,center_camera_cb);
    return true;
}

static bool setup_review_content(bool final)
{
    uint32_t epoch=s_setup_epoch,generation=moonraker_config_generation();
    int index=moonraker_config_active_profile_index();
    wifi_ap_record_t network={0};
    s_setup_dispatching=true;
    bool connected=esp_wifi_sta_get_ap_info(&network)==ESP_OK;
    s_setup_dispatching=false;
    if(!setup_current(epoch) || generation!=moonraker_config_generation() || index!=moonraker_config_active_profile_index())return false;
    const moonraker_profile_t *printer=moonraker_config_profile(index);
    bool configured=printer && printer->configured;
    setup_review_row("Wi-Fi",connected?(const char*)network.ssid:"Choose a network",connected,false,LV_SYMBOL_OK " Connected");
    setup_review_row("Printer",configured?(printer->name[0]?printer->name:"Active profile"):"Add a printer",configured,false,LV_SYMBOL_OK " Saved");
    setup_review_row("Camera",s_camera_done?"Camera configured":"Add later",s_camera_done,true,LV_SYMBOL_OK " Saved");
    if(final) {
        char zone[144];snprintf(zone,sizeof(zone),"Time zone: %s",timezone_config_selected_label());
        lv_obj_t *zone_label=setup_text(s_stage,zone,UI_FONT_BODY,UI_TEXT);
        lv_obj_set_style_text_line_space(zone_label,4,0);
        unsigned sleep=ui_settings_sleep_timeout_minutes();
        const char *timeout=sleep==0?"Sleep off":sleep==5?"Sleep 5 min":sleep==15?"Sleep 15 min":"Sleep 30 min";
        const char *theme=theme_manager_active()==UI_THEME_STUDIO_DARK?"Studio Dark":theme_manager_active()==UI_THEME_OPERATOR?"Operator":theme_manager_active_label();
        char display[160];snprintf(display,sizeof(display),"Display: %s / %s text / %d%% / %s",theme,
            theme_manager_accessibility().large_text?"Larger":"Standard",ui_settings_brightness_percent(),timeout);
        lv_obj_t *display_label=setup_text(s_stage,display,UI_FONT_CAPTION,UI_TEXT_DIM);
        lv_obj_set_style_text_line_space(display_label,4,0);
    } else {
        setup_text(s_stage,"Use Continue to walk through setup, or select a step. Saved choices stay in place when you leave.",UI_FONT_BODY,UI_TEXT_DIM);
    }
    return true;
}

static void setup_render_step(int step)
{
    if(!s_setup_content || step<0 || step>SETUP_STEP_COMPLETE)return;
    delete_poll();
    s_setup_step=step;
    setup_refresh_completion_state();
    setup_update_nav(step);
    const char *titles[]={"Welcome to your print cell","Connect your network","Set your time zone","Display & appearance","Add your printer","Connect a camera","Review your setup"};
    const char *details[]={"Make this display yours. Your choices save as you go.","Scan nearby Wi-Fi networks, choose one, then verify the connection.","Choose the region used by the clock and event history.","Choose a theme, text size, brightness and sleep timeout.","Discover your printer or enter its Moonraker endpoint. Security and camera options are included.","Discover and test this printer's cameras. You can add one later.","Choices save as you go. Finish marks setup complete."};
    const char *title=step==SETUP_STEP_WIFI && s_wifi_done?"Your network":step==SETUP_STEP_PRINTER && s_printer_done?"Your printer":step==SETUP_STEP_CAMERA && s_camera_done?"Your camera":titles[step];
    if(!setup_begin_content(title,details[step]))return;
    if(step==SETUP_STEP_WELCOME || step==SETUP_STEP_COMPLETE) {
        if(!setup_review_content(step==SETUP_STEP_COMPLETE))return;
        if(step==SETUP_STEP_WELCOME)setup_add_next_buttons(false);
        else {s_finish_action=setup_action(s_actions,"Finish setup",UI_POPUP_ACTION_CONFIRM,finish_cb);setup_watch(s_finish_action);}
    } else if(step==SETUP_STEP_TIMEZONE) {
        setup_timezone_content();setup_add_next_buttons(false);
    } else if(step==SETUP_STEP_DISPLAY) {
        setup_display_content();setup_add_next_buttons(false);
    } else if(step==SETUP_STEP_WIFI) {
        if(!setup_network_content())return;
        setup_add_next_buttons(!s_wifi_done);
    } else if(step==SETUP_STEP_PRINTER) {
        setup_printer_content();setup_add_next_buttons(!s_printer_done);
    } else {
        if(!setup_camera_content())return;
        setup_add_next_buttons(!s_camera_done);
    }
    lv_obj_update_layout(s_center);
    setup_fit_actions(s_setup_content);
}

static void center_deleted(lv_event_t *event)
{
    if(lv_event_get_target_obj(event)!=s_center)return;
    ++s_setup_epoch;delete_poll();
    s_center=s_setup_content=s_stage=s_actions=s_status=s_name=s_password=s_keyboard=NULL;
    s_wifi_list=s_camera_list=s_summary=s_options_list=s_brightness_slider=s_brightness_value=s_back_action=NULL;
    memset(s_nav,0,sizeof(s_nav));
    memset(s_wifi_rows,0,sizeof(s_wifi_rows));memset(s_camera_rows,0,sizeof(s_camera_rows));
    s_wifi_connect_action=s_camera_use_action=s_camera_scan_action=s_finish_action=NULL;
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
    /* Keep the profile editor's screen-local modal above its Setup launcher. */
    s_center=ui_popup_create_on_screen(width,height,UI_POPUP_STANDARD);
    if(!s_center)return;
    lv_obj_add_event_cb(s_center,center_deleted,LV_EVENT_DELETE,NULL);
    lv_obj_set_flex_flow(s_center,LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(s_center,20,0);
    lv_obj_set_style_pad_row(s_center,16,0);
    setup_text(s_center,"Setup center",UI_FONT_HEADING,UI_TEXT);
    lv_obj_t *body=setup_plane(s_center,LV_FLEX_FLOW_ROW);
    lv_obj_set_height(body,0);lv_obj_set_flex_grow(body,1);
    lv_obj_set_style_pad_column(body,20,0);
    lv_obj_t *nav=setup_plane(body,LV_FLEX_FLOW_COLUMN);
    lv_obj_set_width(nav,width<600?120:204);lv_obj_set_height(nav,LV_PCT(100));
    lv_obj_add_flag(nav,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_scroll_dir(nav,LV_DIR_VER);
    lv_obj_set_scrollbar_mode(nav,LV_SCROLLBAR_MODE_OFF);
    ui_apply_divider_border(nav,LV_BORDER_SIDE_RIGHT);lv_obj_set_style_pad_right(nav,12,0);
    const char *names[]={"Welcome","Network","Time zone","Display","Printer","Camera","Review"};
    lv_event_cb_t callbacks[]={setup_select_welcome_cb,setup_select_wifi_cb,setup_select_timezone_cb,setup_select_display_cb,setup_select_printer_cb,setup_select_camera_cb,setup_select_complete_cb};
    for(int i=0;i<SETUP_STEP_COUNT;i++){s_nav[i]=setup_action(nav,names[i],UI_POPUP_ACTION_SECONDARY,callbacks[i]);lv_obj_set_style_bg_opa(s_nav[i],LV_OPA_TRANSP,0);setup_watch(s_nav[i]);}
    /* Keep all seven steps visible; readiness is shown in Welcome and Review. */
    s_setup_content=setup_plane(body,LV_FLEX_FLOW_COLUMN);
    setup_watch(s_setup_content);
    lv_obj_set_width(s_setup_content,0);lv_obj_set_flex_grow(s_setup_content,1);
    lv_obj_set_height(s_setup_content,LV_PCT(100));
    setup_render_step(SETUP_STEP_WELCOME);
    lv_obj_t *footer=setup_plane(s_center,LV_FLEX_FLOW_ROW);
    ui_apply_divider_border(footer,LV_BORDER_SIDE_TOP);lv_obj_set_style_pad_top(footer,8,0);
    lv_obj_set_flex_align(footer,LV_FLEX_ALIGN_SPACE_BETWEEN,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
    s_back_action=setup_action(footer,"Back",UI_POPUP_ACTION_SECONDARY,setup_back_cb);
    setup_watch(s_back_action);lv_obj_set_width(s_back_action,width<600?96:120);
    lv_obj_add_state(s_back_action,LV_STATE_DISABLED);
    lv_obj_t *later=setup_action(footer,"Set up later",UI_POPUP_ACTION_CANCEL,later_cb);
    lv_obj_set_width(later,width<600?144:180);
    lv_obj_update_layout(s_center);
    for(int i=0;i<SETUP_STEP_COUNT;i++)if(s_nav[i])lv_obj_send_event(s_nav[i],LV_EVENT_SIZE_CHANGED,NULL);
    lv_obj_send_event(later,LV_EVENT_SIZE_CHANGED,NULL);
    lv_obj_send_event(s_back_action,LV_EVENT_SIZE_CHANGED,NULL);
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
    s_setup_dispatching=false;
    lv_obj_t *center=s_center;
    /* Retire all old ownership before any child DELETE callback can reopen. */
    if(center) {
        ++s_setup_epoch;
        s_center=s_setup_content=s_stage=s_actions=s_status=s_name=s_password=s_keyboard=NULL;
        s_wifi_list=s_camera_list=s_summary=s_options_list=s_brightness_slider=s_brightness_value=s_back_action=NULL;
        s_wifi_connect_action=s_camera_use_action=s_camera_scan_action=s_finish_action=NULL;
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

static lv_obj_t *setup_results_list(lv_obj_t *parent)
{
    lv_obj_t *list=setup_plane(parent,LV_FLEX_FLOW_COLUMN);
    lv_obj_set_height(list,0);lv_obj_set_style_min_height(list,48,0);
    lv_obj_set_flex_grow(list,1);lv_obj_add_flag(list,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(list,LV_DIR_VER);lv_obj_set_scrollbar_mode(list,LV_SCROLLBAR_MODE_OFF);
    return list;
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
        set_status("No networks found. Tap Scan again to retry.");
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
    s_wifi_list=setup_results_list(s_stage);
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
    set_status("Wi-Fi verified. Returning to Setup center...");
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
    set_status("Connecting and verifying Wi-Fi...");
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
        "Searching for Moonraker printers...",printer_editor_discover_closed,
        printer_editor_discover_selected,request))return;
    if(request!=ui_printer_profiles_discovery_request()){moonraker_discovery_close_request(request);return;}
    (void)moonraker_discovery_start(&ip.ip);
}

static void printer_full_options_cb(lv_event_t *event)
{
    (void)event;
    if(!s_center || !s_setup_content || s_setup_dispatching)return;
    int slot=moonraker_config_active_profile_index();
    if(slot<0 || slot>=MOONRAKER_CONFIG_MAX_PROFILES)slot=0;
    ui_printer_profiles_show_for_slot(slot,
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
    camera_actions(false);
    set_status("Printer changed. Scan again for the active printer.");
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
    s_camera_use_action=s_camera_scan_action=NULL;
    lv_obj_clean(s_actions);
    lv_obj_t *action=setup_action(s_actions,verified?"Use camera":"Scan again",
        verified?UI_POPUP_ACTION_CONFIRM:UI_POPUP_ACTION_SECONDARY,
        verified?camera_save_cb:center_camera_cb);
    if(verified)s_camera_use_action=action;else s_camera_scan_action=action;
    setup_watch(action);
    lv_obj_set_width(action,LV_PCT(48));lv_obj_set_flex_grow(action,1);
    const char *saved_url=moonraker_config_camera_stream_url(moonraker_config_active_profile_index());
    lv_obj_t *skip=setup_action(s_actions,saved_url && saved_url[0]?"Continue":"Skip camera",UI_POPUP_ACTION_SECONDARY,setup_skip_cb);
    lv_obj_set_width(skip,LV_PCT(48));lv_obj_set_flex_grow(skip,1);
    lv_obj_update_layout(s_center);setup_fit_actions(s_actions);
}

static void camera_scan_busy(const char *text)
{
    if(!s_camera_scan_action)return;
    lv_obj_add_state(s_camera_scan_action,LV_STATE_DISABLED);
    lv_obj_t *label=lv_obj_get_child(s_camera_scan_action,0);
    if(label)lv_label_set_text(label,text);
    lv_obj_send_event(s_camera_scan_action,LV_EVENT_SIZE_CHANGED,NULL);
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
        camera_actions(false);
        set_status("Camera test failed. Select this camera to retry, or choose another.");
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
    for(unsigned i=0;i<CAMERA_CATALOG_MAX_CAMERAS;i++)
        if(s_camera_rows[i])ui_button_set_filter_active(s_camera_rows[i],(int)i==s_selected_camera);
    char detail[240]; snprintf(detail, sizeof(detail), "Selected: %s. Testing its stream...", entry.name);
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
    camera_scan_busy("Testing...");
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
    camera_actions(false);
    const int profile = moonraker_config_active_profile_index();
    if (!found || !count) {
        set_status("A camera is optional. Scan again or continue setup.");
        if(s_camera_list) {
            setup_text(s_camera_list,"No cameras found",UI_FONT_BODY_LARGE,UI_TEXT);
            setup_text(s_camera_list,"Enable a webcam in your printer's interface, then scan again.",UI_FONT_BODY,UI_TEXT_DIM);
        }
        return;
    }
    set_status(ui_text("Select a camera to test its stream."));
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
    if(!s_center || !s_setup_content || s_setup_dispatching || s_poll)return;
    delete_poll();
    s_camera_owner = moonraker_config_generation();
    s_selected_camera = -1;
    if(!setup_begin_content("Printer cameras","Searching this printer for configured cameras..."))return;
    s_camera_list=setup_results_list(s_stage);
    setup_watch(s_camera_list);
    camera_actions(false);
    const int profile = moonraker_config_active_profile_index();
    const moonraker_profile_t *printer = moonraker_config_profile(profile);
    if (!printer || !printer->configured) {
        set_status("Save a printer profile first, then return here.");
        return;
    }
    moonraker_profile_t selected=*printer;
    uint32_t epoch=s_setup_epoch,owner=s_camera_owner;
    s_setup_dispatching=true;
    bool started=camera_discovery_start(selected.host,selected.port,selected.api_key);
    s_setup_dispatching=false;
    if(!setup_current(epoch) || owner!=moonraker_config_generation())return;
    if (!started) {
        set_status("Could not start camera discovery. Wait and tap Scan again.");
        return;
    }
    s_poll = lv_timer_create(camera_discovery_poll, 150, NULL);
    camera_scan_busy("Scanning...");
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
    int height=lv_display_get_vertical_resolution(NULL)-32;
    if(height>300)height=300;
    s_complete_popup=ui_popup_create(lv_screen_active(),width,height,UI_POPUP_STANDARD);
    if(!s_complete_popup)return;
    lv_obj_add_event_cb(s_complete_popup,completion_deleted,LV_EVENT_DELETE,NULL);
    lv_obj_set_flex_flow(s_complete_popup,LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(s_complete_popup,UI_PAD_POPUP,0);
    lv_obj_set_style_pad_row(s_complete_popup,UI_GAP_ROW,0);
    setup_text(s_complete_popup,"Setup complete",UI_FONT_TITLE,UI_TEXT_BRIGHT);
    ui_form_divide_title(s_complete_popup);
    lv_obj_t *body=setup_plane(s_complete_popup,LV_FLEX_FLOW_COLUMN);
    lv_obj_set_height(body,0);lv_obj_set_flex_grow(body,1);lv_obj_add_flag(body,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *saved=setup_text(body,"Your saved choices are available in Settings. Reopen Setup center whenever you need it.",UI_FONT_BODY,UI_TEXT);
    lv_obj_set_style_text_line_space(saved,4,0);
    lv_obj_t *next=setup_text(body,"Check the Printer page for live connection status. Camera setup remains optional.",UI_FONT_CAPTION,UI_TEXT_DIM);
    lv_obj_set_style_text_line_space(next,4,0);
    lv_obj_set_scroll_dir(body,LV_DIR_VER);lv_obj_set_scrollbar_mode(body,LV_SCROLLBAR_MODE_OFF);
    lv_obj_t *footer=setup_plane(s_complete_popup,LV_FLEX_FLOW_ROW);
    ui_apply_divider_border(footer,LV_BORDER_SIDE_TOP);lv_obj_set_style_pad_top(footer,8,0);
    lv_obj_set_flex_align(footer,LV_FLEX_ALIGN_END,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
    lv_obj_set_width(setup_action(footer,"Done",UI_POPUP_ACTION_CONFIRM,setup_completion_done_cb),160);
    lv_obj_update_layout(s_complete_popup);setup_fit_actions(footer);
    lv_obj_move_foreground(s_complete_popup);
}


static void finish_cb(lv_event_t *event)
{
    if(!setup_action_current(event,s_finish_action,false))return;
    uint32_t epoch=s_setup_epoch;
    setup_refresh_completion_state();
    if(!setup_current(epoch))return;
    if (!s_wifi_done || !s_printer_done) {
        set_status("Connect Wi-Fi and save a printer profile before finishing setup.");
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



void ui_setup_wizard_show_with_appearance_callback(
    ui_setup_wizard_wifi_connect_cb_t wifi_connect_cb,void (*appearance_changed)(void))
{
    if(s_center){lv_obj_move_foreground(s_center);return;}
    if(s_complete_popup){lv_obj_t *popup=s_complete_popup;s_complete_popup=NULL;lv_obj_delete(popup);if(s_center || s_complete_popup)return;}
    setup_refresh_completion_state();
    s_wifi_connect = wifi_connect_cb;
    s_appearance_changed=appearance_changed;
    show_center();
}

void ui_setup_wizard_show(ui_setup_wizard_wifi_connect_cb_t wifi_connect_cb)
{ ui_setup_wizard_show_with_appearance_callback(wifi_connect_cb,NULL); }
