#include "ui_printer_profiles.h"
#include "ui_text.h"
#include "ui_value_update.h"

#include <stdint.h>
#include <dirent.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "moonraker_config_controller.h"
#include "moonraker_transport_security_controller.h"
#include "moonraker_endpoint_test.h"
#include "camera_discovery_controller.h"
#include "camera_catalog_controller.h"
#include "camera_test_controller.h"
#include "ui_camera.h"
#include "ui_dashboard.h"
#include "printer_preview_cache.h"
#include "printer_preview_store.h"
#include "printer_profile_health.h"
#include "ui_popup.h"
#include "ui_form_dialog.h"
#include "ui_button.h"
#include "ui_widgets.h"
#include "ui_theme.h"
#include "ui_printer_chooser.h"

static lv_obj_t *s_manager_popup = NULL;
static lv_obj_t *s_delete_popup = NULL;
static lv_obj_t *s_manager_list = NULL;
static lv_obj_t *s_manager_status = NULL;
static lv_obj_t *s_profile_rows[
    MOONRAKER_CONFIG_MAX_PROFILES];

static lv_obj_t *s_editor_popup = NULL;
static lv_obj_t *s_editor_name = NULL;
static lv_obj_t *s_editor_host = NULL;
static lv_obj_t *s_editor_port = NULL;
static lv_obj_t *s_editor_auth_popup = NULL;
static lv_obj_t *s_editor_auth_key = NULL;
static lv_obj_t *s_editor_auth_keyboard = NULL;
static lv_obj_t *s_editor_keyboard = NULL;
static lv_obj_t *s_editor_keyboard_popup = NULL;
static lv_obj_t *s_editor_keyboard_value = NULL;
static lv_obj_t *s_editor_keyboard_target = NULL;
static char s_editor_api_key[MOONRAKER_CONFIG_API_KEY_LENGTH];
static lv_obj_t *s_editor_camera_popup = NULL;
static lv_obj_t *s_editor_camera_remove_popup = NULL;
static lv_obj_t *s_camera_remove_action,*s_camera_remove_apply,*s_camera_remove_cancel,*s_camera_remove_status;
static int s_camera_remove_profile=-1;
static size_t s_camera_remove_slot;
static uint32_t s_camera_remove_generation;
static void editor_camera_remove_close(void);
static lv_obj_t *s_editor_camera_stream = NULL;
static lv_obj_t *s_editor_camera_name = NULL;
static char s_editor_camera_names[CAMERA_CATALOG_MAX_CAMERAS][CAMERA_CATALOG_NAME_LENGTH];
static lv_obj_t *s_editor_camera_keyboard = NULL;
static char s_editor_camera_url[MOONRAKER_CONFIG_CAMERA_URL_LENGTH];
static lv_obj_t *s_editor_camera_status = NULL;
static lv_timer_t *s_editor_camera_discovery_timer = NULL;
static lv_obj_t *s_camera_save,*s_camera_default,*s_camera_find,*s_camera_test,*s_camera_cancel;
typedef struct {
    lv_obj_t *popup,*editor;
    uint32_t epoch,generation;
    int profile,active;
    size_t slot;
} camera_context_t;
typedef struct {
    camera_context_t context;
    char name[CAMERA_CATALOG_NAME_LENGTH];
    char url[MOONRAKER_CONFIG_CAMERA_URL_LENGTH];
} camera_draft_t;
typedef struct {
    camera_draft_t draft;
    char host[MOONRAKER_CONFIG_HOST_LENGTH],port[6],key[MOONRAKER_CONFIG_API_KEY_LENGTH];
    bool secure;
} camera_job_t;
static camera_job_t s_camera_test_job,s_camera_find_job;
static lv_timer_t *s_editor_camera_test_timer = NULL;
static size_t s_editor_camera_slot = 0;
static lv_obj_t *s_editor_camera_slot_buttons[CAMERA_CATALOG_MAX_CAMERAS];
static lv_obj_t *s_editor_status = NULL;
static lv_timer_t *s_editor_test_timer = NULL;
static uint32_t s_profile_epoch;
static bool s_profile_dispatching;
static lv_obj_t *s_editor_test_action,*s_editor_auth_use,*s_editor_key_done;
static lv_obj_t *s_editor_save_action,*s_manager_select_action,*s_manager_remove_action;
static lv_obj_t *s_manager_edit_action,*s_manager_close_action;
static lv_obj_t *s_editor_cancel_action,*s_editor_discover_action,*s_editor_auth_action,*s_editor_camera_action;
static lv_obj_t *s_auth_cancel_action,*s_key_cancel_action;
static bool s_profile_discovery_dispatching;
typedef struct {
    lv_obj_t *editor;
    uint32_t request,epoch,generation;
    int profile,active;
    char host[MOONRAKER_CONFIG_HOST_LENGTH],port[6],name[MOONRAKER_CONFIG_NAME_LENGTH];
} profile_discovery_request_t;
static profile_discovery_request_t s_profile_discovery_request;
static uint32_t s_profile_discovery_serial;
static lv_obj_t *s_delete_apply,*s_delete_status;
static int s_delete_profile=-1;
static uint32_t s_delete_generation;
typedef struct {
    lv_obj_t *popup;
    uint32_t epoch,generation;
    int profile,port;
    bool secure;
    char host[MOONRAKER_CONFIG_HOST_LENGTH];
    char port_text[6];
    char key[MOONRAKER_CONFIG_API_KEY_LENGTH];
} editor_test_draft_t;
static editor_test_draft_t s_test_draft;

static int s_selected_profile = 0;
static int s_editor_profile = -1;
static bool s_editor_secure = false;
static bool s_camera_standalone, s_camera_setup_opening;
static char s_camera_setup_host[MOONRAKER_CONFIG_HOST_LENGTH], s_camera_setup_port[6];
static lv_obj_t *s_editor_security_popup = NULL;
#define EDITOR_PEM_MAX_FILES 6
static lv_obj_t *s_editor_pem_picker = NULL;
static char s_editor_pem_paths[EDITOR_PEM_MAX_FILES][128];
static lv_obj_t *s_security_opener,*s_security_standard,*s_security_select,*s_security_cancel,*s_security_status;
static lv_obj_t *s_pem_close,*s_pem_status,*s_pem_body,*s_pem_rows[EDITOR_PEM_MAX_FILES];
static lv_obj_t *s_security_editor;
static int s_security_profile=-1,s_security_active=-1;
static uint32_t s_security_generation;
static void editor_security_retire(void);
static void editor_pem_picker_close(void);

static ui_printer_profiles_active_changed_cb_t
    s_active_changed_cb = NULL;

static ui_printer_profiles_discover_cb_t
    s_discover_cb = NULL;

static void editor_security_close(void);
static void editor_set_status(const char *text);
static void editor_camera_set_status(const char *text);
static void editor_auth_open_cb(lv_event_t *event);
static void editor_camera_open_cb(lv_event_t *event);
static void editor_camera_discover_cb(lv_event_t *event);
static void editor_camera_close(void);
static void editor_auth_close(void);
static void profiles_dialog_deleted(lv_event_t *event);
static void editor_keyboard_discard(void);
static void delete_close(void);

static void profiles_clear_secret(void *data,size_t length)
{
    volatile unsigned char *bytes=data;
    while(length--)*bytes++=0;
}
static void editor_test_retire(void)
{
    lv_timer_t *timer=s_editor_test_timer;s_editor_test_timer=NULL;
    profiles_clear_secret(&s_test_draft,sizeof(s_test_draft));
    if(timer)lv_timer_delete(timer);
}
static void camera_job_retire(bool find)
{
    lv_timer_t **owner=find?&s_editor_camera_discovery_timer:&s_editor_camera_test_timer;
    lv_timer_t *timer=*owner;*owner=NULL;
    profiles_clear_secret(find?&s_camera_find_job:&s_camera_test_job,sizeof(camera_job_t));
    if(timer)lv_timer_delete(timer);
}
static void camera_jobs_retire(void){camera_job_retire(false);camera_job_retire(true);}
static void profiles_field_deleted(lv_event_t *event)
{
    lv_obj_t *object=lv_event_get_target_obj(event);
    lv_obj_t **fields[]={&s_manager_list,&s_manager_status,&s_editor_name,&s_editor_host,&s_editor_port,
        &s_editor_status,&s_editor_auth_key,&s_editor_auth_keyboard,&s_editor_keyboard,
        &s_editor_keyboard_value,&s_editor_keyboard_target,&s_editor_camera_stream,&s_editor_camera_name,
        &s_editor_camera_status,&s_editor_camera_keyboard,&s_editor_test_action,&s_editor_auth_use,&s_editor_key_done,
        &s_editor_save_action,&s_manager_select_action,&s_manager_remove_action,&s_delete_apply,&s_delete_status,
        &s_camera_remove_action,&s_camera_remove_apply,&s_camera_remove_cancel,&s_camera_remove_status,
        &s_camera_save,&s_camera_default,&s_camera_find,&s_camera_test,&s_camera_cancel,
        &s_security_opener,&s_security_standard,&s_security_select,&s_security_cancel,&s_security_status,
        &s_pem_close,&s_pem_status,&s_pem_body,&s_manager_edit_action,&s_manager_close_action,
        &s_editor_cancel_action,&s_editor_discover_action,&s_editor_auth_action,&s_editor_camera_action,
        &s_auth_cancel_action,&s_key_cancel_action};
    bool retired=false;
    for(unsigned i=0;i<sizeof(fields)/sizeof(fields[0]);++i)
        if(*fields[i]==object){*fields[i]=NULL;retired=true;}
    for(int i=0;i<MOONRAKER_CONFIG_MAX_PROFILES;++i)if(s_profile_rows[i]==object){
        s_profile_rows[i]=NULL;if(s_selected_profile==i)s_selected_profile=-1;retired=true;
    }
    for(size_t i=0;i<CAMERA_CATALOG_MAX_CAMERAS;++i)if(s_editor_camera_slot_buttons[i]==object){
        s_editor_camera_slot_buttons[i]=NULL;retired=true;
    }
    for(size_t i=0;i<EDITOR_PEM_MAX_FILES;++i)if(s_pem_rows[i]==object){
        s_pem_rows[i]=NULL;s_editor_pem_paths[i][0]='\0';retired=true;
    }
    if(retired){++s_profile_epoch;camera_jobs_retire();}
}
static void profiles_watch(lv_obj_t *object)
{
    if(object)lv_obj_add_event_cb(object,profiles_field_deleted,LV_EVENT_DELETE,NULL);
}
static bool profiles_action_current(lv_event_t *event,lv_obj_t *button,bool keyboard)
{
    if(s_profile_dispatching || s_profile_discovery_dispatching || (!s_editor_popup && !(s_camera_standalone && s_editor_camera_popup)) || !button || lv_obj_has_state(button,LV_STATE_DISABLED))return false;
    if(!event)return true;
    lv_obj_t *target=lv_event_get_target_obj(event);
    return target==lv_event_get_current_target_obj(event) &&
        ((target==button && lv_event_get_code(event)==LV_EVENT_CLICKED) ||
         (keyboard && target==s_editor_keyboard && !lv_obj_has_state(target,LV_STATE_DISABLED) &&
            lv_event_get_code(event)==LV_EVENT_READY));
}
static bool manager_action_current(lv_event_t *event,lv_obj_t *button)
{
    if(s_profile_dispatching || s_profile_discovery_dispatching || !s_manager_popup || !button || lv_obj_has_state(button,LV_STATE_DISABLED))return false;
    return !event || (lv_event_get_code(event)==LV_EVENT_CLICKED &&
        lv_event_get_target_obj(event)==button && lv_event_get_current_target_obj(event)==button);
}

static int32_t profiles_width(int32_t preferred)
{
    int32_t available = lv_display_get_horizontal_resolution(NULL) - (ui_theme_is_studio() ? 48 : 32);
    if(ui_theme_is_studio() && preferred >= 700)return available;
    return available < preferred ? available : preferred;
}
static int32_t profiles_height(int32_t preferred)
{
    int32_t available = lv_display_get_vertical_resolution(NULL) - 32;
    return available < preferred ? available : preferred;
}
static lv_obj_t *profiles_label(lv_obj_t *parent, const char *text, bool caption)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text ? text : "");
    lv_obj_set_width(label, LV_PCT(100));
    profiles_watch(label);
    ui_apply_custom_label_style(label, caption ? UI_FONT_CAPTION : UI_FONT_BODY,
        caption ? UI_TEXT_MUTED : UI_TEXT);
    return label;
}
static lv_obj_t *profiles_body(lv_obj_t *popup, const char *title)
{
    lv_obj_set_flex_flow(popup, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(popup, UI_PAD_POPUP, 0);
    lv_obj_set_style_pad_row(popup, UI_GAP_CARD, 0);
    lv_obj_t *heading = profiles_label(popup, title, false);
    ui_apply_custom_label_style(heading, UI_FONT_TITLE, UI_TEXT_BRIGHT);
    lv_obj_t *body = lv_obj_create(popup);
    lv_obj_remove_style_all(body);
    lv_obj_set_size(body, LV_PCT(100), 0);
    lv_obj_set_flex_grow(body, 1);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(body, UI_GAP_CARD, 0);
    lv_obj_add_flag(body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(body, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(body, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(body, UI_TEXT_MUTED, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(body, LV_OPA_70, LV_PART_SCROLLBAR);
    lv_obj_set_style_width(body, 4, LV_PART_SCROLLBAR);
    return body;
}
/* Profile detail forms share divided headings and compact row spacing. */
static lv_obj_t *profiles_detail_body(lv_obj_t *popup, const char *title)
{
    lv_obj_t *body = profiles_body(popup, title);
    ui_form_divide_title(popup);
    lv_obj_set_style_pad_row(popup, UI_GAP_ROW, 0);
    lv_obj_set_style_pad_row(body, UI_GAP_ROW, 0);
    return body;
}
static lv_obj_t *profiles_footer(lv_obj_t *popup)
{
    lv_obj_t *footer = lv_obj_create(popup);
    lv_obj_remove_style_all(footer);
    lv_obj_set_size(footer, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(footer, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_column(footer, UI_GAP_ROW, 0);
    lv_obj_set_style_pad_row(footer, UI_GAP_ROW, 0);
    if (ui_popup_find_owner(popup) == popup) {
        ui_apply_divider_border(footer, LV_BORDER_SIDE_TOP);
        lv_obj_set_style_pad_top(footer, 8, 0);
    }
    return footer;
}
static lv_obj_t *profiles_action_data(lv_obj_t *parent, ui_popup_action_t kind,
                                 const char *text, lv_event_cb_t callback, void *user)
{
    lv_obj_t *button = ui_popup_add_action_at(parent, kind, text, 0, 0,
        LV_SIZE_CONTENT, 48, callback, user, NULL);
    if (button) {
        profiles_watch(button);
        lv_obj_set_style_pad_hor(button, 12, 0);
        if (lv_obj_get_style_flex_flow(parent, 0) == LV_FLEX_FLOW_ROW_WRAP)
            lv_obj_set_flex_grow(button, 1);
        lv_obj_t *label = lv_obj_get_child(button, 0);
        lv_point_t size;
        lv_text_get_size(&size, text, lv_obj_get_style_text_font(label, 0),
            lv_obj_get_style_text_letter_space(label, 0), 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        lv_obj_set_style_min_width(button, size.x + 28, 0);
    }
    return button;
}
static lv_obj_t *profiles_action(lv_obj_t *parent, ui_popup_action_t kind,
                                 const char *text, lv_event_cb_t callback)
{
    return profiles_action_data(parent, kind, text, callback, NULL);
}
static lv_obj_t *profiles_form_group(lv_obj_t *parent)
{
    lv_obj_t *group=lv_obj_create(parent);
    lv_obj_remove_style_all(group);
    lv_obj_set_size(group,LV_PCT(100),LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(group,LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(group,6,0);
    lv_obj_clear_flag(group,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);
    return group;
}
static lv_obj_t *profiles_field(lv_obj_t *parent, const char *caption,
                                const char *value, uint32_t max, const char *accepted)
{
    profiles_label(parent, caption, true);
    lv_obj_t *field = ui_popup_add_textarea(parent, 1, 56, LV_ALIGN_TOP_LEFT,
        0, 0, true, false, max, "", value, accepted);
    lv_obj_set_width(field, LV_PCT(100));
    profiles_watch(field);
    lv_obj_set_style_text_font(field, UI_FONT_BODY_LARGE, 0);
    lv_obj_set_style_min_height(field, 56, 0);
    lv_obj_set_style_pad_ver(field, (56 - UI_FONT_BODY_LARGE->line_height - 2) / 2, 0);
    return field;
}

static void editor_close(void)
{
    lv_obj_t *popups[]={s_editor_keyboard_popup,s_editor_auth_popup,s_editor_camera_remove_popup,
        s_editor_camera_popup,s_editor_pem_picker,s_editor_security_popup,s_editor_popup};
    lv_timer_t *camera_discovery=s_editor_camera_discovery_timer,*camera_test=s_editor_camera_test_timer;
    ++s_profile_epoch;editor_test_retire();
    memset(&s_profile_discovery_request,0,sizeof(s_profile_discovery_request));
    s_editor_popup=s_editor_name=s_editor_host=s_editor_port=s_editor_status=NULL;
    s_editor_keyboard_popup=s_editor_keyboard=s_editor_keyboard_value=s_editor_keyboard_target=NULL;
    s_editor_auth_popup=s_editor_auth_key=s_editor_auth_keyboard=s_auth_cancel_action=NULL;
    s_editor_cancel_action=s_editor_discover_action=s_editor_auth_action=s_editor_camera_action=s_key_cancel_action=NULL;
    s_editor_camera_remove_popup=s_editor_camera_popup=s_editor_camera_stream=s_editor_camera_name=NULL;
    s_editor_camera_keyboard=s_editor_camera_status=NULL;
    s_camera_remove_action=s_camera_remove_apply=s_camera_remove_cancel=s_camera_remove_status=NULL;
    s_camera_remove_profile=-1;
    editor_security_retire();s_security_opener=NULL;
    s_editor_test_action=s_editor_auth_use=s_editor_key_done=s_editor_save_action=NULL;
    s_camera_save=s_camera_default=s_camera_find=s_camera_test=s_camera_cancel=NULL;
    profiles_clear_secret(&s_camera_find_job,sizeof(s_camera_find_job));
    profiles_clear_secret(&s_camera_test_job,sizeof(s_camera_test_job));
    s_editor_camera_discovery_timer=s_editor_camera_test_timer=NULL;
    memset(s_editor_camera_slot_buttons,0,sizeof(s_editor_camera_slot_buttons));
    profiles_clear_secret(s_editor_api_key,sizeof(s_editor_api_key));
    s_editor_profile=-1;s_editor_secure=false;
    s_camera_standalone=false;
    profiles_clear_secret(s_camera_setup_host,sizeof(s_camera_setup_host));
    profiles_clear_secret(s_camera_setup_port,sizeof(s_camera_setup_port));
    if(camera_discovery)lv_timer_delete(camera_discovery);
    if(camera_test)lv_timer_delete(camera_test);
    ui_camera_set_setup_active(false);
    for(unsigned i=0;i<sizeof(popups)/sizeof(popups[0]);++i)if(popups[i])lv_obj_delete(popups[i]);
}

void ui_printer_profiles_close_all(void)
{
    lv_obj_t *manager=s_manager_popup,*confirmation=s_delete_popup;
    s_delete_popup=s_manager_popup=s_manager_list=s_manager_status=NULL;
    s_manager_select_action=s_manager_remove_action=s_delete_apply=s_delete_status=NULL;
    s_manager_edit_action=s_manager_close_action=NULL;
    s_delete_profile=-1;
    memset(s_profile_rows,0,sizeof(s_profile_rows));
    s_active_changed_cb=NULL;s_discover_cb=NULL;
    editor_close();
    if(confirmation)lv_obj_delete(confirmation);
    if(manager)lv_obj_delete(manager);
}


static void profiles_dialog_deleted(lv_event_t *event)
{
    lv_obj_t *popup = lv_event_get_target_obj(event);
    if (popup == s_manager_popup) {
        s_manager_popup = NULL;
        ui_printer_profiles_close_all();
    } else if (popup == s_editor_popup) {
        s_editor_popup = NULL;
        editor_close();
    } else if (popup == s_delete_popup) {s_delete_popup=NULL;delete_close();}
    else if (popup == s_editor_keyboard_popup) {
        ++s_profile_epoch;
        s_editor_keyboard_popup = NULL;
        s_editor_keyboard = NULL;
        s_editor_keyboard_value = NULL;
        s_editor_keyboard_target = NULL;
        s_editor_key_done=s_key_cancel_action=NULL;
    } else if (popup == s_editor_auth_popup) {
        s_editor_auth_popup = NULL;
        editor_auth_close();
    } else if (popup == s_editor_camera_popup) {
        s_editor_camera_popup = NULL;
        editor_camera_close();
    } else if (popup == s_editor_security_popup) {
        s_editor_security_popup = NULL;
        editor_security_close();
    } else if (popup == s_editor_pem_picker) {
        s_editor_pem_picker=NULL;editor_pem_picker_close();
    }
    else if (popup == s_editor_camera_remove_popup) {
        s_editor_camera_remove_popup=NULL;editor_camera_remove_close();
    }
}

static void manager_close_cb(lv_event_t *event)
{
    if(!manager_action_current(event,s_manager_close_action))return;
    ui_printer_profiles_close_all();
}

static void editor_cancel_cb(lv_event_t *event)
{
    if(!profiles_action_current(event,s_editor_cancel_action,false))return;
    editor_close();
}

static void editor_pem_picker_retire(void)
{
    s_editor_pem_picker=s_pem_close=s_pem_status=s_pem_body=NULL;
    memset(s_pem_rows,0,sizeof(s_pem_rows));
    memset(s_editor_pem_paths,0,sizeof(s_editor_pem_paths));
}
static void editor_security_retire(void)
{
    s_editor_security_popup=s_security_editor=NULL;
    s_security_standard=s_security_select=s_security_cancel=s_security_status=NULL;
    s_security_profile=s_security_active=-1;
    editor_pem_picker_retire();
}
static void editor_pem_picker_close(void)
{
    lv_obj_t *popup=s_editor_pem_picker;
    ++s_profile_epoch;editor_pem_picker_retire();
    if(popup)lv_obj_delete(popup);
}
static void editor_pem_picker_close_cb(lv_event_t *event)
{
    if(!s_editor_pem_picker || !profiles_action_current(event,s_pem_close,false))return;
    editor_pem_picker_close();
}
static void editor_security_close(void)
{
    lv_obj_t *popup=s_editor_security_popup,*picker=s_editor_pem_picker;
    ++s_profile_epoch;editor_security_retire();
    if(picker)lv_obj_delete(picker);
    if(popup)lv_obj_delete(popup);
}
static bool editor_security_current(void)
{
    return s_editor_security_popup && s_security_editor==s_editor_popup && s_security_profile>=0 &&
        s_security_profile==s_editor_profile && s_security_active==moonraker_config_active_profile_index() &&
        s_security_generation==moonraker_config_generation();
}
static bool editor_security_action(lv_event_t *event,lv_obj_t *button)
{
    if(!s_editor_security_popup || !profiles_action_current(event,button,false))return false;
    if(editor_security_current())return true;
    if(s_security_status)lv_label_set_text(s_security_status,"Printer configuration changed. Close and reopen Security.");
    return false;
}
static void editor_security_standard_cb(lv_event_t *event)
{
    if(!editor_security_action(event,s_security_standard) || s_editor_pem_picker)return;
    lv_obj_t *editor=s_editor_popup;int profile=s_editor_profile;uint32_t epoch=s_profile_epoch;
    s_profile_dispatching=true;editor_test_retire();camera_jobs_retire();
    s_editor_secure=false;
    editor_security_close();
    if(editor==s_editor_popup && profile==s_editor_profile && s_profile_epoch==epoch+1)
        editor_set_status("Standard HTTP selected. Save the profile to apply this draft.");
    s_profile_dispatching=false;
}
static bool editor_pem_name(const char *name)
{
    size_t length=name?strlen(name):0;
    return length>4 && length+strlen("/sdcard/")<sizeof(s_editor_pem_paths[0]) &&
        !strchr(name,'/') && !strchr(name,'\\') && strcasecmp(name+length-4,".pem")==0;
}
static void editor_security_pem_selected_cb(lv_event_t *event)
{
    if(!event || !s_editor_pem_picker)return;
    size_t row=(size_t)(uintptr_t)lv_event_get_user_data(event);
    if(row>=EDITOR_PEM_MAX_FILES || !profiles_action_current(event,s_pem_rows[row],false))return;
    if(!editor_security_current()){
        if(s_pem_status)lv_label_set_text(s_pem_status,"Printer configuration changed. Close and reopen Security.");
        return;
    }
    if(!s_editor_port || !s_editor_pem_paths[row][0]){
        if(s_pem_status)lv_label_set_text(s_pem_status,"Connection fields are unavailable. Reopen the profile editor.");
        return;
    }
    char path[sizeof(s_editor_pem_paths[0])],port[6];
    strlcpy(path,s_editor_pem_paths[row],sizeof(path));strlcpy(port,lv_textarea_get_text(s_editor_port),sizeof(port));
    lv_obj_t *editor=s_editor_popup,*security=s_editor_security_popup,*picker=s_editor_pem_picker;
    int profile=s_editor_profile;uint32_t epoch=s_profile_epoch;
    s_profile_dispatching=true;editor_test_retire();camera_jobs_retire();
    bool imported=moonraker_transport_security_import_ca_file_for_profile(profile,path);
    if(epoch!=s_profile_epoch || editor!=s_editor_popup || security!=s_editor_security_popup ||
        picker!=s_editor_pem_picker || !editor_security_current()){
        s_profile_dispatching=false;return;
    }
    if(!imported){
        if(s_pem_status)lv_label_set_text(s_pem_status,"CA import failed. Check the PEM file and SD card, then select it again.");
        s_profile_dispatching=false;return;
    }
    s_editor_secure=true;
    if(s_editor_port && !strcmp(port,"7125") && !strcmp(lv_textarea_get_text(s_editor_port),port))
        lv_textarea_set_text(s_editor_port,"443");
    if(epoch==s_profile_epoch && editor==s_editor_popup && security==s_editor_security_popup &&
        picker==s_editor_pem_picker && editor_security_current()){
        editor_security_close();
        if(editor==s_editor_popup && profile==s_editor_profile && s_profile_epoch==epoch+1)
            editor_set_status("CA imported. Secure draft selected; check the TLS port (443-446) and save the profile.");
    }
    s_profile_dispatching=false;
}
static bool editor_pem_scan_current(lv_obj_t *popup,lv_obj_t *body,uint32_t epoch)
{
    return popup==s_editor_pem_picker && body==s_pem_body && epoch==s_profile_epoch && editor_security_current();
}
static void editor_security_pem_picker_open_cb(lv_event_t *event)
{
    if(!editor_security_action(event,s_security_select))return;
    if(s_editor_pem_picker){lv_obj_move_foreground(s_editor_pem_picker);return;}
    s_editor_pem_picker=ui_popup_create(lv_layer_top(),profiles_width(700),profiles_height(500),UI_POPUP_STANDARD);
    if(!s_editor_pem_picker)return;
    lv_obj_add_event_cb(s_editor_pem_picker,profiles_dialog_deleted,LV_EVENT_DELETE,NULL);
    lv_obj_t *popup=s_editor_pem_picker;
    s_pem_body=profiles_detail_body(popup,"Select CA certificate");profiles_watch(s_pem_body);
    lv_obj_t *body=s_pem_body;
    s_pem_status=profiles_label(body,"SD-card root .pem files only",true);
    lv_obj_set_style_text_line_space(s_pem_status,4,0);
    s_pem_close=profiles_action(profiles_footer(popup),UI_POPUP_ACTION_CLOSE,"Close",editor_pem_picker_close_cb);
    uint32_t epoch=s_profile_epoch;
    s_profile_dispatching=true;
    DIR *directory=opendir("/sdcard");
    size_t count=0;
    while(directory && count<EDITOR_PEM_MAX_FILES && editor_pem_scan_current(popup,body,epoch)){
        struct dirent *entry=readdir(directory);
        if(!editor_pem_scan_current(popup,body,epoch) || !entry)break;
        if(!editor_pem_name(entry->d_name))continue;
        snprintf(s_editor_pem_paths[count],sizeof(s_editor_pem_paths[count]),"/sdcard/%.*s",
            (int)(sizeof(s_editor_pem_paths[0])-sizeof("/sdcard/")),entry->d_name);
        lv_obj_t *button=ui_popup_add_selectable_row(body,entry->d_name,0,0,1,56,
            editor_security_pem_selected_cb,(void *)(uintptr_t)count);
        s_pem_rows[count]=button;profiles_watch(button);
        lv_obj_set_size(button,LV_PCT(100),LV_SIZE_CONTENT);
        lv_obj_set_style_min_height(button,56,0);
        lv_obj_set_style_pad_hor(button,UI_PAD_CARD,0);
        lv_obj_set_style_pad_ver(button,10,0);
        lv_obj_set_flex_flow(button,LV_FLEX_FLOW_COLUMN);
        lv_obj_t *label=lv_obj_get_child(button,0);lv_obj_set_width(label,LV_PCT(100));
        lv_label_set_long_mode(label,LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_align(label,LV_TEXT_ALIGN_LEFT,0);
        lv_obj_set_style_text_line_space(label,4,0);++count;
    }
    if(directory)closedir(directory);
    if(editor_pem_scan_current(popup,body,epoch)){
        if(count==0)profiles_label(body,"No .pem files found in SD-card root.",false);
    }else if(popup==s_editor_pem_picker && s_pem_status)
        lv_label_set_text(s_pem_status,"Certificate listing interrupted. Close and reopen Security.");
    s_profile_dispatching=false;
}
static void editor_security_cancel_cb(lv_event_t *event)
{
    if(!s_editor_security_popup || !profiles_action_current(event,s_security_cancel,false))return;
    editor_security_close();
}
static void editor_security_open_cb(lv_event_t *event)
{
    if(!profiles_action_current(event,s_security_opener,false))return;
    if(s_editor_security_popup){lv_obj_move_foreground(s_editor_security_popup);return;}
    s_editor_security_popup=ui_popup_create(lv_layer_top(),profiles_width(700),profiles_height(400),UI_POPUP_STANDARD);
    if(!s_editor_security_popup)return;
    s_security_editor=s_editor_popup;s_security_profile=s_editor_profile;
    s_security_active=moonraker_config_active_profile_index();s_security_generation=moonraker_config_generation();
    lv_obj_add_event_cb(s_editor_security_popup,profiles_dialog_deleted,LV_EVENT_DELETE,NULL);
    lv_obj_t *body=profiles_detail_body(s_editor_security_popup,"Connection security");
    s_security_status=profiles_label(body,
        s_editor_secure
            ? "This draft uses HTTPS/WSS. Choose a trusted PEM certificate or switch to standard HTTP."
            : "This draft uses standard HTTP. Choose a trusted PEM certificate to enable HTTPS/WSS.",false);
    lv_obj_set_style_text_line_space(s_security_status,4,0);
    lv_obj_t *footer=profiles_footer(s_editor_security_popup);
    s_security_cancel=profiles_action(footer,UI_POPUP_ACTION_CANCEL,"Cancel",editor_security_cancel_cb);
    s_security_standard=profiles_action(footer,UI_POPUP_ACTION_SECONDARY,"Standard HTTP",editor_security_standard_cb);
    s_security_select=profiles_action(footer,UI_POPUP_ACTION_CONFIRM,"Choose .pem",editor_security_pem_picker_open_cb);
}

static bool profile_discovery_request_current(void)
{
    const profile_discovery_request_t *request=&s_profile_discovery_request;
    return request->request && request->editor==s_editor_popup && request->epoch==s_profile_epoch &&
        request->profile==s_editor_profile && request->active==moonraker_config_active_profile_index() &&
        request->generation==moonraker_config_generation() && s_editor_host && s_editor_port && s_editor_name &&
        !strcmp(request->host,lv_textarea_get_text(s_editor_host)) &&
        !strcmp(request->port,lv_textarea_get_text(s_editor_port)) &&
        !strcmp(request->name,lv_textarea_get_text(s_editor_name));
}
uint32_t ui_printer_profiles_discovery_request(void)
{
    if(profile_discovery_request_current())return s_profile_discovery_request.request;
    memset(&s_profile_discovery_request,0,sizeof(s_profile_discovery_request));return 0;
}
void ui_printer_profiles_cancel_discovery_request(uint32_t request)
{
    if(request && request==s_profile_discovery_request.request)
        memset(&s_profile_discovery_request,0,sizeof(s_profile_discovery_request));
}
static void editor_discover_cb(lv_event_t *event)
{
    if(!profiles_action_current(event,s_editor_discover_action,false) || s_profile_discovery_dispatching)return;
    ui_printer_profiles_discover_cb_t callback=s_discover_cb;
    if(!callback || !s_editor_host || !s_editor_port || !s_editor_name)return;
    if(++s_profile_discovery_serial==0)++s_profile_discovery_serial;
    s_profile_discovery_request=(profile_discovery_request_t){.editor=s_editor_popup,
        .request=s_profile_discovery_serial,.epoch=s_profile_epoch,.generation=moonraker_config_generation(),
        .profile=s_editor_profile,.active=moonraker_config_active_profile_index()};
    strlcpy(s_profile_discovery_request.host,lv_textarea_get_text(s_editor_host),sizeof(s_profile_discovery_request.host));
    strlcpy(s_profile_discovery_request.port,lv_textarea_get_text(s_editor_port),sizeof(s_profile_discovery_request.port));
    strlcpy(s_profile_discovery_request.name,lv_textarea_get_text(s_editor_name),sizeof(s_profile_discovery_request.name));
    /* The callback may deliver a selected endpoint synchronously. Keep the
     * launcher fenced without blocking that normal handoff. */
    s_profile_discovery_dispatching=true;
    callback();
    s_profile_discovery_dispatching=false;
}


static void manager_set_status(
    const char *text)
{
    if (s_manager_status) {
        lv_label_set_text(
            s_manager_status,
            text ? text : ui_text(""));
    }
}


static void editor_set_status(
    const char *text)
{
    if (s_editor_status) {
        lv_label_set_text(
            s_editor_status,
            text ? text : ui_text(""));
    }
}


static void manager_refresh_selection(void)
{
    lv_obj_t *popup=s_manager_popup;uint32_t epoch=s_profile_epoch;int selected=s_selected_profile;
    for (int index = 0; index < MOONRAKER_CONFIG_MAX_PROFILES; ++index) {
        if(popup!=s_manager_popup || epoch!=s_profile_epoch || selected!=s_selected_profile)return;
        lv_obj_t *row=s_profile_rows[index];
        if(row && lv_obj_has_state(row,LV_STATE_USER_1)!=(index==selected)){
            if(index==selected)lv_obj_add_state(row,LV_STATE_USER_1);
            else lv_obj_remove_state(row,LV_STATE_USER_1);
        }
    }
    const moonraker_profile_t *profile = selected>=0 && selected<MOONRAKER_CONFIG_MAX_PROFILES
        ? moonraker_config_profile(selected) : NULL;
    if(popup!=s_manager_popup || epoch!=s_profile_epoch || selected!=s_selected_profile)return;
    bool configured=profile && profile->configured;
    if(s_manager_edit_action)
        ui_value_set_text(lv_obj_get_child(s_manager_edit_action,0), configured
            ? LV_SYMBOL_EDIT " Edit printer" : LV_SYMBOL_PLUS " Add printer");
}


static void profile_row_clicked_cb(
    lv_event_t *event)
{
    if(!event || !s_manager_popup || s_profile_dispatching || s_profile_discovery_dispatching || s_delete_popup ||
        lv_event_get_code(event)!=LV_EVENT_CLICKED)return;
    int profile_index =
        (int)(intptr_t)
            lv_event_get_user_data(event);

    if (profile_index < 0 ||
        profile_index >=
            MOONRAKER_CONFIG_MAX_PROFILES) {
        return;
    }
    if(lv_event_get_target_obj(event)!=s_profile_rows[profile_index] ||
        lv_event_get_current_target_obj(event)!=s_profile_rows[profile_index] ||
        lv_obj_has_state(s_profile_rows[profile_index],LV_STATE_DISABLED))return;

    s_selected_profile =
        profile_index;

    lv_obj_t *popup=s_manager_popup;uint32_t epoch=s_profile_epoch;
    manager_refresh_selection();
    if(popup!=s_manager_popup || epoch!=s_profile_epoch || profile_index!=s_selected_profile)return;

    const moonraker_profile_t *profile =
        moonraker_config_profile(
            profile_index);
    if(popup!=s_manager_popup || epoch!=s_profile_epoch || profile_index!=s_selected_profile)return;

    char status[160];

    if (profile && profile->configured) {
        snprintf(
            status,
            sizeof(status),
            "Selected: %s",
            profile->name);
    } else {
        snprintf(
            status,
            sizeof(status),
            "Slot %d is empty. Choose Add printer.",
            profile_index + 1);
    }

    manager_set_status(status);
}


static void manager_rebuild_rows(void)
{
    if (!s_manager_list) {
        return;
    }

    lv_obj_t *list=s_manager_list;
    uint32_t epoch=s_profile_epoch;
    memset(
        s_profile_rows,
        0,
        sizeof(s_profile_rows));
    lv_obj_clean(list);
    if(epoch!=s_profile_epoch || list!=s_manager_list)return;

    int active =
        moonraker_config_active_profile_index();

    for (int index = 0;
         index < MOONRAKER_CONFIG_MAX_PROFILES;
         ++index) {
        const moonraker_profile_t *profile =
            moonraker_config_profile(index);
        if(epoch!=s_profile_epoch || list!=s_manager_list)return;

        bool configured=profile && profile->configured;
        char row_text[MOONRAKER_CONFIG_NAME_LENGTH], endpoint_text[MOONRAKER_CONFIG_HOST_LENGTH+16];
        if(configured){
            strlcpy(row_text,profile->name,sizeof(row_text));
            snprintf(endpoint_text,sizeof(endpoint_text),"%s:%d",profile->host,profile->port);
        } else {
            snprintf(row_text,sizeof(row_text),"Add printer %d",index+1);
            strlcpy(endpoint_text,"Available profile slot",sizeof(endpoint_text));
        }
        lv_obj_t *row = ui_popup_add_selectable_row(s_manager_list, row_text,
            0, 0, 1, 72, profile_row_clicked_cb, (void *)(intptr_t)index);
        s_profile_rows[index] = row;
        if (!row) continue;
        profiles_watch(row);
        lv_obj_remove_style_all(row);
        lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_style_min_height(row, 72, 0);
        lv_obj_set_style_pad_hor(row, 12, 0);
        lv_obj_set_style_pad_ver(row, 8, 0);
        lv_obj_set_style_pad_row(row, 4, 0);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_radius(row,0,0);
        lv_obj_set_style_bg_color(row,UI_PANEL,0);
        lv_obj_set_style_bg_opa(row,LV_OPA_TRANSP,0);
        lv_obj_set_style_bg_opa(row,LV_OPA_COVER,LV_STATE_USER_1);
        lv_obj_set_style_bg_color(row,UI_CONTROL,LV_STATE_PRESSED);
        lv_obj_set_style_bg_opa(row,LV_OPA_COVER,LV_STATE_PRESSED);
        lv_obj_set_style_border_width(row,3,0);
        lv_obj_set_style_border_side(row,LV_BORDER_SIDE_LEFT,0);
        lv_obj_set_style_border_color(row,UI_ACCENT_BRIGHT,0);
        lv_obj_set_style_border_opa(row,LV_OPA_TRANSP,0);
        lv_obj_set_style_border_opa(row,LV_OPA_COVER,LV_STATE_USER_1);
        lv_obj_clear_flag(row,LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t *name = lv_obj_get_child(row, 0);
        lv_obj_set_width(name, LV_PCT(100));
        lv_obj_set_style_pad_right(name,88,0);
        lv_obj_set_height(name,lv_obj_get_style_text_font(name,0)->line_height);
        lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
        ui_apply_custom_label_style(name, UI_FONT_BODY_LARGE, configured ? UI_TEXT_BRIGHT : UI_TEXT_DIM);
        lv_obj_t *endpoint = profiles_label(row, endpoint_text, true);
        lv_obj_set_height(endpoint,lv_obj_get_style_text_font(endpoint,0)->line_height);
        lv_label_set_long_mode(endpoint, LV_LABEL_LONG_DOT);
        ui_apply_label_dim(endpoint);
        lv_obj_t *badge=profiles_label(row,configured && index==active ? "Active" : "",true);
        lv_obj_add_flag(badge,LV_OBJ_FLAG_IGNORE_LAYOUT);
        lv_obj_set_width(badge,76);
        lv_label_set_long_mode(badge,LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_align(badge,LV_TEXT_ALIGN_RIGHT,0);
        ui_apply_label_success(badge);
        lv_obj_align(badge,LV_ALIGN_TOP_RIGHT,0,0);
        lv_obj_t *divider=lv_obj_create(row);
        lv_obj_remove_style_all(divider);
        lv_obj_set_size(divider,LV_PCT(100),1);
        lv_obj_set_style_bg_color(divider,UI_BORDER,0);
        lv_obj_set_style_bg_opa(divider,LV_OPA_60,0);
        lv_obj_clear_flag(divider,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);

    }

    manager_refresh_selection();
}


static void delete_close(void)
{
    lv_obj_t *popup=s_delete_popup;
    ++s_profile_epoch;s_delete_popup=s_delete_apply=s_delete_status=NULL;s_delete_profile=-1;
    if(popup)lv_obj_delete(popup);
}


static void delete_cancel_cb(lv_event_t *event)
{
    (void)event;
    delete_close();
}


static void delete_confirm_cb(lv_event_t *event)
{
    if(!s_delete_popup || !manager_action_current(event,s_delete_apply))return;
    uint32_t epoch=s_profile_epoch;
    lv_obj_t *popup=s_delete_popup;
    int target=s_delete_profile;
    ui_printer_profiles_active_changed_cb_t changed=s_active_changed_cb;
    const moonraker_profile_t *profile=moonraker_config_profile(target);
    if(s_delete_generation!=moonraker_config_generation() || !profile || !profile->configured ||
        moonraker_config_profile_count()<=1) {
        if(s_delete_status)lv_label_set_text(s_delete_status,"Profiles changed or only one remains. Cancel and review again.");
        return;
    }
    char name[MOONRAKER_CONFIG_NAME_LENGTH];strlcpy(name,profile->name,sizeof(name));
    bool active_removed=target==moonraker_config_active_profile_index();
    s_profile_dispatching=true;
    bool removed=moonraker_config_delete_profile(target);
    if(!removed){
        if(epoch==s_profile_epoch && popup==s_delete_popup && s_delete_status)
            lv_label_set_text(s_delete_status,"Unable to remove this printer. The review is retained; retry when storage is available.");
        s_profile_dispatching=false;return;
    }
    printer_profile_health_set(target,true,false);
    printer_preview_cache_invalidate(target);printer_preview_store_invalidate(target);
    if(epoch==s_profile_epoch && popup==s_delete_popup) {
        s_selected_profile=moonraker_config_active_profile_index();
        delete_close();
        if(s_profile_epoch==epoch+1 && s_manager_popup){
            manager_rebuild_rows();
            if(s_profile_epoch==epoch+1){
                char status[160];snprintf(status,sizeof(status),"Removed printer profile: %s.",name);manager_set_status(status);
            }
        }
    }
    if(active_removed && changed)changed();else ui_printer_chooser_refresh();
    s_profile_dispatching=false;
}


static void manager_delete_cb(lv_event_t *event)
{
    if(!manager_action_current(event,s_manager_remove_action))return;

    const moonraker_profile_t *profile =
        moonraker_config_profile(s_selected_profile);

    if (!profile || !profile->configured) {
        manager_set_status("Select a configured printer to remove.");
        return;
    }

    if (moonraker_config_profile_count() <= 1) {
        manager_set_status(
            "At least one profile is required. Edit this profile instead.");
        return;
    }

    if (s_delete_popup) {
        lv_obj_move_foreground(s_delete_popup);
        return;
    }

    char reviewed_name[MOONRAKER_CONFIG_NAME_LENGTH];
    strlcpy(reviewed_name,profile->name,sizeof(reviewed_name));
    s_delete_profile=s_selected_profile;s_delete_generation=moonraker_config_generation();
    s_delete_popup = ui_popup_create(
        lv_screen_active(),
        profiles_width(640),
        profiles_height(330),
        UI_POPUP_DANGER);

    if (!s_delete_popup) {
        return;
    }

    lv_obj_add_event_cb(s_delete_popup, profiles_dialog_deleted, LV_EVENT_DELETE, NULL);
    lv_obj_t *body = profiles_detail_body(s_delete_popup, "Remove printer?");

    char message[256];
    snprintf(
        message,
        sizeof(message),
        "Remove %s from this panel?",
        reviewed_name);

    lv_obj_t *prompt=profiles_label(body, message, false);
    lv_obj_set_style_text_line_space(prompt,4,0);
    lv_obj_t *consequences=profiles_label(body,
        "Its saved endpoint, health state and cached preview will be deleted.",true);
    lv_obj_set_style_text_line_space(consequences,4,0);
    s_delete_status=profiles_label(body,"",true);
    lv_obj_set_style_text_line_space(s_delete_status,4,0);
    lv_obj_t *footer = profiles_footer(s_delete_popup);
    profiles_action(footer, UI_POPUP_ACTION_CANCEL, LV_SYMBOL_CLOSE " Cancel", delete_cancel_cb);
    s_delete_apply=profiles_action(footer, UI_POPUP_ACTION_DANGER, LV_SYMBOL_TRASH " Remove", delete_confirm_cb);

}


static void manager_select_cb(lv_event_t *event)
{
    if(s_delete_popup || !manager_action_current(event,s_manager_select_action))return;
    int target=s_selected_profile,previous=moonraker_config_active_profile_index();
    const moonraker_profile_t *profile=moonraker_config_profile(target);
    if(!profile || !profile->configured){manager_set_status("The selected slot is empty. Choose Add printer first.");return;}
    uint32_t epoch=s_profile_epoch;lv_obj_t *popup=s_manager_popup;
    ui_printer_profiles_active_changed_cb_t changed=s_active_changed_cb;
    s_profile_dispatching=true;
    bool selected=moonraker_config_select_profile(target);
    if(epoch==s_profile_epoch && popup==s_manager_popup){
        if(!selected)manager_set_status("Unable to activate the selected printer. Retry when storage is available.");
        else {
            manager_rebuild_rows();
            if(epoch==s_profile_epoch){char status[160];snprintf(status,sizeof(status),"Active printer: %s",moonraker_config_active_profile_name());manager_set_status(status);}
        }
    }
    if(selected && previous!=moonraker_config_active_profile_index() && changed)changed();
    s_profile_dispatching=false;
}


static void editor_keyboard_discard(void)
{
    ++s_profile_epoch;
    lv_obj_t *popup = s_editor_keyboard_popup;
    s_editor_keyboard_popup = NULL;
    s_editor_keyboard = NULL;
    s_editor_keyboard_value = NULL;
    s_editor_keyboard_target = NULL;
    s_editor_key_done=s_key_cancel_action=NULL;
    if (popup) lv_obj_delete(popup);
}

static void editor_keyboard_value_changed(lv_event_t *event)
{
    if (!event || s_profile_dispatching || s_profile_discovery_dispatching ||
        !s_editor_keyboard_popup || !s_editor_keyboard_value || !s_editor_keyboard_target ||
        s_editor_keyboard_target != s_editor_port ||
        lv_event_get_code(event) != LV_EVENT_VALUE_CHANGED ||
        lv_event_get_target_obj(event) != s_editor_keyboard_value ||
        lv_event_get_current_target_obj(event) != s_editor_keyboard_value) return;
    ui_popup_clear_feedback(ui_form_body(s_editor_keyboard_popup));
}

static void editor_keyboard_popup_done_cb(lv_event_t *event)
{
    if(!profiles_action_current(event,s_editor_key_done,true) || !s_editor_keyboard_popup ||
        !s_editor_keyboard_target || !s_editor_keyboard_value)return;
    if(s_editor_keyboard_target==s_editor_port && s_editor_keyboard_value) {
        const char *value=lv_textarea_get_text(s_editor_keyboard_value);char *end=NULL;
        long port=strtol(value?value:"",&end,10);
        if(!value||!value[0]||!end||*end||port<1||port>65535) {
            ui_popup_feedback(lv_obj_get_child(s_editor_keyboard_popup,1),UI_STATUS_WARNING,
                "Invalid port","Enter a port from 1 to 65535. Input has been kept.");
            ui_popup_focus_input(s_editor_keyboard_value);return;
        }
    }
    uint32_t epoch=s_profile_epoch;
    lv_obj_t *popup=s_editor_keyboard_popup;
    char value[MOONRAKER_CONFIG_CAMERA_URL_LENGTH];
    strlcpy(value,lv_textarea_get_text(s_editor_keyboard_value),sizeof(value));
    lv_textarea_set_text(s_editor_keyboard_target,value);
    profiles_clear_secret(value,sizeof(value));
    if(epoch!=s_profile_epoch || popup!=s_editor_keyboard_popup)return;
    editor_keyboard_discard();
}

static void editor_keyboard_cancel_cb(lv_event_t *event)
{
    if(!s_editor_keyboard_popup || (!s_editor_popup && !(s_camera_standalone && s_editor_camera_popup)) || s_profile_dispatching || s_profile_discovery_dispatching)return;
    if(!event){
        if(!s_key_cancel_action || lv_obj_has_state(s_key_cancel_action,LV_STATE_DISABLED))return;
    }else{
        lv_obj_t *target=lv_event_get_target_obj(event);
        if(target!=lv_event_get_current_target_obj(event) || lv_obj_has_state(target,LV_STATE_DISABLED))return;
        if(!((lv_event_get_code(event)==LV_EVENT_CLICKED && target==s_key_cancel_action) ||
            (lv_event_get_code(event)==LV_EVENT_CANCEL && target==s_editor_keyboard)))return;
    }
    editor_keyboard_discard();
}

static void editor_field_focused_cb(lv_event_t *event)
{
    if(!event || (!s_editor_popup && !(s_camera_standalone && s_editor_camera_popup)) || s_profile_dispatching || s_profile_discovery_dispatching)return;
    lv_obj_t *field = lv_event_get_target_obj(event);
    if(lv_event_get_code(event)!=LV_EVENT_CLICKED || field!=lv_event_get_current_target_obj(event) ||
        lv_obj_has_state(field,LV_STATE_DISABLED))return;
    if (!field || (field!=s_editor_name && field!=s_editor_host && field!=s_editor_port &&
        field!=s_editor_auth_key && field!=s_editor_camera_stream && field!=s_editor_camera_name)) return;
    if (s_editor_keyboard_popup && s_editor_keyboard_target == field) {
        lv_obj_move_foreground(s_editor_keyboard_popup);
        return;
    }
    lv_obj_t *editor=s_editor_popup;uint32_t epoch=s_profile_epoch;
    editor_keyboard_discard();
    if(editor!=s_editor_popup || s_profile_epoch!=epoch+1 || s_editor_keyboard_popup ||
        (field!=s_editor_name && field!=s_editor_host && field!=s_editor_port &&
         field!=s_editor_auth_key && field!=s_editor_camera_stream && field!=s_editor_camera_name))return;
    s_editor_keyboard_target = field;
    s_editor_keyboard_popup = ui_popup_create(lv_screen_active(), profiles_width(800), profiles_height(560), UI_POPUP_STANDARD);
    if (!s_editor_keyboard_popup) { s_editor_keyboard_target = NULL; return; }
    lv_obj_add_event_cb(s_editor_keyboard_popup, profiles_dialog_deleted, LV_EVENT_DELETE, NULL);
    const char *title = field == s_editor_port ? "Edit port" :
        field == s_editor_host ? "Edit host / IP" :
        field == s_editor_auth_key ? "Edit API key" :
        field == s_editor_camera_stream ? "Edit camera URL" :
        field == s_editor_camera_name ? "Edit camera name" : "Edit printer name";
    lv_obj_t *body = profiles_detail_body(s_editor_keyboard_popup, title);
    s_editor_keyboard_value = ui_popup_add_textarea(body, 1, 56, LV_ALIGN_TOP_LEFT, 0, 0,
        true, lv_textarea_get_password_mode(field), lv_textarea_get_max_length(field),
        "", lv_textarea_get_text(field), lv_textarea_get_accepted_chars(field));
    profiles_watch(s_editor_keyboard_value);
    lv_obj_add_event_cb(s_editor_keyboard_value,editor_keyboard_value_changed,LV_EVENT_VALUE_CHANGED,NULL);
    lv_obj_set_width(s_editor_keyboard_value, LV_PCT(100));
    lv_obj_set_style_min_height(s_editor_keyboard_value, 56, 0);
    lv_obj_set_style_text_font(s_editor_keyboard_value, UI_FONT_BODY_LARGE, 0);
    s_editor_keyboard = ui_popup_add_keyboard(body, s_editor_keyboard_value,
        1, 1, LV_ALIGN_TOP_LEFT, 0, 0,
        field == s_editor_port ? LV_KEYBOARD_MODE_NUMBER : LV_KEYBOARD_MODE_TEXT_LOWER);
    profiles_watch(s_editor_keyboard);
    lv_obj_set_size(s_editor_keyboard, LV_PCT(100), 0);
    lv_obj_set_flex_grow(s_editor_keyboard, 1);
    lv_obj_set_style_min_height(s_editor_keyboard, 140, 0);
    lv_obj_add_event_cb(s_editor_keyboard, editor_keyboard_popup_done_cb, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(s_editor_keyboard, editor_keyboard_cancel_cb, LV_EVENT_CANCEL, NULL);
    lv_obj_t *footer = profiles_footer(s_editor_keyboard_popup);
    s_key_cancel_action=profiles_action(footer, UI_POPUP_ACTION_CANCEL, "Cancel", editor_keyboard_cancel_cb);
    s_editor_key_done=profiles_action(footer, UI_POPUP_ACTION_CONFIRM, LV_SYMBOL_OK " Done", editor_keyboard_popup_done_cb);
    ui_popup_pin_keyboard(s_editor_keyboard_popup,s_editor_keyboard);
    lv_obj_move_foreground(s_editor_keyboard_popup);
}

static void editor_apply_identity_suggestion(
    const char *identity)
{
    /*
     * A verified identity is a suggestion for a fresh profile only. It never
     * overwrites an operator's existing custom profile name.
     */
    if (!identity || !identity[0] || !s_editor_name) {
        return;
    }

    char default_name[MOONRAKER_CONFIG_NAME_LENGTH];
    snprintf(
        default_name,
        sizeof(default_name),
        "Printer %d",
        s_editor_profile + 1);

    const char *current_name =
        lv_textarea_get_text(s_editor_name);

    if (!current_name || !current_name[0] ||
        strcmp(current_name, default_name) == 0) {
        lv_textarea_set_text(s_editor_name, identity);
    }
}


static bool editor_test_draft_current(void)
{
    return s_editor_popup && s_editor_popup==s_test_draft.popup && s_profile_epoch==s_test_draft.epoch &&
        s_editor_profile==s_test_draft.profile && moonraker_config_generation()==s_test_draft.generation &&
        s_editor_secure==s_test_draft.secure && s_editor_host && s_editor_port &&
        !strcmp(lv_textarea_get_text(s_editor_host),s_test_draft.host) &&
        !strcmp(lv_textarea_get_text(s_editor_port),s_test_draft.port_text) &&
        !strcmp(s_editor_api_key,s_test_draft.key);
}
static void editor_test_poll_cb(lv_timer_t *timer)
{
    if(!s_editor_test_timer || (timer && timer!=s_editor_test_timer) || s_profile_dispatching)return;
    if(!editor_test_draft_current()) {
        bool same_editor=s_editor_popup==s_test_draft.popup && s_profile_epoch==s_test_draft.epoch;
        editor_test_retire();
        if(same_editor)editor_set_status("Connection details changed. Run TEST again for this draft.");
        return;
    }
    uint32_t epoch=s_profile_epoch;
    moonraker_probe_result_t result={0};bool verified=false;
    s_profile_dispatching=true;
    bool received=moonraker_endpoint_test_take_result(&result,&verified);
    s_profile_dispatching=false;
    if(epoch!=s_profile_epoch || !s_editor_test_timer || !editor_test_draft_current())return;
    if(!received)return;
    editor_test_retire();
    if(!verified){editor_set_status("No verified Moonraker response. Check the host, port and network.");return;}
    editor_apply_identity_suggestion(result.identity);
    if(epoch!=s_profile_epoch || !s_editor_popup)return;
    char status[192];
    snprintf(status,sizeof(status),result.klippy_ready
        ? "Verified %s. Klipper is READY. Press SAVE to keep this profile."
        : "Verified %s. Moonraker is reachable; Klipper is not ready.",
        result.identity[0]?result.identity:"Moonraker");
    editor_set_status(status);
}


static void editor_auth_close(void)
{
    ++s_profile_epoch;
    lv_obj_t *popup=s_editor_auth_popup;
    s_editor_auth_popup=s_editor_auth_key=s_editor_auth_keyboard=s_editor_auth_use=s_auth_cancel_action=NULL;
    editor_keyboard_discard();
    if(popup)lv_obj_delete(popup);
}


static void editor_auth_cancel_cb(lv_event_t *event)
{
    if(!s_editor_auth_popup || !profiles_action_current(event,s_auth_cancel_action,false))return;
    editor_auth_close();
}


static void editor_auth_save_cb(lv_event_t *event)
{
    if (!profiles_action_current(event,s_editor_auth_use,false) || !s_editor_auth_key) {
        return;
    }

    strlcpy(
        s_editor_api_key,
        lv_textarea_get_text(s_editor_auth_key),
        sizeof(s_editor_api_key));
    uint32_t expected=s_profile_epoch+2;
    editor_auth_close();
    if(s_profile_epoch!=expected || !s_editor_popup)return;
    editor_set_status(
        s_editor_api_key[0]
            ? "API key ready. Test the connection, then save."
            : "Moonraker API key cleared. Trusted-client access remains enabled.");
}


static void editor_auth_open_cb(lv_event_t *event)
{
    if(!profiles_action_current(event,s_editor_auth_action,false))return;
    if (s_editor_auth_popup) { lv_obj_move_foreground(s_editor_auth_popup); return; }
    s_editor_auth_popup = ui_popup_create(lv_screen_active(), profiles_width(800), profiles_height(380), UI_POPUP_STANDARD);
    if (!s_editor_auth_popup) return;
    ++s_profile_epoch;
    lv_obj_add_event_cb(s_editor_auth_popup, profiles_dialog_deleted, LV_EVENT_DELETE, NULL);
    lv_obj_t *body = profiles_detail_body(s_editor_auth_popup, "API key");
    lv_obj_t *privacy=profiles_label(body, "Plain backups omit this key. Encrypted backups protect it with your passphrase.", true);
    lv_obj_set_style_text_line_space(privacy,4,0);
    s_editor_auth_key = profiles_field(body, "API key (optional)", s_editor_api_key, MOONRAKER_CONFIG_API_KEY_LENGTH - 1, NULL);
    lv_textarea_set_password_mode(s_editor_auth_key, true);
    lv_textarea_set_placeholder_text(s_editor_auth_key, "Leave blank for trusted clients");
    lv_obj_add_event_cb(s_editor_auth_key, editor_field_focused_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *footer = profiles_footer(s_editor_auth_popup);
    s_auth_cancel_action=profiles_action(footer, UI_POPUP_ACTION_CANCEL, LV_SYMBOL_CLOSE " Cancel", editor_auth_cancel_cb);
    s_editor_auth_use=profiles_action(footer, UI_POPUP_ACTION_CONFIRM, LV_SYMBOL_SAVE " Use key", editor_auth_save_cb);
    lv_obj_move_foreground(s_editor_auth_popup);
}


static void editor_test_cb(lv_event_t *event)
{
    if (!profiles_action_current(event,s_editor_test_action,false) || s_editor_test_timer ||
        !s_editor_host || !s_editor_port) {
        return;
    }

    const char *host = lv_textarea_get_text(s_editor_host);
    const char *port_text = lv_textarea_get_text(s_editor_port);

    if (!host || !host[0]) {
        editor_set_status("Enter a Moonraker host or IP address first.");
        return;
    }

    char *port_end = NULL;
    long port = strtol(port_text ? port_text : "", &port_end, 10);
    if (!port_text || !port_text[0] || !port_end || *port_end != '\0' ||
        port <= 0 || port >= 65536) {
        editor_set_status("Port must be between 1 and 65535.");
        return;
    }

    ++s_profile_epoch;
    editor_test_draft_t captured={.popup=s_editor_popup,.epoch=s_profile_epoch,
        .generation=moonraker_config_generation(),.profile=s_editor_profile,.port=(int)port,.secure=s_editor_secure};
    strlcpy(captured.host,host,sizeof(captured.host));strlcpy(captured.port_text,port_text,sizeof(captured.port_text));
    strlcpy(captured.key,s_editor_api_key,sizeof(captured.key));
    s_profile_dispatching=true;
    bool started=moonraker_endpoint_test_start(captured.host,captured.port,captured.key);
    s_profile_dispatching=false;
    if(captured.epoch!=s_profile_epoch || captured.popup!=s_editor_popup) {
        profiles_clear_secret(&captured,sizeof(captured));return;
    }
    if(!started){
        profiles_clear_secret(&captured,sizeof(captured));
        editor_set_status(moonraker_endpoint_test_busy()?"A connection test is already running.":"Unable to start the connection test.");return;
    }
    s_test_draft=captured;profiles_clear_secret(&captured,sizeof(captured));
    if(!editor_test_draft_current()){editor_test_retire();return;}
    s_profile_dispatching=true;
    editor_set_status("Testing Moonraker connection...");
    if(!editor_test_draft_current()){s_profile_dispatching=false;editor_test_retire();return;}
    s_editor_test_timer=lv_timer_create(editor_test_poll_cb,80,NULL);
    s_profile_dispatching=false;
    if(!s_editor_test_timer){editor_test_retire();editor_set_status("Connection test is running; wait for it to finish.");}

}


static void editor_camera_close(void)
{
    lv_obj_t *popup=s_editor_camera_popup,*review=s_editor_camera_remove_popup;
    lv_timer_t *discovery=s_editor_camera_discovery_timer,*test=s_editor_camera_test_timer;
    ++s_profile_epoch;
    s_editor_camera_popup=s_editor_camera_remove_popup=NULL;
    s_editor_camera_stream=s_editor_camera_name=s_editor_camera_keyboard=s_editor_camera_status=NULL;
    s_camera_remove_action=s_camera_remove_apply=s_camera_remove_cancel=s_camera_remove_status=NULL;
    s_camera_remove_profile=-1;
    s_camera_save=s_camera_default=s_camera_find=s_camera_test=s_camera_cancel=NULL;
    profiles_clear_secret(&s_camera_find_job,sizeof(s_camera_find_job));
    profiles_clear_secret(&s_camera_test_job,sizeof(s_camera_test_job));
    s_editor_camera_discovery_timer=s_editor_camera_test_timer=NULL;
    memset(s_editor_camera_slot_buttons,0,sizeof(s_editor_camera_slot_buttons));
    if(s_camera_standalone){
        s_camera_standalone=false;s_editor_profile=-1;s_editor_secure=false;
        profiles_clear_secret(s_editor_api_key,sizeof(s_editor_api_key));
        profiles_clear_secret(s_camera_setup_host,sizeof(s_camera_setup_host));
        profiles_clear_secret(s_camera_setup_port,sizeof(s_camera_setup_port));
    }
    if(discovery)lv_timer_delete(discovery);
    if(test)lv_timer_delete(test);
    ui_camera_set_setup_active(false);
    editor_keyboard_discard();
    if(review)lv_obj_delete(review);
    if(popup)lv_obj_delete(popup);
}

static void editor_camera_cancel_cb(lv_event_t *event)
{
    if(!s_editor_camera_popup || !profiles_action_current(event,s_camera_cancel,false))return;
    editor_camera_close();
}

static bool camera_action_current(lv_event_t *event,lv_obj_t *button)
{
    return s_editor_camera_popup && !s_editor_camera_remove_popup && profiles_action_current(event,button,false);
}
static camera_context_t camera_context(void)
{
    return (camera_context_t){.popup=s_editor_camera_popup,.editor=s_editor_popup,.epoch=s_profile_epoch,
        .generation=moonraker_config_generation(),.profile=s_editor_profile,
        .active=moonraker_config_active_profile_index(),.slot=s_editor_camera_slot};
}
static bool camera_owner_current(const camera_context_t *context)
{
    return context->popup && context->popup==s_editor_camera_popup && context->editor==s_editor_popup &&
        context->profile==s_editor_profile && context->slot==s_editor_camera_slot &&
        context->active==moonraker_config_active_profile_index();
}
static bool camera_context_current(const camera_context_t *context)
{
    return context->epoch==s_profile_epoch && camera_owner_current(context);
}
static bool camera_draft_capture(camera_draft_t *draft)
{
    if(!s_editor_camera_popup || s_editor_profile<0 || s_editor_profile>=MOONRAKER_CONFIG_MAX_PROFILES ||
        s_editor_camera_slot>=CAMERA_CATALOG_MAX_CAMERAS || !s_editor_camera_name || !s_editor_camera_stream)return false;
    *draft=(camera_draft_t){.context=camera_context()};
    strlcpy(draft->url,lv_textarea_get_text(s_editor_camera_stream),sizeof(draft->url));
    const char *name=lv_textarea_get_text(s_editor_camera_name);
    strlcpy(draft->name,name[0]?name:"Camera",sizeof(draft->name));
    return true;
}
static bool camera_url_valid(const char *url)
{
    return !url[0] || strncmp(url,"http://",7)==0 || strncmp(url,"https://",8)==0;
}
static bool editor_camera_store_draft(const camera_draft_t *draft,bool apply_primary)
{
    const camera_context_t *context=&draft->context;
    bool saved=(context->slot==0 && !apply_primary) ||
        camera_catalog_set(context->profile,context->slot,draft->name,draft->url);
    /* Dispatch prevents a replacement camera opening here. Keep accepted writes
     * staged for the same owner even if an unrelated field was deleted. */
    if(saved && camera_owner_current(context)){
        strlcpy(s_editor_camera_names[context->slot],draft->name,sizeof(s_editor_camera_names[context->slot]));
        if(context->slot==0)strlcpy(s_editor_camera_url,draft->url,sizeof(s_editor_camera_url));
    }
    return saved;
}
static void editor_camera_load_slot(void);

static void editor_camera_update_slot_buttons(void)
{
    camera_context_t context=camera_context();
    int profile=context.profile;
    if(!context.popup)return;
    size_t default_slot = camera_catalog_default(profile);
    if(!camera_context_current(&context))return;

    for (size_t index = 0; index < CAMERA_CATALOG_MAX_CAMERAS; ++index) {
        if(!camera_context_current(&context))return;
        if (!s_editor_camera_slot_buttons[index]) continue;

        camera_catalog_entry_t entry = {0};
        bool configured = camera_catalog_get(profile, index, &entry) &&
                          entry.configured;
        if(!camera_context_current(&context))return;
        lv_obj_t *button=s_editor_camera_slot_buttons[index];
        if(!button)continue;
        if (index == 0 && s_editor_camera_url[0]) configured = true;

        const char *name = s_editor_camera_names[index][0]
            ? s_editor_camera_names[index]
            : (entry.name[0] ? entry.name : "");
        char label[56];
        if (configured) {
            snprintf(label, sizeof(label), "%u: %.31s",
                     (unsigned)(index + 1), name[0] ? name : "Camera");
        } else {
            snprintf(label, sizeof(label), "+ Camera %u", (unsigned)(index + 1));
        }

        lv_obj_t *button_label = lv_obj_get_child(button, 0);
        if (button_label) lv_label_set_text(button_label, label);
        if(!camera_context_current(&context))return;
        lv_obj_t *marker=lv_obj_get_child(button,1);
        ui_value_set_text(marker,configured ? (default_slot==index ? "Default" : "") : "Available slot");
        if(!camera_context_current(&context))return;
        ui_value_set_color(marker,configured && default_slot==index ? UI_ACCENT_BRIGHT : UI_TEXT_DIM,0);
        if(!camera_context_current(&context))return;
        ui_button_set_filter_active(button, index == s_editor_camera_slot);
    }
}

static void editor_camera_select_slot_cb(lv_event_t *event)
{
    if(!event || !s_editor_camera_popup || s_editor_camera_remove_popup)return;
    size_t slot=(size_t)(uintptr_t)lv_event_get_user_data(event);
    if(slot>=CAMERA_CATALOG_MAX_CAMERAS || slot==s_editor_camera_slot ||
        !profiles_action_current(event,s_editor_camera_slot_buttons[slot],false))return;
    camera_draft_t draft;
    if(!camera_draft_capture(&draft))return;
    if(!camera_url_valid(draft.url)){
        editor_camera_set_status("Camera URL must be empty or start with http:// or https://.");return;
    }
    s_profile_dispatching=true;camera_jobs_retire();
    bool saved=editor_camera_store_draft(&draft,false);
    if(saved && draft.context.slot!=0){ui_camera_refresh_catalog();ui_dashboard_refresh_camera();}
    if(camera_context_current(&draft.context)){
        if(saved){s_editor_camera_slot=slot;editor_camera_load_slot();}
        else editor_camera_set_status("Unable to save this camera. Retry selecting the next slot.");
    }
    s_profile_dispatching=false;
}

static void editor_camera_load_slot(void)
{
    camera_context_t context=camera_context();
    if(!context.popup || context.profile<0 || context.slot>=CAMERA_CATALOG_MAX_CAMERAS)return;
    char url[MOONRAKER_CONFIG_CAMERA_URL_LENGTH]="";
    camera_catalog_entry_t entry={0};
    bool loaded=camera_catalog_get(context.profile,context.slot,&entry);
    if(!camera_context_current(&context))return;
    if(loaded && entry.name[0])strlcpy(s_editor_camera_names[context.slot],entry.name,sizeof(s_editor_camera_names[context.slot]));
    if(!s_editor_camera_names[context.slot][0])snprintf(s_editor_camera_names[context.slot],
        sizeof(s_editor_camera_names[context.slot]),"Camera %u",(unsigned)(context.slot+1));
    if(context.slot==0)strlcpy(url,s_editor_camera_url,sizeof(url));
    else if(entry.configured)strlcpy(url,entry.stream_url,sizeof(url));
    if(s_editor_camera_name)lv_textarea_set_text(s_editor_camera_name,s_editor_camera_names[context.slot]);
    if(!camera_context_current(&context))return;
    if(s_editor_camera_stream)lv_textarea_set_text(s_editor_camera_stream,url);
    if(!camera_context_current(&context))return;
    char status[128];
    snprintf(status,sizeof(status),"Editing Camera %u of %u. Enter its stream or search Moonraker.",
        (unsigned)(context.slot+1),(unsigned)CAMERA_CATALOG_MAX_CAMERAS);
    editor_camera_set_status(status);
    if(camera_context_current(&context))editor_camera_update_slot_buttons();
}

static void editor_camera_make_default_cb(lv_event_t *event)
{
    if(!camera_action_current(event,s_camera_default))return;
    camera_draft_t draft;
    if(!camera_draft_capture(&draft))return;
    if(!camera_url_valid(draft.url) || !draft.url[0]){
        editor_camera_set_status("Enter a valid camera stream before making it default.");return;
    }
    s_profile_dispatching=true;camera_jobs_retire();
    bool saved=editor_camera_store_draft(&draft,true),selected=false;
    if(saved && camera_context_current(&draft.context))
        selected=camera_catalog_set_default(draft.context.profile,draft.context.slot);
    if(saved){ui_camera_refresh_catalog();ui_dashboard_refresh_camera();}
    if(camera_context_current(&draft.context)){
        editor_camera_update_slot_buttons();
        if(camera_context_current(&draft.context))editor_camera_set_status(selected
            ?"This is now the default camera for this printer."
            :saved?"Camera saved; unable to set it as default. Retry DEFAULT.":"Unable to save this camera. Retry DEFAULT.");
    }
    s_profile_dispatching=false;
}

static void editor_camera_remove_close(void)
{
    lv_obj_t *popup=s_editor_camera_remove_popup;
    s_editor_camera_remove_popup=NULL;
    s_camera_remove_apply=s_camera_remove_cancel=s_camera_remove_status=NULL;
    s_camera_remove_profile=-1;
    ++s_profile_epoch;
    if(popup)lv_obj_delete(popup);
}

static void editor_camera_remove_cancel_cb(lv_event_t *event)
{
    if(event && !profiles_action_current(event,s_camera_remove_cancel,false))return;
    lv_obj_t *popup=s_editor_camera_popup;
    uint32_t epoch=s_profile_epoch;
    editor_camera_remove_close();
    if(popup && popup==s_editor_camera_popup && epoch+1==s_profile_epoch && !s_editor_camera_remove_popup)
        editor_camera_set_status("Camera removal cancelled. Run Find or Test again if needed.");
}

static void editor_camera_remove_apply_cb(lv_event_t *event)
{
    if(!s_editor_camera_popup || !s_editor_camera_remove_popup ||
        !profiles_action_current(event,s_camera_remove_apply,false))return;
    if(s_camera_remove_profile!=s_editor_profile || s_camera_remove_profile<0 ||
        s_camera_remove_slot>=CAMERA_CATALOG_MAX_CAMERAS ||
        s_camera_remove_generation!=moonraker_config_generation()){
        if(s_camera_remove_status)lv_label_set_text(s_camera_remove_status,
            "Printer configuration changed. Keep Camera and review removal again.");
        return;
    }
    int profile=s_camera_remove_profile;
    size_t slot=s_camera_remove_slot;
    lv_obj_t *popup=s_editor_camera_popup,*review=s_editor_camera_remove_popup;
    uint32_t epoch=s_profile_epoch;
    s_profile_dispatching=true;
    bool removed=camera_catalog_clear(profile,slot);
    /* The accepted write belongs to the captured catalog even if its UI closed. */
    if(removed){ui_camera_refresh_catalog();ui_dashboard_refresh_camera();}
    if(popup!=s_editor_camera_popup || review!=s_editor_camera_remove_popup || profile!=s_editor_profile){
        s_profile_dispatching=false;return;
    }
    if(!removed){
        if(s_camera_remove_status)lv_label_set_text(s_camera_remove_status,
            "Unable to remove this camera. Try Remove again or Keep Camera.");
        s_profile_dispatching=false;return;
    }
    if(slot==0)s_editor_camera_url[0]='\0';
    snprintf(s_editor_camera_names[slot],sizeof(s_editor_camera_names[slot]),
        "Camera %u",(unsigned)(slot+1));
    epoch=s_profile_epoch;
    editor_camera_remove_close();
    if(popup==s_editor_camera_popup && s_profile_epoch==epoch+1 && profile==s_editor_profile){
        epoch=s_profile_epoch;
        if(s_editor_camera_slot==slot){
            if(s_editor_camera_name)lv_textarea_set_text(s_editor_camera_name,s_editor_camera_names[slot]);
            if(epoch==s_profile_epoch && popup==s_editor_camera_popup && s_editor_camera_stream)
                lv_textarea_set_text(s_editor_camera_stream,"");
        }
        if(epoch==s_profile_epoch && popup==s_editor_camera_popup)editor_camera_update_slot_buttons();
        if(epoch==s_profile_epoch && popup==s_editor_camera_popup)editor_camera_set_status("Camera removed.");
    }
    s_profile_dispatching=false;
}

static void editor_camera_remove_cb(lv_event_t *event)
{
    if(!s_editor_camera_popup || !profiles_action_current(event,s_camera_remove_action,false))return;
    if (s_editor_camera_remove_popup) {
        lv_obj_move_foreground(s_editor_camera_remove_popup);
        return;
    }
    if(s_editor_profile<0 || s_editor_camera_slot>=CAMERA_CATALOG_MAX_CAMERAS)return;
    s_camera_remove_profile=s_editor_profile;
    s_camera_remove_slot=s_editor_camera_slot;
    s_camera_remove_generation=moonraker_config_generation();
    /* A result from an earlier Find must not re-import a removed camera. */
    lv_timer_t *discovery=s_editor_camera_discovery_timer,*test=s_editor_camera_test_timer;
    s_editor_camera_discovery_timer=s_editor_camera_test_timer=NULL;
    profiles_clear_secret(&s_camera_find_job,sizeof(s_camera_find_job));
    profiles_clear_secret(&s_camera_test_job,sizeof(s_camera_test_job));
    if(discovery)lv_timer_delete(discovery);
    if(test)lv_timer_delete(test);
    const char *name = s_editor_camera_names[s_camera_remove_slot][0]
        ? s_editor_camera_names[s_camera_remove_slot] : "this camera";
    char prompt[160];
    snprintf(prompt, sizeof(prompt),
             "Remove Camera %u (%s) from this printer? Its saved stream and name will be cleared.",
             (unsigned)(s_camera_remove_slot+1),name);

    s_editor_camera_remove_popup = ui_popup_create(
        lv_screen_active(), profiles_width(620), profiles_height(340), UI_POPUP_STANDARD);
    if (!s_editor_camera_remove_popup){s_camera_remove_profile=-1;return;}
    lv_obj_add_event_cb(s_editor_camera_remove_popup, profiles_dialog_deleted, LV_EVENT_DELETE, NULL);
    lv_obj_t *body = profiles_detail_body(s_editor_camera_remove_popup, "Remove camera?");
    lv_obj_t *warning=profiles_label(body, prompt, false);
    lv_obj_set_style_text_line_space(warning,4,0);
    s_camera_remove_status=profiles_label(body, "This cannot be undone from the panel.", true);
    lv_obj_set_style_text_line_space(s_camera_remove_status,4,0);
    lv_obj_t *footer = profiles_footer(s_editor_camera_remove_popup);
    s_camera_remove_cancel=profiles_action(footer, UI_POPUP_ACTION_CANCEL, LV_SYMBOL_CLOSE " Keep camera", editor_camera_remove_cancel_cb);
    s_camera_remove_apply=profiles_action(footer, UI_POPUP_ACTION_DANGER, LV_SYMBOL_TRASH " Remove", editor_camera_remove_apply_cb);
    lv_obj_move_foreground(s_editor_camera_remove_popup);
}

static void editor_camera_save_cb(lv_event_t *event)
{
    if(!camera_action_current(event,s_camera_save))return;
    camera_draft_t draft;
    if(!camera_draft_capture(&draft))return;
    if(!camera_url_valid(draft.url)){
        editor_camera_set_status("Camera URL must be empty or start with http:// or https://.");return;
    }
    s_profile_dispatching=true;camera_jobs_retire();
    bool saved=editor_camera_store_draft(&draft,true);
    if(saved){ui_camera_refresh_catalog();ui_dashboard_refresh_camera();}
    if(camera_context_current(&draft.context)){
        if(!saved)editor_camera_set_status("Unable to apply this camera. Retry Use camera.");
        else{
            editor_camera_close();
            if(draft.context.editor==s_editor_popup && draft.context.profile==s_editor_profile &&
                s_profile_epoch==draft.context.epoch+2)editor_set_status(draft.url[0]
                    ?"Camera source applied.":"Camera source cleared.");
        }
    }
    s_profile_dispatching=false;
}


static void editor_camera_set_status(const char *text)
{
    if (s_editor_camera_status) {
        lv_label_set_text(s_editor_camera_status, text ? text : ui_text(""));
    }
}


static const char *camera_find_host(void)
{
    return s_camera_standalone ? s_camera_setup_host :
        s_editor_host ? lv_textarea_get_text(s_editor_host) : NULL;
}
static const char *camera_find_port(void)
{
    return s_camera_standalone ? s_camera_setup_port :
        s_editor_port ? lv_textarea_get_text(s_editor_port) : NULL;
}
static bool camera_job_current(const camera_job_t *job,bool find,bool generation)
{
    if(!camera_context_current(&job->draft.context) || !s_editor_camera_stream ||
        strcmp(job->draft.url,lv_textarea_get_text(s_editor_camera_stream)))return false;
    if(generation && job->draft.context.generation!=moonraker_config_generation())return false;
    if(!find)return true;
    return s_editor_camera_name && camera_find_host() && camera_find_port() &&
        !strcmp(job->draft.name,lv_textarea_get_text(s_editor_camera_name)) &&
        !strcmp(job->host,camera_find_host()) && !strcmp(job->port,camera_find_port()) &&
        !strcmp(job->key,s_editor_api_key) && job->secure==s_editor_secure;
}
static bool camera_job_capture(camera_job_t *job,bool find)
{
    *job=(camera_job_t){.draft.context=camera_context()};
    if(!s_editor_camera_stream || job->draft.context.profile<0 ||
        job->draft.context.slot>=CAMERA_CATALOG_MAX_CAMERAS)return false;
    strlcpy(job->draft.url,lv_textarea_get_text(s_editor_camera_stream),sizeof(job->draft.url));
    if(!find)return true;
    if(!s_editor_camera_name || !camera_find_host() || !camera_find_port())return false;
    strlcpy(job->draft.name,lv_textarea_get_text(s_editor_camera_name),sizeof(job->draft.name));
    strlcpy(job->host,camera_find_host(),sizeof(job->host));
    strlcpy(job->port,camera_find_port(),sizeof(job->port));
    strlcpy(job->key,s_editor_api_key,sizeof(job->key));job->secure=s_editor_secure;
    return true;
}
static void camera_job_changed(bool find)
{
    camera_job_retire(find);
    editor_camera_set_status(find?"Camera or printer details changed. Search cameras again."
        :"Camera or printer details changed. Test this camera again.");
}
static void editor_camera_discovery_poll_cb(lv_timer_t *timer)
{
    if(!timer || timer!=s_editor_camera_discovery_timer || s_profile_dispatching)return;
    if(!camera_job_current(&s_camera_find_job,true,true)){camera_job_changed(true);return;}
    camera_context_t context=s_camera_find_job.draft.context;
    moonraker_webcam_t webcam={0};bool found=false;size_t count=0;
    s_profile_dispatching=true;
    bool ready=camera_discovery_take_result(&webcam,&found,&count);
    /* Consumption imports Camera 1 and can advance its own config generation. */
    if(timer!=s_editor_camera_discovery_timer || !camera_context_current(&context)){
        if(timer==s_editor_camera_discovery_timer)camera_job_retire(true);
        s_profile_dispatching=false;return;
    }
    if(!camera_job_current(&s_camera_find_job,true,!ready || !found)){
        camera_job_changed(true);s_profile_dispatching=false;return;
    }
    if(!ready){s_profile_dispatching=false;return;}
    camera_job_retire(true);
    if(!found){
        editor_camera_set_status("Moonraker did not return another camera. Enter this stream URL manually.");
        s_profile_dispatching=false;return;
    }
    camera_job_retire(false);
    /* Keep the staged primary URL current even when an additional slot was selected. */
    strlcpy(s_editor_camera_url,webcam.stream_url,sizeof(s_editor_camera_url));
    strlcpy(s_editor_camera_names[0],webcam.name[0]?webcam.name:"Camera 1",sizeof(s_editor_camera_names[0]));
    editor_camera_load_slot();
    if(camera_context_current(&context)){
        char status[192];snprintf(status,sizeof(status),"%u camera%s found and imported. Camera %u is selected.",
            (unsigned)count,count==1?"":"s",(unsigned)(context.slot+1));
        editor_camera_set_status(status);
    }
    s_profile_dispatching=false;
}
static void editor_camera_test_poll_cb(lv_timer_t *timer)
{
    if(!timer || timer!=s_editor_camera_test_timer || s_profile_dispatching)return;
    if(!camera_job_current(&s_camera_test_job,false,true)){camera_job_changed(false);return;}
    camera_context_t context=s_camera_test_job.draft.context;
    bool ok=false;int width=0,height=0;size_t bytes=0;
    s_profile_dispatching=true;
    bool ready=camera_test_take_result(&ok,&width,&height,&bytes);
    if(timer!=s_editor_camera_test_timer || !camera_context_current(&context)){
        if(timer==s_editor_camera_test_timer)camera_job_retire(false);
        s_profile_dispatching=false;return;
    }
    if(!camera_job_current(&s_camera_test_job,false,true)){
        camera_job_changed(false);s_profile_dispatching=false;return;
    }
    if(ready){
        camera_job_retire(false);
        if(ok){char status[128];snprintf(status,sizeof(status),"Camera verified: %d x %d JPEG (%u KB).",
            width,height,(unsigned)((bytes+1023U)/1024U));editor_camera_set_status(status);}
        else editor_camera_set_status("Camera test failed: no JPEG frame returned.");
    }
    s_profile_dispatching=false;
}
static void editor_camera_test_cb(lv_event_t *event)
{
    if(!camera_action_current(event,s_camera_test) || s_editor_camera_test_timer)return;
    camera_job_t captured;
    if(!camera_job_capture(&captured,false))return;
    if(!captured.draft.url[0] || !camera_url_valid(captured.draft.url)){
        editor_camera_set_status("Enter or find a valid camera stream URL first.");return;
    }
    s_profile_dispatching=true;
    bool started=camera_test_start(captured.draft.url);
    if(camera_job_current(&captured,false,true)){
        if(started){
            s_camera_test_job=captured;
            editor_camera_set_status("Testing camera stream...");
            if(camera_job_current(&s_camera_test_job,false,true)){
                s_editor_camera_test_timer=lv_timer_create(editor_camera_test_poll_cb,80,NULL);
                if(!s_editor_camera_test_timer){camera_job_retire(false);editor_camera_set_status("Camera test is running; retry Test after it finishes.");}
            }else camera_job_retire(false);
        }else editor_camera_set_status(camera_test_busy()?"Camera test already running.":"Unable to start camera test.");
    }
    profiles_clear_secret(&captured,sizeof(captured));s_profile_dispatching=false;
}
static void editor_camera_discover_cb(lv_event_t *event)
{
    if(!camera_action_current(event,s_camera_find) || s_editor_camera_discovery_timer)return;
    camera_job_t captured;
    if(!camera_job_capture(&captured,true)){profiles_clear_secret(&captured,sizeof(captured));return;}
    char *end=NULL;long port=strtol(captured.port,&end,10);
    if(!captured.host[0] || !captured.port[0] || !end || *end || port<1 || port>65535){
        profiles_clear_secret(&captured,sizeof(captured));
        editor_camera_set_status("Enter a valid Moonraker host and port before searching.");return;
    }
    s_profile_dispatching=true;
    bool started=camera_discovery_start_for_profile(captured.draft.context.profile,captured.host,(int)port,captured.key);
    if(camera_job_current(&captured,true,true)){
        if(started){
            s_camera_find_job=captured;
            editor_camera_set_status("Searching Moonraker for configured cameras...");
            if(camera_job_current(&s_camera_find_job,true,true)){
                s_editor_camera_discovery_timer=lv_timer_create(editor_camera_discovery_poll_cb,80,NULL);
                if(!s_editor_camera_discovery_timer){camera_job_retire(true);editor_camera_set_status("Camera search is running; retry Find after it finishes.");}
            }else camera_job_retire(true);
        }else editor_camera_set_status(camera_discovery_busy()?"Camera search is already running.":"Unable to start the camera search.");
    }
    profiles_clear_secret(&captured,sizeof(captured));s_profile_dispatching=false;
}


static void editor_camera_field_focused_cb(lv_event_t *event)
{
    editor_field_focused_cb(event);
}

static void editor_camera_open(void)
{
    if (s_editor_camera_popup) { lv_obj_move_foreground(s_editor_camera_popup); return; }
    s_editor_camera_popup = ui_popup_create_on_screen(profiles_width(800), profiles_height(560), UI_POPUP_STANDARD);
    if (!s_editor_camera_popup) return;
    lv_obj_add_event_cb(s_editor_camera_popup, profiles_dialog_deleted, LV_EVENT_DELETE, NULL);
    ui_camera_set_setup_active(true);
    lv_obj_t *body = profiles_detail_body(s_editor_camera_popup, "Camera setup");
    lv_obj_set_style_bg_color(body,lv_obj_get_style_bg_color(s_editor_camera_popup,0),0);
    lv_obj_set_style_bg_opa(body,LV_OPA_COVER,0);
    lv_obj_t *slots = profiles_footer(body);
    lv_obj_clear_flag(slots,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);
    static const int32_t wide_columns[]={LV_GRID_FR(1),LV_GRID_FR(1),LV_GRID_FR(1),LV_GRID_FR(1),LV_GRID_TEMPLATE_LAST};
    static const int32_t narrow_columns[]={LV_GRID_FR(1),LV_GRID_FR(1),LV_GRID_TEMPLATE_LAST};
    static const int32_t wide_rows[]={72,LV_GRID_TEMPLATE_LAST};
    static const int32_t narrow_rows[]={72,72,LV_GRID_TEMPLATE_LAST};
    int columns=lv_display_get_horizontal_resolution(NULL)>=900 ? 4 : 2;
    lv_obj_set_grid_dsc_array(slots,columns==4?wide_columns:narrow_columns,columns==4?wide_rows:narrow_rows);
    for (size_t index = 0; index < CAMERA_CATALOG_MAX_CAMERAS; ++index) {
        /* This two-line slot owns its labels; generic centered-button resize
         * handling would replace the grid label width/alignment. */
        lv_obj_t *button = ui_button_create_empty(slots,UI_BUTTON_OUTLINED);
        s_editor_camera_slot_buttons[index] = button;
        if(!button)continue;
        profiles_watch(button);
        lv_obj_add_event_cb(button,editor_camera_select_slot_cb,LV_EVENT_CLICKED,(void *)(uintptr_t)index);
        lv_obj_set_style_pad_hor(button,12,0);
        lv_obj_set_style_min_width(button,0,0);
        lv_obj_set_grid_cell(button,LV_GRID_ALIGN_STRETCH,index%columns,1,LV_GRID_ALIGN_STRETCH,index/columns,1);
        lv_obj_set_style_pad_ver(button,6,0);
        lv_obj_set_style_border_width(button,UI_BORDER_STRONG,0);
        ui_button_set_filter_active(button, false);
        /* Selection is exclusive and owned by update_slot_buttons. A second
         * native tap must not toggle the current slot off. */
        lv_obj_clear_flag(button, LV_OBJ_FLAG_CHECKABLE);
        lv_obj_t *label = profiles_label(button,"+ Camera",false);
        ui_apply_custom_label_style(label,UI_FONT_BODY_LARGE,UI_TEXT_BRIGHT);
        lv_obj_set_width(label, LV_PCT(100));
        lv_obj_set_height(label,lv_obj_get_style_text_font(label,0)->line_height);
        lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
        lv_obj_align(label,LV_ALIGN_TOP_LEFT,0,0);
        lv_obj_t *marker=profiles_label(button,"",true);
        ui_apply_label_dim(marker);
        lv_obj_set_height(marker,lv_obj_get_style_text_font(marker,0)->line_height);
        lv_label_set_long_mode(marker,LV_LABEL_LONG_CLIP);
        lv_obj_align(marker,LV_ALIGN_BOTTOM_LEFT,0,0);
    }
    lv_obj_t *name_group=profiles_form_group(body);
    s_editor_camera_name = profiles_field(name_group, "Camera name", "", CAMERA_CATALOG_NAME_LENGTH - 1, NULL);
    lv_obj_t *stream_group=profiles_form_group(body);
    s_editor_camera_stream = profiles_field(stream_group, "Stream URL (optional)", s_editor_camera_url, MOONRAKER_CONFIG_CAMERA_URL_LENGTH - 1, NULL);
    lv_obj_add_event_cb(s_editor_camera_name, editor_camera_field_focused_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(s_editor_camera_stream, editor_camera_field_focused_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *actions = profiles_footer(body);
    s_camera_find=profiles_action(actions, UI_POPUP_ACTION_SECONDARY, LV_SYMBOL_REFRESH " Find cameras", editor_camera_discover_cb);
    s_camera_test=profiles_action(actions, UI_POPUP_ACTION_SECONDARY, LV_SYMBOL_PLAY " Test stream", editor_camera_test_cb);
    s_camera_default=profiles_action(actions, UI_POPUP_ACTION_SECONDARY, LV_SYMBOL_OK " Make default", editor_camera_make_default_cb);
    s_camera_remove_action=profiles_action(actions, UI_POPUP_ACTION_DANGER, LV_SYMBOL_TRASH " Remove", editor_camera_remove_cb);
    s_editor_camera_status = profiles_label(body, "Select a slot, then find a camera or enter its stream URL.", true);
    lv_obj_t *footer = profiles_footer(s_editor_camera_popup);
    s_camera_cancel=profiles_action(footer, UI_POPUP_ACTION_CANCEL, LV_SYMBOL_CLOSE " Cancel", editor_camera_cancel_cb);
    s_camera_save=profiles_action(footer, UI_POPUP_ACTION_CONFIRM, LV_SYMBOL_SAVE " Use camera", editor_camera_save_cb);
    camera_context_t context=camera_context();
    editor_camera_load_slot();
    if(camera_context_current(&context))lv_obj_move_foreground(context.popup);
}

static void editor_camera_open_cb(lv_event_t *event)
{
    if(profiles_action_current(event,s_editor_camera_action,false))editor_camera_open();
}

static bool editor_save_current(lv_obj_t *popup,uint32_t epoch,int active)
{
    return popup==s_editor_popup && epoch==s_profile_epoch && active==moonraker_config_active_profile_index();
}
static void editor_save_cb(lv_event_t *event)
{
    if(!profiles_action_current(event,s_editor_save_action,false) || s_editor_profile<0 ||
        !s_editor_name || !s_editor_host || !s_editor_port)return;
    const char *port_text=lv_textarea_get_text(s_editor_port);char *end=NULL;
    long port=strtol(port_text?port_text:"",&end,10);
    if(!port_text || !port_text[0] || !end || *end || port<1 || port>65535){editor_set_status("Port must be between 1 and 65535.");return;}
    if(!lv_textarea_get_text(s_editor_host)[0]){editor_set_status("A Moonraker host or IP address is required.");return;}
    moonraker_profile_t draft={.port=(int)port,.secure_transport=s_editor_secure};
    strlcpy(draft.name,lv_textarea_get_text(s_editor_name),sizeof(draft.name));
    strlcpy(draft.host,lv_textarea_get_text(s_editor_host),sizeof(draft.host));
    strlcpy(draft.api_key,s_editor_api_key,sizeof(draft.api_key));
    strlcpy(draft.camera_stream_url,s_editor_camera_url,sizeof(draft.camera_stream_url));
    char camera_name[CAMERA_CATALOG_NAME_LENGTH];strlcpy(camera_name,s_editor_camera_names[0],sizeof(camera_name));
    int target=s_editor_profile,active=moonraker_config_active_profile_index();
    uint32_t epoch=s_profile_epoch;lv_obj_t *popup=s_editor_popup;
    ui_printer_profiles_active_changed_cb_t changed=s_active_changed_cb;
    bool notify=false,camera_written=false;
    s_profile_dispatching=true;
    bool approved=!draft.secure_transport || moonraker_transport_security_ca_pem_for_profile(target)!=NULL;
    if(!editor_save_current(popup,epoch,active))goto finished;
    if(!approved){editor_set_status("Secure mode requires an approved CA certificate.");goto finished;}
    bool saved=moonraker_config_save_profile(target,draft.name,draft.host,draft.port,draft.api_key);
    notify=saved && target==active;
    if(!editor_save_current(popup,epoch,active))goto finished;
    if(!saved){editor_set_status("Could not save the profile. Check the host, port and storage, then retry.");goto finished;}
    saved=moonraker_config_set_camera_stream_url(target,draft.camera_stream_url);
    camera_written=saved;
    if(!editor_save_current(popup,epoch,active))goto finished;
    if(!saved){editor_set_status("Connection saved; camera URL could not be saved. Check the URL/storage and retry.");goto finished;}
    saved=camera_catalog_set(target,0,camera_name,draft.camera_stream_url);
    if(!editor_save_current(popup,epoch,active))goto finished;
    if(!saved){editor_set_status("Connection and camera URL saved; camera name could not be saved. Retry when storage is available.");goto finished;}
    saved=moonraker_config_set_transport_security(target,draft.secure_transport);
    if(!editor_save_current(popup,epoch,active))goto finished;
    if(!saved){editor_set_status("Profile/camera saved; connection-security setting could not be saved. Retry when storage is available.");goto finished;}
    editor_close();
    if(s_profile_epoch==epoch+1 && !s_editor_popup){
        manager_rebuild_rows();
        if(s_profile_epoch==epoch+1)manager_set_status(target==active?"Active printer updated. Reconnecting...":"Printer profile saved.");
    }
finished:
    profiles_clear_secret(&draft,sizeof(draft));
    if(camera_written){ui_camera_refresh_catalog();ui_dashboard_refresh_camera();}
    if(notify && changed)changed();
    s_profile_dispatching=false;
}


static bool profile_apply_discovered_endpoint(const char *host,int port,const char *identity)
{
    if(s_profile_dispatching || !s_editor_popup || !s_editor_host || !s_editor_port ||
        !host || !host[0] || port<1 || port>65535)return false;
    memset(&s_profile_discovery_request,0,sizeof(s_profile_discovery_request));
    bool applied=false;
    char captured_host[MOONRAKER_CONFIG_HOST_LENGTH],captured_identity[MOONRAKER_CONFIG_NAME_LENGTH],port_text[6];
    strlcpy(captured_host,host,sizeof(captured_host));
    strlcpy(captured_identity,identity?identity:"",sizeof(captured_identity));
    snprintf(port_text,sizeof(port_text),"%d",port);
    lv_obj_t *popup=s_editor_popup;uint32_t epoch=s_profile_epoch,generation=moonraker_config_generation();
    int profile=s_editor_profile,active=moonraker_config_active_profile_index();
    s_profile_dispatching=true;editor_test_retire();camera_jobs_retire();
    lv_textarea_set_text(s_editor_host,captured_host);
    if(popup!=s_editor_popup || epoch!=s_profile_epoch || profile!=s_editor_profile ||
        generation!=moonraker_config_generation() || active!=moonraker_config_active_profile_index())goto finished;
    lv_textarea_set_text(s_editor_port,port_text);
    if(popup!=s_editor_popup || epoch!=s_profile_epoch || profile!=s_editor_profile ||
        generation!=moonraker_config_generation() || active!=moonraker_config_active_profile_index())goto finished;
    editor_apply_identity_suggestion(captured_identity);
    if(popup==s_editor_popup && epoch==s_profile_epoch && profile==s_editor_profile &&
        generation==moonraker_config_generation() && active==moonraker_config_active_profile_index())
    {
        editor_set_status("Verified Moonraker endpoint selected. Review the name and press SAVE.");
        applied=popup==s_editor_popup && epoch==s_profile_epoch;
    }
finished:
    s_profile_dispatching=false;return applied;
}
void ui_printer_profiles_set_discovered_endpoint(const char *host,int port,const char *identity)
{
    (void)profile_apply_discovered_endpoint(host,port,identity);
}
bool ui_printer_profiles_apply_discovered_endpoint(uint32_t request,const char *host,int port,const char *identity)
{
    if(!request || request!=s_profile_discovery_request.request || s_profile_dispatching)return false;
    if(!profile_discovery_request_current()){
        memset(&s_profile_discovery_request,0,sizeof(s_profile_discovery_request));return false;
    }
    return profile_apply_discovered_endpoint(host,port,identity);
}


static void manager_edit_cb(
    lv_event_t *event)
{
    (void)event;
    if(!s_manager_popup || s_selected_profile<0 || s_selected_profile>=MOONRAKER_CONFIG_MAX_PROFILES)return;

    if (s_editor_popup) {
        lv_obj_move_foreground(
            s_editor_popup);
        return;
    }

    lv_obj_t *manager=s_manager_popup;uint32_t epoch=s_profile_epoch;int target=s_selected_profile;
    const moonraker_profile_t *profile=moonraker_config_profile(target);
    if(manager!=s_manager_popup || epoch!=s_profile_epoch || target!=s_selected_profile || s_editor_popup)return;
    moonraker_profile_t snapshot={0};
    if(profile){snapshot=*profile;profile=&snapshot;}
    char camera_names[CAMERA_CATALOG_MAX_CAMERAS][CAMERA_CATALOG_NAME_LENGTH]={0};
    for(size_t index=0;index<CAMERA_CATALOG_MAX_CAMERAS;++index){
        camera_catalog_entry_t entry={0};
        (void)camera_catalog_get(target,index,&entry);
        if(manager!=s_manager_popup || epoch!=s_profile_epoch || target!=s_selected_profile || s_editor_popup){
            profiles_clear_secret(&snapshot,sizeof(snapshot));return;
        }
        if(entry.name[0])strlcpy(camera_names[index],entry.name,sizeof(camera_names[index]));
        else snprintf(camera_names[index],sizeof(camera_names[index]),"Camera %u",(unsigned)(index+1));
    }
    memcpy(s_editor_camera_names,camera_names,sizeof(s_editor_camera_names));

    char default_name[
        MOONRAKER_CONFIG_NAME_LENGTH];

    snprintf(
        default_name,
        sizeof(default_name),
        "Printer %d",
        s_selected_profile + 1);

    const char *name =
        profile &&
        profile->configured &&
        profile->name[0]
            ? profile->name
            : default_name;

    const char *host =
        profile &&
        profile->configured
            ? profile->host
            : "";

    int port =
        profile &&
        profile->configured
            ? profile->port
            : 7125;

    strlcpy(
        s_editor_api_key,
        profile && profile->configured ? profile->api_key : "",
        sizeof(s_editor_api_key));
    strlcpy(s_editor_camera_url, profile && profile->configured ? profile->camera_stream_url : "", sizeof(s_editor_camera_url));
    s_editor_camera_slot = 0;

    char port_text[16];

    snprintf(
        port_text,
        sizeof(port_text),
        "%d",
        port);

    s_editor_profile =
        s_selected_profile;
    s_editor_secure = profile && profile->secure_transport;

    s_editor_popup = ui_popup_create_on_screen(profiles_width(800), profiles_height(560), UI_POPUP_STANDARD);
    if (!s_editor_popup) { s_editor_profile = -1; profiles_clear_secret(&snapshot,sizeof(snapshot)); return; }
    ++s_profile_epoch;
    lv_obj_add_event_cb(s_editor_popup, profiles_dialog_deleted, LV_EVENT_DELETE, NULL);
    lv_obj_t *body = profiles_detail_body(s_editor_popup, profile && profile->configured ? "Edit printer" : "Add printer");
    /* Cull the popup/backdrop below scroll damage, as in the theme chooser. */
    lv_obj_set_style_bg_color(body, lv_obj_get_style_bg_color(s_editor_popup, 0), 0);
    lv_obj_set_style_bg_opa(body, LV_OPA_COVER, 0);
    lv_obj_t *identity=profiles_form_group(body);
    s_editor_name = profiles_field(identity, "Printer name", name, MOONRAKER_CONFIG_NAME_LENGTH - 1, NULL);

    lv_obj_t *connection=profiles_footer(body);
    lv_obj_clear_flag(connection,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *host_group=profiles_form_group(connection);
    lv_obj_set_width(host_group,0);lv_obj_set_flex_grow(host_group,1);
    lv_obj_set_style_min_width(host_group,240,0);
    lv_obj_t *port_group=profiles_form_group(connection);
    lv_obj_set_width(port_group,128);lv_obj_set_style_min_width(port_group,128,0);
    s_editor_host = profiles_field(host_group, "Moonraker host / IP", host, MOONRAKER_CONFIG_HOST_LENGTH - 1, NULL);
    s_editor_port = profiles_field(port_group, "Port", port_text, 5, "0123456789");
    profiles_clear_secret(&snapshot,sizeof(snapshot));
    s_editor_discover_action=profiles_action(body, UI_POPUP_ACTION_SECONDARY, LV_SYMBOL_REFRESH " Discover printers", editor_discover_cb);

    lv_obj_t *security=profiles_form_group(body);
    profiles_label(security, "Security & access", true);
    lv_obj_t *security_actions=profiles_footer(security);
    lv_obj_clear_flag(security_actions,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);
    s_security_opener=profiles_action(security_actions, UI_POPUP_ACTION_SECONDARY, LV_SYMBOL_SETTINGS " Connection security", editor_security_open_cb);
    s_editor_auth_action=profiles_action(security_actions, UI_POPUP_ACTION_SECONDARY, LV_SYMBOL_SETTINGS " API key", editor_auth_open_cb);
    s_editor_status = profiles_label(body, "Set the connection, then test and save.", true);
    lv_obj_set_style_text_line_space(s_editor_status,4,0);
    if (s_editor_name) {
        lv_obj_add_event_cb(
            s_editor_name,
            editor_field_focused_cb,
            LV_EVENT_CLICKED,
            NULL);
    }

    if (s_editor_host) {
        lv_obj_add_event_cb(
            s_editor_host,
            editor_field_focused_cb,
            LV_EVENT_CLICKED,
            NULL);
    }

    if (s_editor_port) {
        lv_obj_add_event_cb(
            s_editor_port,
            editor_field_focused_cb,
            LV_EVENT_CLICKED,
            NULL);
    }

    lv_obj_t *footer = profiles_footer(s_editor_popup);
    s_editor_cancel_action=profiles_action(footer, UI_POPUP_ACTION_CANCEL, LV_SYMBOL_CLOSE " Cancel", editor_cancel_cb);
    s_editor_camera_action=profiles_action(footer, UI_POPUP_ACTION_SECONDARY, LV_SYMBOL_IMAGE " Camera", editor_camera_open_cb);
    s_editor_test_action=profiles_action(footer, UI_POPUP_ACTION_SECONDARY, LV_SYMBOL_PLAY " Test connection", editor_test_cb);
    s_editor_save_action=profiles_action(footer, UI_POPUP_ACTION_CONFIRM, LV_SYMBOL_SAVE " Save", editor_save_cb);

    lv_obj_move_foreground(
        s_editor_popup);
}


static void manager_edit_action_cb(lv_event_t *event)
{
    if(!manager_action_current(event,s_manager_edit_action) || s_delete_popup)return;
    manager_edit_cb(NULL);
}


void ui_printer_profiles_show(
    ui_printer_profiles_active_changed_cb_t active_changed_cb,
    ui_printer_profiles_discover_cb_t discover_cb)
{
    if(s_manager_popup){lv_obj_move_foreground(s_manager_popup);return;}
    s_active_changed_cb =
        active_changed_cb;

    s_discover_cb =
        discover_cb;

    if (s_manager_popup) {
        lv_obj_move_foreground(
            s_manager_popup);
        return;
    }

    s_selected_profile =
        moonraker_config_active_profile_index();

    s_manager_popup =
        ui_popup_create_on_screen(
            profiles_width(760),
            profiles_height(520),
            UI_POPUP_STANDARD);

    if (!s_manager_popup) {
        return;
    }

    lv_obj_add_event_cb(s_manager_popup, profiles_dialog_deleted, LV_EVENT_DELETE, NULL);
    lv_obj_t *body = profiles_body(s_manager_popup, "Manage printers");
    s_manager_list = lv_obj_create(body);
    profiles_watch(s_manager_list);
    lv_obj_remove_style_all(s_manager_list);
    lv_obj_set_size(s_manager_list, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(s_manager_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_manager_list, 0, 0);
    s_manager_status = profiles_label(body, "Select a printer to edit or use.", true);
    lv_obj_t *footer = profiles_footer(s_manager_popup);
    s_manager_close_action=profiles_action(footer, UI_POPUP_ACTION_CLOSE, LV_SYMBOL_CLOSE " Close", manager_close_cb);
    s_manager_remove_action=profiles_action(footer, UI_POPUP_ACTION_DANGER, LV_SYMBOL_TRASH " Remove", manager_delete_cb);
    s_manager_edit_action=profiles_action(footer, UI_POPUP_ACTION_SECONDARY, LV_SYMBOL_EDIT " Edit printer", manager_edit_action_cb);
    s_manager_select_action=profiles_action(footer, UI_POPUP_ACTION_CONFIRM, LV_SYMBOL_OK " Use printer", manager_select_cb);

    manager_rebuild_rows();

    const moonraker_profile_t *active =
        moonraker_config_profile(
            moonraker_config_active_profile_index());

    if (active) {
        char status[160];

        snprintf(
            status,
            sizeof(status),
            "Active printer: %s",
            active->name);

        manager_set_status(status);
    }

    lv_obj_move_foreground(
        s_manager_popup);
}


void ui_printer_profiles_show_for_slot(
    int profile_index,
    ui_printer_profiles_active_changed_cb_t active_changed_cb,
    ui_printer_profiles_discover_cb_t discover_cb)
{
    ui_printer_profiles_show(active_changed_cb, discover_cb);

    if (!s_manager_popup) {
        return;
    }

    if (profile_index < 0 ||
        profile_index >= MOONRAKER_CONFIG_MAX_PROFILES) {
        return;
    }

    s_selected_profile = profile_index;
    manager_rebuild_rows();

    const moonraker_profile_t *profile =
        moonraker_config_profile(profile_index);

    if (!profile || !profile->configured) {
        /* Empty chooser cards should not make an operator select the slot
         * a second time before entering the profile details. */
        manager_edit_cb(NULL);
        return;
    }

    char status[160];
    snprintf(
        status,
        sizeof(status),
        "Selected: %s",
        profile->name);
    manager_set_status(status);
}


void ui_printer_profiles_open_camera_setup(
    int profile_index,
    ui_printer_profiles_active_changed_cb_t active_changed_cb,
    ui_printer_profiles_discover_cb_t discover_cb)
{
    (void)active_changed_cb;(void)discover_cb;
    if(s_camera_setup_opening || s_profile_dispatching || s_profile_discovery_dispatching ||
        profile_index<0 || profile_index>=MOONRAKER_CONFIG_MAX_PROFILES)return;
    s_camera_setup_opening=true;
    uint32_t generation=moonraker_config_generation();
    int active=moonraker_config_active_profile_index();
    ui_printer_profiles_close_all();
    uint32_t epoch=s_profile_epoch;
    moonraker_profile_t snapshot={0};
#define SETUP_CURRENT() (!s_manager_popup && !s_editor_popup && !s_editor_camera_popup && \
    epoch==s_profile_epoch && generation==moonraker_config_generation() && \
    active==moonraker_config_active_profile_index())
    if(!SETUP_CURRENT())goto finished;
    const moonraker_profile_t *profile=moonraker_config_profile(profile_index);
    if(!SETUP_CURRENT() || !profile)goto finished;
    snapshot=*profile;
    char names[CAMERA_CATALOG_MAX_CAMERAS][CAMERA_CATALOG_NAME_LENGTH]={0};
    for(size_t i=0;i<CAMERA_CATALOG_MAX_CAMERAS;++i){
        camera_catalog_entry_t entry={0};
        (void)camera_catalog_get(profile_index,i,&entry);
        if(!SETUP_CURRENT())goto finished;
        if(entry.name[0])strlcpy(names[i],entry.name,sizeof(names[i]));
        else snprintf(names[i],sizeof(names[i]),"Camera %u",(unsigned)(i+1));
    }
    memcpy(s_editor_camera_names,names,sizeof(names));
    s_editor_profile=profile_index;s_editor_camera_slot=0;
    s_editor_secure=snapshot.secure_transport;
    strlcpy(s_editor_api_key,snapshot.configured?snapshot.api_key:"",sizeof(s_editor_api_key));
    strlcpy(s_camera_setup_host,snapshot.configured?snapshot.host:"",sizeof(s_camera_setup_host));
    snprintf(s_camera_setup_port,sizeof(s_camera_setup_port),"%u",
        (unsigned)(snapshot.configured?snapshot.port:7125));
    strlcpy(s_editor_camera_url,snapshot.configured?snapshot.camera_stream_url:"",sizeof(s_editor_camera_url));
    s_camera_standalone=true;
    ++s_profile_epoch;
    editor_camera_open();
    if(!s_editor_camera_popup)editor_camera_close();
finished:
    profiles_clear_secret(&snapshot,sizeof(snapshot));
    s_camera_setup_opening=false;
#undef SETUP_CURRENT
}
