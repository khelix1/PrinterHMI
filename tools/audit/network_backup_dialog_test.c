#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "custom_theme.h"
#define close_cb network_close_cb
#define save_cb network_save_cb
#include "ui_network_tools.c"
#include "keyboard_layout_checks.h"
#undef close_cb
#undef save_cb
#include "network_wifi_scan.c"
#include "ui_settings_backup_popup.c"
bool custom_theme_color_override(uint32_t a,uint32_t b,uint32_t c,uint32_t*d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_metric_override(int32_t a,int32_t b,int32_t c,int32_t*d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_accent_override(uint8_t a,uint32_t*b){(void)a;(void)b;return false;}
bool custom_theme_surface_opacity(uint8_t*a){(void)a;return false;}
static uint16_t raster[1024*600];
static unsigned render_frame;
static void (*backup_hook)(void);
static unsigned worker_starts;
static void run_backup_hook(void){if(backup_hook){void(*hook)(void)=backup_hook;backup_hook=NULL;hook();}}
static bool allowed=true,worker_allowed=true;
static unsigned restored,encrypted_restored,removed,exported,selected_calls;
static int saved_port;
static settings_backup_encrypted_worker_state_t worker_state;
static char worker_message[256]="Worker status",selected_ssid[33],scan_status[192];
static const char passphrase[]="test-passphrase-123";
bool settings_backup_export(char *s,size_t n){exported++;run_backup_hook();snprintf(s,n,"Backup exported");return allowed;}
bool settings_backup_preflight(char *s,size_t n){snprintf(s,n,"Backup verified: schema 1, 2 printer profiles.\nActive printer: Sermoon D1.\nAppearance, display and notifications included.\nPlain backups omit API keys.");return allowed;}
bool settings_backup_restore(char *s,size_t n){restored++;run_backup_hook();snprintf(s,n,allowed?"Restored; reboot required":"Restore failed; current settings retained");return allowed;}
bool settings_backup_restore_encrypted(const char *p,char *s,size_t n){assert(!strcmp(p,passphrase));encrypted_restored++;run_backup_hook();snprintf(s,n,"Encrypted restore result");return allowed;}
bool settings_backup_remove_all(char *s,size_t n){removed++;run_backup_hook();snprintf(s,n,allowed?"Removed backup files":"Remove failed; retry available");return allowed;}
bool settings_backup_encrypted_worker_start_export(const char *p,char *s,size_t n){assert(!strcmp(p,passphrase));snprintf(s,n,"Worker busy");worker_starts++;run_backup_hook();if(worker_allowed)worker_state=SETTINGS_BACKUP_ENCRYPTED_WORKER_ENCRYPTING;return worker_allowed;}
bool settings_backup_encrypted_worker_start_verify(const char *p,char *s,size_t n){assert(!strcmp(p,passphrase));snprintf(s,n,"Worker busy");worker_starts++;run_backup_hook();if(worker_allowed)worker_state=SETTINGS_BACKUP_ENCRYPTED_WORKER_VERIFYING;return worker_allowed;}
settings_backup_encrypted_worker_state_t settings_backup_encrypted_worker_state(void){return worker_state;}
void settings_backup_encrypted_worker_status(char *s,size_t n){snprintf(s,n,"%s",worker_message);}
static void mock_stage(settings_backup_progress_cb_t cb,void *context,settings_backup_stage_t stage)
{
    assert(s_export_progress_popup&&s_export_progress_bar&&s_progress_close);
    assert(lv_obj_has_state(s_progress_close,LV_STATE_DISABLED));
    cb(stage,context);
    assert(lv_bar_get_value(s_export_progress_bar)>0&&strstr(lv_label_get_text(s_export_progress_value),"Working"));
}
bool settings_backup_export_with_progress(settings_backup_progress_cb_t cb,void *ctx,char *s,size_t n)
{
    mock_stage(cb,ctx,SETTINGS_BACKUP_STAGE_PREPARING);
    mock_stage(cb,ctx,SETTINGS_BACKUP_STAGE_WRITING);
    return settings_backup_export(s,n);
}
bool settings_backup_restore_with_progress(settings_backup_progress_cb_t cb,void *ctx,char *s,size_t n)
{
    mock_stage(cb,ctx,SETTINGS_BACKUP_STAGE_READING);
    mock_stage(cb,ctx,SETTINGS_BACKUP_STAGE_APPLYING);
    if(!allowed)mock_stage(cb,ctx,SETTINGS_BACKUP_STAGE_ROLLING_BACK);
    return settings_backup_restore(s,n);
}
bool settings_backup_restore_encrypted_with_progress(const char *p,settings_backup_progress_cb_t cb,void *ctx,char *s,size_t n)
{
    mock_stage(cb,ctx,SETTINGS_BACKUP_STAGE_AUTHENTICATING);
    mock_stage(cb,ctx,SETTINGS_BACKUP_STAGE_APPLYING);
    if(!allowed)mock_stage(cb,ctx,SETTINGS_BACKUP_STAGE_ROLLING_BACK);
    return settings_backup_restore_encrypted(p,s,n);
}
static unsigned password_saves;
static void password_nested(lv_event_t *event){password_saves++;owned_password_save(event);}
static void noop(lv_event_t *e){(void)e;}
static void port_saved(int port){assert(!s_moon_port_popup);saved_port=port;}
static void selected(lv_event_t *e){selected_calls++;ui_network_tools_wifi_select_owned(lv_event_get_target_obj(e),lv_event_get_user_data(e),selected_ssid,sizeof(selected_ssid),scan_status,sizeof(scan_status));}
static void inside(lv_obj_t *child,lv_obj_t *parent)
{lv_area_t a,b;lv_obj_get_coords(child,&a);lv_obj_get_coords(parent,&b);assert(a.x1>=b.x1&&a.x2<=b.x2&&a.y1>=b.y1&&a.y2<=b.y2);}
static void flush(lv_display_t *d,const lv_area_t *a,uint8_t *p)
{uint16_t *v=(uint16_t *)p;int width=lv_display_get_horizontal_resolution(d);for(int y=a->y1;y<=a->y2;y++)for(int x=a->x1;x<=a->x2;x++)raster[y*width+x]=*v++;lv_display_flush_ready(d);}
static void capture(void)
{
 const char *folder=getenv("NETWORK_BACKUP_SCREENSHOTS");
 if(!folder||lv_display_get_horizontal_resolution(NULL)!=1024||ui_theme_get_density()!=1||!ui_theme_get_accessibility().large_text)return;
 lv_refr_now(NULL);char path[512];snprintf(path,sizeof(path),"%s/theme%u-frame%u.ppm",folder,(unsigned)ui_theme_get_active(),render_frame++);
 FILE *file=fopen(path,"wb");assert(file);fprintf(file,"P6\n1024 600\n255\n");
 for(unsigned i=0;i<1024*600;i++){uint16_t v=raster[i];unsigned char c[3]={(v>>11)*255/31,((v>>5)&63)*255/63,(v&31)*255/31};fwrite(c,1,3,file);}fclose(file);
}
static void layout(lv_obj_t *popup)
{
    lv_obj_update_layout(popup);inside(popup,lv_screen_active());
    lv_obj_t *body=ui_form_body(popup),*footer=lv_obj_get_child(popup,-1);
    inside(body,popup);inside(footer,popup);
    if(lv_obj_get_child_count(popup)==4)inside(lv_obj_get_child(popup,2),popup);
    for(unsigned i=0;i<lv_obj_get_child_count(footer);i++) {
        lv_obj_t *button=lv_obj_get_child(footer,i),*label=lv_obj_get_child(button,0);
        inside(button,footer);inside(label,button);assert(lv_obj_get_height(button)==48);
        lv_area_t a,b;lv_obj_get_coords(button,&a);lv_obj_get_coords(label,&b);
        assert(abs(a.x1+a.x2-b.x1-b.x2)<=2&&abs(a.y1+a.y2-b.y1-b.y2)<=2);
    }
    lv_area_t before,after;lv_obj_get_coords(footer,&before);
    lv_obj_scroll_to_y(body,lv_obj_get_scroll_y(body)+lv_obj_get_scroll_bottom(body),LV_ANIM_OFF);
    lv_obj_update_layout(popup);assert(lv_obj_get_scroll_bottom(body)<=1);
    lv_obj_get_coords(footer,&after);assert(!memcmp(&before,&after,sizeof(before)));
    lv_obj_scroll_to_y(body,0,LV_ANIM_OFF);capture();
}
static void network(void)
{
    wifi_ap_record_t aps[20]={0};for(unsigned i=0;i<20;i++){snprintf((char *)aps[i].ssid,sizeof(aps[i].ssid),"Long-network-name-abcdefghij-%02u",i);aps[i].rssi=-57-(int)i;}
    ui_network_tools_wifi_scan_render_owned(scan_status,sizeof(scan_status),aps,20,20,noop,selected);
    layout(s_wifi_scan_popup);assert(lv_obj_get_child_count(s_wifi_scan_list)==20);
    lv_obj_t *row=lv_obj_get_child(s_wifi_scan_list,19);lv_obj_scroll_to_view(row,LV_ANIM_OFF);lv_obj_update_layout(row);
    inside(lv_obj_get_child(row,0),row);assert(strstr(lv_label_get_text(lv_obj_get_child(row,0)),(char *)aps[19].ssid));
    inside(lv_obj_get_child(row,1),row);assert(strstr(lv_label_get_text(lv_obj_get_child(row,1)),"dBm"));
    assert(lv_label_get_long_mode(lv_obj_get_child(row,0))==LV_LABEL_LONG_WRAP);
    lv_obj_send_event(row,LV_EVENT_CLICKED,NULL);assert(!strcmp(selected_ssid,(char *)aps[19].ssid));lv_obj_update_layout(row);
    assert(lv_obj_get_style_pad_left(row,0)==12);inside(lv_obj_get_child(row,0),row);inside(lv_obj_get_child(row,1),row);
    ui_network_tools_wifi_password_show_owned(selected_ssid,noop,noop);layout(s_wifi_password_popup);
    keyboard_layout_check(s_wifi_password_popup,lv_obj_get_child(s_wifi_password_popup,2),s_wifi_password_textarea);
    assert(lv_textarea_get_password_mode(s_wifi_password_textarea));assert(lv_textarea_get_max_length(s_wifi_password_textarea)==64);
    lv_textarea_set_text(s_wifi_password_textarea,"wifi-secret");ui_network_tools_wifi_password_set_status("Connection startup failed. Try again.");
    char copy[65];ui_network_tools_wifi_password_copy_owned(copy,sizeof(copy));assert(!strcmp(copy,"wifi-secret"));
    assert(strstr(lv_label_get_text(s_wifi_password_status),"failed"));
    lv_obj_delete(s_wifi_scan_popup);assert(!s_wifi_scan_popup&&!s_wifi_scan_list&&!s_wifi_selected_btn&&!s_wifi_password_popup&&!s_wifi_password_textarea&&!s_wifi_password_status);
    ui_network_port_popup_show(7125,port_saved);layout(s_moon_port_popup);saved_port=0;
    lv_textarea_set_text(s_moon_port_ta,"0");lv_obj_send_event(lv_obj_get_child(s_moon_port_popup,2),LV_EVENT_READY,NULL);assert(s_moon_port_popup&&!saved_port&&strstr(lv_label_get_text(s_port_status),"65535"));
    lv_obj_t *foreign=lv_textarea_create(lv_screen_active());
    lv_obj_add_event_cb(foreign,port_value_changed,LV_EVENT_VALUE_CHANGED,NULL);
    lv_textarea_set_text(foreign,"7126");assert(strstr(lv_label_get_text(s_port_status),"65535"));lv_obj_delete(foreign);
    lv_textarea_set_text(s_moon_port_ta,"65536");assert(!saved_port&&!strcmp(lv_label_get_text(s_port_status),s_port_guidance));
    lv_obj_send_event(lv_obj_get_child(s_moon_port_popup,2),LV_EVENT_READY,NULL);assert(s_moon_port_popup&&!saved_port&&strstr(lv_label_get_text(s_port_status),"65535"));
    lv_textarea_set_text(s_moon_port_ta,"7126");assert(!saved_port&&!strcmp(lv_label_get_text(s_port_status),s_port_guidance));
    network_save_cb(NULL);assert(!s_moon_port_popup&&saved_port==7126);
    ui_network_port_popup_show(7125,port_saved);saved_port=0;
    lv_textarea_set_text(s_moon_port_ta,"0");network_save_cb(NULL);assert(strstr(lv_label_get_text(s_port_status),"65535"));
    lv_textarea_set_text(s_moon_port_ta,"7127");ui_network_port_popup_close();assert(!saved_port);
    ui_network_port_popup_show(7125,port_saved);lv_obj_delete(s_port_status);assert(!s_port_status);
    lv_textarea_set_text(s_moon_port_ta,"7128");assert(!saved_port);ui_network_port_popup_close();
    ui_network_port_popup_show(7125,port_saved);lv_obj_delete(s_moon_port_ta);assert(!s_moon_port_ta&&lv_keyboard_get_textarea(s_port_keyboard)==NULL);network_save_cb(NULL);ui_network_port_popup_close();
    ui_network_port_popup_show(7125,port_saved);lv_obj_delete(s_moon_port_popup);assert(!s_moon_port_ta&&!s_save_cb);
    ui_network_tools_wifi_password_show_owned("fixture",noop,password_nested);unsigned saves=password_saves;owned_password_save(NULL);assert(password_saves==saves+1);lv_obj_t *keyboard=lv_obj_get_child(s_wifi_password_popup,2);
    lv_obj_delete(s_wifi_password_textarea);assert(!s_wifi_password_textarea&&lv_keyboard_get_textarea(keyboard)==NULL);
    owned_password_save(NULL);assert(password_saves==saves+1);ui_network_tools_wifi_password_copy_owned(copy,sizeof(copy));assert(!copy[0]);ui_network_tools_wifi_password_close_owned();
    ui_network_tools_wifi_scan_render_owned(scan_status,sizeof(scan_status),aps,2,2,noop,selected);
    lv_obj_send_event(lv_obj_get_child(s_wifi_scan_list,0),LV_EVENT_CLICKED,NULL);lv_obj_delete(s_wifi_selected_btn);assert(!s_wifi_selected_btn);
    lv_obj_delete(s_wifi_scan_status_label);assert(!s_wifi_scan_status_label);ui_network_tools_wifi_scan_show_owned("retry",noop);ui_network_tools_wifi_scan_close_owned();
    ui_network_tools_show_test_moonraker_popup("Moonraker failed","Complete connection error and troubleshooting details remain reachable with large text in a narrow viewport.",false);
    lv_obj_t *popup=lv_obj_get_child(lv_layer_top(),-1);layout(popup);
    lv_obj_send_event(lv_obj_get_child(lv_obj_get_child(popup,-1),0),LV_EVENT_CLICKED,NULL);
}
static void replace_backup(void){ui_settings_backup_popup_close();ui_settings_backup_popup_show();backup_cb(NULL);}
static void retry_plain_nested(void){backup_cb(NULL);}
static void backup(void)
{
    worker_state=SETTINGS_BACKUP_ENCRYPTED_WORKER_IDLE;allowed=true;worker_allowed=true;ui_settings_backup_popup_show();layout(s_popup);
    lv_obj_t *actions=lv_obj_get_child(ui_form_body(s_popup),-1);
    lv_obj_update_layout(s_popup);
    for(unsigned i=0;i<lv_obj_get_child_count(actions);i++) {
        lv_obj_t *button=lv_obj_get_child(actions,i),*label=lv_obj_get_child(button,0);
        inside(button,actions);inside(label,button);
        assert(lv_obj_get_height(label)==lv_obj_get_style_text_font(label,0)->line_height);
        lv_point_t natural;lv_text_get_size(&natural,lv_label_get_text(label),lv_obj_get_style_text_font(label,0),0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
        assert(natural.x<=lv_obj_get_width(label));
    }
    exported=0;backup_cb(NULL);assert(exported==1&&s_export_progress_popup&&lv_bar_get_value(s_export_progress_bar)==100);
    layout(s_export_progress_popup);progress_close_cb(NULL);assert(!s_export_progress_popup);
    allowed=false;backup_cb(NULL);assert(exported==2&&s_export_progress_popup&&lv_bar_get_value(s_export_progress_bar)<100);
    progress_close_cb(NULL);allowed=true;
    unsigned before=restored;allowed=false;restore_request_cb(NULL);assert(!s_restore_confirmation&&restored==before);
    allowed=true;restore_request_cb(NULL);layout(s_restore_confirmation);assert(restored==before);
    allowed=false;restore_confirm_cb(NULL);assert(s_restore_confirmation&&strstr(lv_label_get_text(s_restore_status),"failed"));
    assert(s_export_progress_popup&&lv_bar_get_value(s_export_progress_bar)<100&&!lv_obj_has_state(s_progress_close,LV_STATE_DISABLED));
    layout(s_export_progress_popup);progress_close_cb(NULL);assert(!s_export_progress_popup&&s_restore_confirmation);
    allowed=true;restore_confirm_cb(NULL);assert(!s_restore_confirmation&&restored==before+2);
    assert(s_export_progress_popup&&lv_bar_get_value(s_export_progress_bar)==100&&strstr(lv_label_get_text(s_export_progress_status),"reboot"));
    layout(s_export_progress_popup);progress_close_cb(NULL);assert(!s_export_progress_popup);
    show_secret_popup(false);layout(s_secret_popup);keyboard_layout_check(s_secret_popup,s_secret_keyboard,s_secret_input);keyboard_layout_check(s_secret_popup,s_secret_keyboard,s_secret_confirm);lv_obj_send_event(s_secret_keyboard,LV_EVENT_READY,NULL);assert(s_secret_popup&&strstr(lv_label_get_text(s_secret_status),"12 characters"));
    before=worker_starts;
    lv_obj_send_event(s_secret_input,LV_EVENT_CLICKED,NULL);assert(lv_keyboard_get_textarea(s_secret_keyboard)==s_secret_input);
    lv_obj_t *foreign=lv_textarea_create(lv_screen_active());
    lv_obj_add_event_cb(foreign,secret_value_changed,LV_EVENT_VALUE_CHANGED,NULL);
    lv_obj_add_event_cb(foreign,secret_focus_cb,LV_EVENT_CLICKED,NULL);
    lv_textarea_set_text(foreign,"unrelated");lv_obj_send_event(foreign,LV_EVENT_CLICKED,NULL);
    assert(strstr(lv_label_get_text(s_secret_status),"12 characters")&&lv_keyboard_get_textarea(s_secret_keyboard)==s_secret_input);lv_obj_delete(foreign);
    lv_textarea_set_text(s_secret_input,"short");assert(!lv_label_get_text(s_secret_status)[0]&&worker_starts==before);
    secret_submit_cb(NULL);assert(strstr(lv_label_get_text(s_secret_status),"12 characters"));
    lv_textarea_set_text(s_secret_input,passphrase);assert(!lv_label_get_text(s_secret_status)[0]);
    lv_textarea_set_text(s_secret_confirm,"different");secret_submit_cb(NULL);assert(s_secret_popup&&strstr(lv_label_get_text(s_secret_status),"match")&&worker_starts==before);
    lv_textarea_set_text(s_secret_input,"another-passphrase");assert(!lv_label_get_text(s_secret_status)[0]&&worker_starts==before);
    secret_submit_cb(NULL);assert(strstr(lv_label_get_text(s_secret_status),"match"));
    lv_textarea_set_text(s_secret_input,passphrase);assert(!lv_label_get_text(s_secret_status)[0]);
    secret_submit_cb(NULL);assert(strstr(lv_label_get_text(s_secret_status),"match"));
    lv_obj_send_event(s_secret_confirm,LV_EVENT_CLICKED,NULL);assert(lv_keyboard_get_textarea(s_secret_keyboard)==s_secret_confirm);
    lv_textarea_set_text(s_secret_confirm,passphrase);assert(!lv_label_get_text(s_secret_status)[0]&&worker_starts==before);
    worker_allowed=false;secret_submit_cb(NULL);assert(s_secret_popup&&strstr(lv_label_get_text(s_secret_status),"busy"));
    lv_textarea_set_text(s_secret_confirm,"different");assert(strstr(lv_label_get_text(s_secret_status),"busy"));
    lv_textarea_set_text(s_secret_confirm,passphrase);assert(strstr(lv_label_get_text(s_secret_status),"busy"));
    worker_allowed=true;secret_submit_cb(NULL);assert(!s_secret_popup&&s_export_progress_popup);layout(s_export_progress_popup);
    assert(s_progress_close&&lv_obj_has_state(s_progress_close,LV_STATE_DISABLED));
    progress_close_cb(NULL);assert(s_export_progress_popup&&s_export_progress_timer);
    snprintf(worker_message,sizeof(worker_message),"Encrypted configuration backed up to the SD card.");
    lv_timer_t *finished_timer=s_export_progress_timer;
    worker_state=SETTINGS_BACKUP_ENCRYPTED_WORKER_SUCCEEDED;export_progress_timer_cb(finished_timer);
    assert(s_export_progress_popup&&!s_export_progress_timer&&lv_bar_get_value(s_export_progress_bar)==100);
    assert(!lv_obj_has_state(s_progress_close,LV_STATE_DISABLED)&&strstr(lv_label_get_text(s_export_progress_value),"saved successfully"));
    assert(strstr(lv_label_get_text(s_export_progress_status),"Encrypted configuration backed up"));
    assert(!s_secret_input&&!s_restore_passphrase[0]);layout(s_export_progress_popup);
    lv_tick_inc(1000);lv_timer_handler();assert(s_export_progress_popup);
    export_progress_timer_cb(finished_timer);assert(s_export_progress_popup&&lv_bar_get_value(s_export_progress_bar)==100);
    progress_close_cb(NULL);assert(!s_export_progress_popup&&!s_progress_close);
    for(unsigned phase=0;phase<2;phase++){
        encrypted_backup_request_cb(NULL);assert(s_secret_popup&&!s_secret_is_restore);
        assert(!lv_textarea_get_text(s_secret_input)[0]&&!lv_textarea_get_text(s_secret_confirm)[0]);
        lv_textarea_set_text(s_secret_input,passphrase);lv_textarea_set_text(s_secret_confirm,passphrase);
        secret_submit_cb(NULL);assert(s_export_progress_popup&&s_export_progress_timer);
        worker_state=phase?SETTINGS_BACKUP_ENCRYPTED_WORKER_WRITING:SETTINGS_BACKUP_ENCRYPTED_WORKER_ENCRYPTING;
        export_progress_timer_cb(s_export_progress_timer);
        snprintf(worker_message,sizeof(worker_message),phase?"SD write failed; existing backup was preserved.":"Encryption failed. Try again.");
        finished_timer=s_export_progress_timer;worker_state=SETTINGS_BACKUP_ENCRYPTED_WORKER_FAILED;
        before=worker_starts;export_progress_timer_cb(finished_timer);
        assert(s_export_progress_popup&&!s_export_progress_timer&&lv_bar_get_value(s_export_progress_bar)<100);
        assert(!lv_obj_has_state(s_progress_close,LV_STATE_DISABLED)&&!strcmp(lv_label_get_text(s_export_progress_value),"Backup failed"));
        assert(!strcmp(lv_label_get_text(s_export_progress_status),worker_message)&&!strcmp(lv_label_get_text(s_status),worker_message));
        assert(!s_secret_popup&&!s_restore_passphrase[0]);layout(s_export_progress_popup);
        lv_tick_inc(1000);lv_timer_handler();export_progress_timer_cb(finished_timer);
        assert(s_export_progress_popup&&worker_starts==before&&!strcmp(lv_label_get_text(s_export_progress_status),worker_message));
        progress_close_cb(NULL);assert(!s_export_progress_popup&&!s_progress_close);
    }
    show_secret_popup(true);layout(s_secret_popup);before=worker_starts;
    secret_submit_cb(NULL);assert(strstr(lv_label_get_text(s_secret_status),"12 characters"));
    lv_textarea_set_text(s_secret_input,passphrase);assert(!lv_label_get_text(s_secret_status)[0]&&worker_starts==before);
    secret_submit_cb(NULL);
    assert(!s_secret_popup&&s_export_progress_popup&&!strcmp(s_restore_passphrase,passphrase));
    worker_state=SETTINGS_BACKUP_ENCRYPTED_WORKER_SUCCEEDED;export_progress_timer_cb(s_export_progress_timer);
    assert(s_restore_confirmation&&s_pending_encrypted_restore&&!strcmp(s_restore_passphrase,passphrase));layout(s_restore_confirmation);
    before=encrypted_restored;restore_cancel_cb(NULL);assert(encrypted_restored==before&&!s_restore_passphrase[0]);
    show_secret_popup(true);lv_textarea_set_text(s_secret_input,passphrase);secret_submit_cb(NULL);
    worker_state=SETTINGS_BACKUP_ENCRYPTED_WORKER_SUCCEEDED;export_progress_timer_cb(s_export_progress_timer);
    allowed=false;before=encrypted_restored;restore_confirm_cb(NULL);assert(s_restore_confirmation&&encrypted_restored==before+1&&!strcmp(s_restore_passphrase,passphrase));
    allowed=true;restore_confirm_cb(NULL);assert(!s_restore_confirmation&&encrypted_restored==before+2&&!s_restore_passphrase[0]);
    assert(s_export_progress_popup&&lv_bar_get_value(s_export_progress_bar)==100);layout(s_export_progress_popup);progress_close_cb(NULL);
    show_secret_popup(true);lv_textarea_set_text(s_secret_input,passphrase);secret_submit_cb(NULL);
    snprintf(worker_message,sizeof(worker_message),"Passphrase is incorrect or the encrypted backup was changed.");
    worker_state=SETTINGS_BACKUP_ENCRYPTED_WORKER_FAILED;export_progress_timer_cb(s_export_progress_timer);
    assert(!s_export_progress_popup&&!s_restore_passphrase[0]&&s_secret_popup&&s_secret_is_restore&&!s_restore_confirmation);
    assert(!lv_textarea_get_text(s_secret_input)[0]&&strstr(lv_label_get_text(s_secret_status),"Passphrase is incorrect"));layout(s_secret_popup);
    show_secret_popup(true);lv_textarea_set_text(s_secret_input,passphrase);secret_submit_cb(NULL);lv_obj_delete(s_export_progress_popup);
    assert(!s_export_progress_timer&&!s_restore_passphrase[0]);
    before=exported;backup_cb(NULL);assert(exported==before);
    before=removed;remove_request_cb(NULL);assert(!s_remove_confirmation&&removed==before);
    worker_state=SETTINGS_BACKUP_ENCRYPTED_WORKER_SUCCEEDED;
    remove_request_cb(NULL);layout(s_remove_confirmation);before=removed;remove_cancel_cb(NULL);assert(removed==before);
    remove_request_cb(NULL);allowed=false;remove_confirm_cb(NULL);assert(removed==before+1&&s_remove_confirmation&&strstr(lv_label_get_text(s_remove_status),"failed"));allowed=true;remove_confirm_cb(NULL);assert(removed==before+2&&!s_remove_confirmation);
    show_secret_popup(false);lv_obj_delete(s_popup);assert(!s_popup&&!s_status&&!s_secret_popup&&!s_secret_input&&!s_secret_keyboard);
    ui_settings_backup_popup_show();
    before=exported;backup_hook=retry_plain_nested;backup_cb(NULL);assert(exported==before+1);
    before=exported;backup_hook=replace_backup;backup_cb(NULL);assert(exported==before+1&&s_popup);
    assert(strstr(lv_label_get_text(s_status),"Plain backups exclude"));
    restore_request_cb(NULL);before=restored;backup_hook=replace_backup;restore_confirm_cb(NULL);assert(restored==before+1&&!s_restore_confirmation&&s_popup);
    show_secret_popup(false);secret_submit_cb(NULL);assert(s_secret_validation_error);
    lv_obj_delete(s_secret_status);assert(!s_secret_status);lv_textarea_set_text(s_secret_input,"short");
    before=worker_starts;secret_cancel_cb(NULL);assert(worker_starts==before&&!s_secret_validation_error);
    show_secret_popup(false);assert(!s_secret_validation_error&&!lv_label_get_text(s_secret_status)[0]);lv_obj_delete(s_secret_input);assert(!s_secret_input&&lv_keyboard_get_textarea(s_secret_keyboard)==NULL);
    before=worker_starts;secret_submit_cb(NULL);assert(worker_starts==before);close_secret_popup(false);
    show_secret_popup(false);lv_textarea_set_text(s_secret_input,passphrase);lv_textarea_set_text(s_secret_confirm,passphrase);
    backup_hook=replace_backup;secret_submit_cb(NULL);assert(s_popup&&!s_secret_popup&&!s_export_progress_popup&&!s_restore_passphrase[0]);
    worker_state=SETTINGS_BACKUP_ENCRYPTED_WORKER_SUCCEEDED;
    show_secret_popup(true);lv_textarea_set_text(s_secret_input,passphrase);secret_submit_cb(NULL);
    lv_timer_t *retired=s_export_progress_timer;ui_settings_backup_popup_close();ui_settings_backup_popup_show();
    export_progress_timer_cb(retired);assert(!s_restore_confirmation&&!s_restore_passphrase[0]);
    worker_state=SETTINGS_BACKUP_ENCRYPTED_WORKER_SUCCEEDED;
    ui_settings_backup_popup_close();assert(!s_popup);
}
int main(void)
{
    lv_init();lv_display_t *display=lv_display_create(1024,600);static uint8_t pixels[1024*40*2];
    lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB565);lv_display_set_buffers(display,pixels,NULL,sizeof(pixels),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(display,flush);
    for(int theme=0;theme<5;theme++)for(int density=0;density<3;density++)for(int large=0;large<2;large++)for(int viewport=0;viewport<3;viewport++) {
        lv_display_set_resolution(display,viewport==0?1024:viewport==1?640:480,viewport==2?400:600);
        ui_theme_set_active(theme);ui_theme_set_density(density);ui_theme_set_accessibility((ui_accessibility_t){.large_text=large});
        network();backup();assert(!lv_obj_get_child_count(lv_layer_top()));
    }
    lv_display_delete(display);lv_deinit();puts("PASS: Network/backup nested-dispatch and worker-overlap guards, retired inputs/keyboard detachment, callback replacement, failed removal retry, five themes, densities/text sizes/viewports, full SSIDs, pinned keyboards/actions, inline validation/retry, restore review, worker ownership and deletion cleanup");return 0;
}
