#include "ui_widgets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ui_network_tools.h"
#include "ui_text.h"
#include "ui_button.h"
#include "ui_theme.h"
#include "ui_popup.h"
#include "network_wifi_scan.h"
#include "ui_form_dialog.h"
#include "ui_value_update.h"

/* Internally-owned WiFi popup objects. */
static lv_obj_t *s_wifi_scan_popup = NULL;
static lv_obj_t *s_wifi_scan_status_label = NULL;
static lv_obj_t *s_wifi_scan_list = NULL;
static lv_obj_t *s_wifi_selected_btn = NULL;
static lv_obj_t *s_wifi_password_popup = NULL;
static lv_obj_t *s_wifi_password_textarea = NULL;
static lv_obj_t *s_wifi_password_status = NULL;
static lv_event_cb_t s_wifi_password_save;
static bool s_wifi_password_dispatching;
static void owned_password_save(lv_event_t *event)
{
    if (!s_wifi_password_popup || !s_wifi_password_textarea || !s_wifi_password_save || s_wifi_password_dispatching) return;
    lv_event_cb_t save=s_wifi_password_save;
    s_wifi_password_dispatching=true;
    save(event);
    s_wifi_password_dispatching=false;
}

typedef struct {
    lv_obj_t **popup, **first, **second;
    lv_obj_t *keyboard;
    unsigned references;
    bool owned_scan, owned_password;
} network_form_owner_t;
static void network_field_deleted(lv_event_t *event)
{
    network_form_owner_t *owner = lv_event_get_user_data(event);
    lv_obj_t *target = lv_event_get_target_obj(event);
    if (owner->first && *owner->first == target) {
        *owner->first = NULL;
        if (owner->keyboard) lv_keyboard_set_textarea(owner->keyboard, NULL);
    }
    if (owner->second && *owner->second == target) *owner->second = NULL;
    if (target == owner->keyboard) owner->keyboard = NULL;
    if (target == s_wifi_password_status) s_wifi_password_status = NULL;
    if (--owner->references == 0) free(owner);
}
static void network_field_own(network_form_owner_t *owner, lv_obj_t *field)
{
    ++owner->references;
    lv_obj_add_event_cb(field, network_field_deleted, LV_EVENT_DELETE, owner);
}
static void network_form_deleted(lv_event_t *event)
{
    network_form_owner_t *owner=lv_event_get_user_data(event);
    if(*owner->popup==lv_event_get_target_obj(event)) {
        *owner->popup=NULL;
        if(owner->first)*owner->first=NULL;
        if(owner->second)*owner->second=NULL;
        if(owner->owned_scan){s_wifi_selected_btn=NULL;ui_network_tools_wifi_password_close_owned();}
        if(owner->owned_password){s_wifi_password_status=NULL;s_wifi_password_save=NULL;}
    }
    if (--owner->references == 0) free(owner);
}
static network_form_owner_t *network_form_own(lv_obj_t **popup,lv_obj_t **first,lv_obj_t **second)
{
    ui_form_divide_title(*popup);
    lv_obj_set_style_pad_row(*popup,8,0);
    network_form_owner_t *owner=calloc(1,sizeof(*owner));
    if(!owner){lv_obj_delete(*popup);*popup=NULL;return NULL;}
    *owner=(network_form_owner_t){.popup=popup,.first=first,.second=second,.references=1,
        .owned_scan=popup==&s_wifi_scan_popup,.owned_password=popup==&s_wifi_password_popup};
    lv_obj_add_event_cb(*popup,network_form_deleted,LV_EVENT_DELETE,owner);return owner;
}
void ui_network_tools_wifi_password_set_status(const char *status)
{
    if(s_wifi_password_status)ui_value_set_text(s_wifi_password_status,status?status:"");
}
static void wifi_row_deleted(lv_event_t *event){if(lv_event_get_target_obj(event)==s_wifi_selected_btn)s_wifi_selected_btn=NULL;free(lv_event_get_user_data(event));}


void ui_network_tools_wifi_scan_show_owned(
    const char *status_text,
    lv_event_cb_t close_cb)
{
    ui_network_tools_show_wifi_scan_popup(
        &s_wifi_scan_popup,
        &s_wifi_scan_status_label,
        &s_wifi_scan_list,
        status_text,
        close_cb);
}

void ui_network_tools_wifi_scan_close_owned(void)
{
    ui_network_tools_close_wifi_scan_popup(
        &s_wifi_scan_popup,
        &s_wifi_scan_status_label,
        &s_wifi_scan_list,
        &s_wifi_selected_btn,
        &s_wifi_password_popup,
        &s_wifi_password_textarea);
}

void ui_network_tools_wifi_password_show_owned(
    const char *selected_ssid,
    lv_event_cb_t close_cb,
    lv_event_cb_t save_cb)
{
    if(s_wifi_password_popup){lv_obj_move_foreground(s_wifi_password_popup);return;}
    s_wifi_password_save=save_cb;
    ui_network_tools_show_wifi_password_popup_window(
        &s_wifi_password_popup,
        &s_wifi_password_textarea,
        selected_ssid,
        close_cb,
        owned_password_save);
}

void ui_network_tools_wifi_password_close_owned(void)
{
    ui_network_tools_close_wifi_password_popup(
        &s_wifi_password_popup,
        &s_wifi_password_textarea);
}

void ui_network_tools_wifi_password_copy_owned(
    char *password_buf,
    size_t password_buf_size)
{
    ui_network_tools_copy_wifi_password_text(
        s_wifi_password_textarea,
        password_buf,
        password_buf_size);
}

void ui_network_tools_wifi_select_owned(
    lv_obj_t *btn,
    const char *ssid,
    char *selected_ssid,
    size_t selected_ssid_size,
    char *status_buf,
    size_t status_buf_size)
{
    ui_network_tools_select_wifi_ssid(
        btn,
        &s_wifi_selected_btn,
        ssid,
        selected_ssid,
        selected_ssid_size,
        s_wifi_scan_status_label,
        status_buf,
        status_buf_size);
}

void ui_network_tools_wifi_scan_set_status(
    const char *status_text)
{
    if (s_wifi_scan_status_label) {
        ui_value_set_text(
            s_wifi_scan_status_label,
            status_text ? status_text : ui_text(""));
    }
}

void ui_network_tools_wifi_scan_render_owned(
    char *status_buf,
    size_t status_buf_size,
    const wifi_ap_record_t *aps,
    uint16_t count,
    unsigned total_count,
    lv_event_cb_t close_cb,
    lv_event_cb_t selected_cb)
{
    network_wifi_scan_render_results(
        &s_wifi_scan_popup,
        &s_wifi_scan_status_label,
        &s_wifi_scan_list,
        &s_wifi_selected_btn,
        status_buf,
        status_buf_size,
        aps,
        count,
        total_count,
        close_cb,
        selected_cb);
}

void ui_network_tools_wifi_popup_destroy_all(void)
{
    ui_network_tools_wifi_scan_close_owned();
}


void ui_network_tools_clear_wifi_popup_network_buttons(lv_obj_t *list, lv_obj_t **selected_btn)
{
    if (!list) return;

    if (selected_btn) *selected_btn = NULL;
    lv_obj_clean(list);

    if (selected_btn) {
        *selected_btn = NULL;
    }
}

static void wifi_style_selected(lv_obj_t *row,bool selected)
{
    ui_popup_set_selectable_row_selected(row,selected);
    lv_obj_set_style_pad_all(row,12,0);
    lv_obj_set_style_pad_row(row,4,0);
}
void ui_network_tools_add_wifi_ssid_button(lv_obj_t *parent,const char *ssid,int rssi,int y,lv_event_cb_t selected_cb)
{
    (void)y;if(!parent||!ssid||!ssid[0])return;
    char *copy=strdup(ssid);if(!copy)return;
    lv_obj_t *button=ui_popup_add_selectable_row(parent,ssid,0,0,LV_PCT(100),64,selected_cb,copy);
    if(!button){free(copy);return;}
    lv_obj_set_height(button,LV_SIZE_CONTENT);lv_obj_set_style_min_height(button,64,0);
    lv_obj_set_flex_flow(button,LV_FLEX_FLOW_COLUMN);
    wifi_style_selected(button,false);
    lv_obj_t *name=lv_obj_get_child(button,0);
    lv_label_set_long_mode(name,LV_LABEL_LONG_WRAP);
    lv_obj_set_size(name,LV_PCT(100),LV_SIZE_CONTENT);
    lv_obj_set_style_text_align(name,LV_TEXT_ALIGN_LEFT,0);
    ui_apply_custom_label_style(name,UI_FONT_BODY,UI_TEXT_BRIGHT);
    char signal[48];snprintf(signal,sizeof(signal),"Signal: %d dBm",rssi);
    lv_obj_t *reading=ui_form_text(button,signal,UI_FONT_CAPTION,UI_TEXT_DIM);
    lv_obj_set_style_text_align(reading,LV_TEXT_ALIGN_LEFT,0);
    lv_obj_add_event_cb(button,wifi_row_deleted,LV_EVENT_DELETE,copy);
}

void ui_network_tools_show_wifi_scan_popup(lv_obj_t **popup,lv_obj_t **status_label,lv_obj_t **list,const char *status_text,lv_event_cb_t close_cb)
{
    if(!popup||!status_label||!list)return;
    if(*popup){lv_obj_move_foreground(*popup);if(*status_label)ui_value_set_text(*status_label,status_text?status_text:"");return;}
    *popup=ui_form_dialog("Wi-Fi networks",860,540,UI_POPUP_STANDARD);
    if(!*popup)return;
    network_form_owner_t *owner=network_form_own(popup,status_label,list);if(!owner)return;
    lv_obj_t *body=ui_form_body(*popup);lv_obj_clear_flag(body,LV_OBJ_FLAG_SCROLLABLE);
    *status_label=ui_form_text(body,status_text,UI_FONT_CAPTION,UI_TEXT_DIM);
    *list=lv_obj_create(body);lv_obj_remove_style_all(*list);
    lv_obj_set_size(*list,LV_PCT(100),0);lv_obj_set_flex_grow(*list,1);
    lv_obj_set_flex_flow(*list,LV_FLEX_FLOW_COLUMN);lv_obj_set_style_pad_row(*list,8,0);
    lv_obj_set_scroll_dir(*list,LV_DIR_VER);lv_obj_set_scrollbar_mode(*list,LV_SCROLLBAR_MODE_OFF);
    network_field_own(owner,*status_label);network_field_own(owner,*list);
    ui_form_action(*popup,UI_BUTTON_CLOSE,"Close",close_cb,NULL);ui_form_finish(*popup);
}

void ui_network_tools_close_wifi_scan_popup(lv_obj_t **scan_popup,
                                            lv_obj_t **scan_label,
                                            lv_obj_t **scan_list,
                                            lv_obj_t **selected_btn,
                                            lv_obj_t **password_popup,
                                            lv_obj_t **password_textarea)
{
    ui_network_tools_close_wifi_password_popup(password_popup,password_textarea);
    lv_obj_t *popup=scan_popup?*scan_popup:NULL;
    if(scan_popup)*scan_popup=NULL;
    if(scan_label)*scan_label=NULL;
    if(scan_list)*scan_list=NULL;
    if(selected_btn)*selected_btn=NULL;
    if(popup)lv_obj_delete(popup);
}


void ui_network_tools_close_wifi_password_popup(lv_obj_t **password_popup,
                                                lv_obj_t **password_textarea)
{
    lv_obj_t *popup=password_popup?*password_popup:NULL;
    if(password_popup)*password_popup=NULL;
    if(password_textarea)*password_textarea=NULL;
    if(password_popup==&s_wifi_password_popup){s_wifi_password_status=NULL;s_wifi_password_save=NULL;}
    if(popup)lv_obj_delete(popup);
}

void ui_network_tools_show_wifi_password_popup_window(lv_obj_t **password_popup,lv_obj_t **password_textarea,const char *selected_ssid,lv_event_cb_t close_cb,lv_event_cb_t save_cb)
{
    if(!password_popup||!password_textarea||!selected_ssid||!selected_ssid[0])return;
    if(*password_popup){lv_obj_move_foreground(*password_popup);return;}
    *password_popup=ui_form_dialog("Wi-Fi password",960,568,UI_POPUP_STANDARD);
    if(!*password_popup)return;
    network_form_owner_t *owner=network_form_own(password_popup,password_textarea,NULL);if(!owner)return;
    lv_obj_t *body=ui_form_body(*password_popup);
    ui_form_text(body,selected_ssid,UI_FONT_BODY_LARGE,UI_TEXT_BRIGHT);
    *password_textarea=ui_form_input(body,"Wi-Fi password; leave blank for an open network",NULL,true,64);
    lv_obj_t *status=ui_form_text(body,"Connect requests a network change. Connection status remains on Network.",UI_FONT_CAPTION,UI_TEXT_DIM);
    if(password_popup==&s_wifi_password_popup)s_wifi_password_status=status;
    lv_obj_t *keyboard=ui_form_keyboard(*password_popup,*password_textarea,LV_KEYBOARD_MODE_TEXT_LOWER);
    owner->keyboard=keyboard;
    network_field_own(owner,*password_textarea);network_field_own(owner,keyboard);network_field_own(owner,status);
    ui_popup_keyboard_actions(keyboard,save_cb,close_cb);
    ui_form_action(*password_popup,UI_BUTTON_CANCEL,"Back",close_cb,NULL);
    ui_form_action(*password_popup,UI_BUTTON_PRIMARY,"Connect",save_cb,NULL);
    ui_form_finish(*password_popup);
    lv_obj_scroll_to_view(*password_textarea,LV_ANIM_OFF);
}


void ui_network_tools_copy_wifi_password_text(lv_obj_t *password_textarea,
                                               char *password_buf,
                                               size_t password_buf_size)
{
    if (!password_buf || password_buf_size == 0) return;

    const char *pw = "";
    if (password_textarea) {
        const char *txt = lv_textarea_get_text(password_textarea);
        if (txt) pw = txt;
    }

    snprintf(password_buf, password_buf_size, "%s", pw);
}


void ui_network_tools_select_wifi_ssid(lv_obj_t *btn,
                                      lv_obj_t **selected_btn,
                                      const char *ssid,
                                      char *selected_ssid,
                                      size_t selected_ssid_size,
                                      lv_obj_t *status_label,
                                      char *status_buf,
                                      size_t status_buf_size)
{
    if (!ssid || !selected_ssid || selected_ssid_size == 0) return;

    snprintf(selected_ssid, selected_ssid_size, "%s", ssid);

    if (selected_btn && *selected_btn) {
        wifi_style_selected(
            *selected_btn,
            false);
    }

    if (selected_btn) {
        *selected_btn = btn;
    }

    if (selected_btn && *selected_btn) {
        wifi_style_selected(
            *selected_btn,
            true);
    }

    if (status_buf && status_buf_size > 0) {
        snprintf(status_buf, status_buf_size,
                 "Selected network: %s",
                 selected_ssid);
    }

    if (status_label && status_buf) {
        lv_label_set_text(status_label, status_buf);
    }
}


void ui_network_tools_populate_wifi_scan_buttons(lv_obj_t *list,
                                                lv_obj_t *fallback_parent,
                                                const wifi_ap_record_t *aps,
                                                uint16_t count,
                                                lv_event_cb_t selected_cb)
{
    if (!aps || !selected_cb) return;

    lv_obj_t *parent = list ? list : fallback_parent;
    if (!parent) return;

    int y = 0;
    for (uint16_t i = 0; i < count; i++) {
        const char *ssid = aps[i].ssid[0] ? (char *)aps[i].ssid : "(hidden)";
        ui_network_tools_add_wifi_ssid_button(parent, ssid, aps[i].rssi, y, selected_cb);
        y += 58;
    }
}

/* Moonraker port edit popup */
static lv_obj_t *s_moon_port_popup = NULL;
static lv_obj_t *s_moon_port_ta = NULL;
static ui_network_port_save_cb_t s_save_cb = NULL;
static lv_obj_t *s_port_status, *s_port_keyboard;
static bool s_port_dispatching;
static uint32_t s_port_epoch;
static void port_field_deleted(lv_event_t *event)
{
    lv_obj_t *target=lv_event_get_target_obj(event);
    if(target==s_moon_port_ta){s_moon_port_ta=NULL;if(s_port_keyboard)lv_keyboard_set_textarea(s_port_keyboard,NULL);}
    else if(target==s_port_keyboard)s_port_keyboard=NULL;
    else if(target==s_port_status)s_port_status=NULL;
}
static void port_deleted(lv_event_t *event)
{
    if(lv_event_get_target_obj(event)!=s_moon_port_popup)return;
    ++s_port_epoch;s_moon_port_popup=NULL;s_moon_port_ta=NULL;s_save_cb=NULL;s_port_status=NULL;s_port_keyboard=NULL;
}

void ui_network_port_popup_close(void)
{
    lv_obj_t *popup=s_moon_port_popup;
    ++s_port_epoch;
    s_moon_port_popup=s_moon_port_ta=s_port_status=s_port_keyboard=NULL;s_save_cb=NULL;
    if(popup)lv_obj_delete(popup);
}

static void close_cb(lv_event_t *e)
{
    (void)e;
    ui_network_port_popup_close();
}

static void save_cb(lv_event_t *e)
{
    (void)e;if(!s_moon_port_popup||!s_moon_port_ta||s_port_dispatching)return;
    const char *text=lv_textarea_get_text(s_moon_port_ta);char *end=NULL;
    long port=strtol(text?text:"",&end,10);
    if(!text||!text[0]||!end||*end||port<1||port>65535) {
        ui_value_set_text(s_port_status,"Enter a port from 1 to 65535.");return;
    }
    ui_network_port_save_cb_t saved=s_save_cb;
    uint32_t epoch=s_port_epoch;
    s_port_dispatching=true;
    ui_network_port_popup_close();if(s_port_epoch==epoch+1 && saved)saved((int)port);
    s_port_dispatching=false;
}

void ui_network_port_popup_show(int current_port,ui_network_port_save_cb_t saved)
{
    if(s_moon_port_popup){lv_obj_move_foreground(s_moon_port_popup);return;}
    s_moon_port_popup=ui_form_dialog("Moonraker port",680,540,UI_POPUP_STANDARD);
    if(!s_moon_port_popup)return;
    ui_form_divide_title(s_moon_port_popup);
    lv_obj_set_style_pad_row(s_moon_port_popup,8,0);
    ++s_port_epoch;s_save_cb=saved;lv_obj_add_event_cb(s_moon_port_popup,port_deleted,LV_EVENT_DELETE,NULL);
    lv_obj_t *body=ui_form_body(s_moon_port_popup);char value[16];snprintf(value,sizeof(value),"%d",current_port);
    s_moon_port_ta=ui_form_input(body,"Port 1 to 65535",value,false,5);
    lv_textarea_set_accepted_chars(s_moon_port_ta,"0123456789");
    s_port_status=ui_form_text(body,"Use the Moonraker port configured on this printer.",UI_FONT_CAPTION,UI_TEXT_DIM);
    lv_obj_t *keyboard=ui_form_keyboard(s_moon_port_popup,s_moon_port_ta,LV_KEYBOARD_MODE_NUMBER);
    s_port_keyboard=keyboard;
    lv_obj_t *fields[]={s_moon_port_ta,s_port_status,s_port_keyboard};
    for(size_t i=0;i<3;i++)lv_obj_add_event_cb(fields[i],port_field_deleted,LV_EVENT_DELETE,NULL);
    ui_popup_keyboard_actions(keyboard,save_cb,close_cb);
    ui_form_action(s_moon_port_popup,UI_BUTTON_CANCEL,"Cancel",close_cb,NULL);
    ui_form_action(s_moon_port_popup,UI_BUTTON_PRIMARY,"Save",save_cb,NULL);ui_form_finish(s_moon_port_popup);
    lv_obj_scroll_to_view(s_moon_port_ta,LV_ANIM_OFF);
}

/* Moonraker test result popup */
static void close_test_popup_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    lv_obj_t *popup = ui_popup_find_owner(obj);
    if (!popup) popup = lv_obj_get_parent(obj);
    if (popup) lv_obj_delete(popup);
}

void ui_network_tools_show_test_moonraker_popup(const char *title,const char *message,bool ok)
{
    lv_obj_t *popup=ui_form_dialog(title?title:"Connection test",720,400,ok?UI_POPUP_STANDARD:UI_POPUP_DANGER);
    if(!popup)return;
    ui_form_divide_title(popup);
    lv_obj_t *result=ui_form_text(ui_form_body(popup),message,UI_FONT_BODY,UI_TEXT);
    lv_obj_set_style_text_line_space(result,4,0);
    ui_form_action(popup,UI_BUTTON_CLOSE,"Close",close_test_popup_cb,NULL);ui_form_finish(popup);
}
