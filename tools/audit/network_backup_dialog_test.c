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
static bool allowed=true,worker_allowed=true;
static unsigned restored,encrypted_restored,removed,exported,selected_calls;
static int saved_port;
static settings_backup_encrypted_worker_state_t worker_state;
static char worker_message[256]="Worker status",selected_ssid[33],scan_status[192];
static const char passphrase[]="test-passphrase-123";
bool settings_backup_export(char *s,size_t n){exported++;snprintf(s,n,"Backup exported");return allowed;}
bool settings_backup_preflight(char *s,size_t n){snprintf(s,n,"Backup verified: schema 1, 2 printer profiles.\nActive printer: Sermoon D1.\nAppearance, display and notifications included.\nPlain backups omit API keys.");return allowed;}
bool settings_backup_restore(char *s,size_t n){restored++;snprintf(s,n,allowed?"Restored; reboot required":"Restore failed; current settings retained");return allowed;}
bool settings_backup_restore_encrypted(const char *p,char *s,size_t n){assert(!strcmp(p,passphrase));encrypted_restored++;snprintf(s,n,"Encrypted restore result");return allowed;}
bool settings_backup_remove_all(char *s,size_t n){removed++;snprintf(s,n,"Removed backup files");return allowed;}
bool settings_backup_encrypted_worker_start_export(const char *p,char *s,size_t n){assert(!strcmp(p,passphrase));snprintf(s,n,"Worker busy");worker_state=SETTINGS_BACKUP_ENCRYPTED_WORKER_ENCRYPTING;return worker_allowed;}
bool settings_backup_encrypted_worker_start_verify(const char *p,char *s,size_t n){assert(!strcmp(p,passphrase));snprintf(s,n,"Worker busy");worker_state=SETTINGS_BACKUP_ENCRYPTED_WORKER_VERIFYING;return worker_allowed;}
settings_backup_encrypted_worker_state_t settings_backup_encrypted_worker_state(void){return worker_state;}
void settings_backup_encrypted_worker_status(char *s,size_t n){snprintf(s,n,"%s",worker_message);}
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
    lv_obj_send_event(row,LV_EVENT_CLICKED,NULL);assert(!strcmp(selected_ssid,(char *)aps[19].ssid));
    ui_network_tools_wifi_password_show_owned(selected_ssid,noop,noop);layout(s_wifi_password_popup);
    keyboard_layout_check(s_wifi_password_popup,lv_obj_get_child(s_wifi_password_popup,2),s_wifi_password_textarea);
    assert(lv_textarea_get_password_mode(s_wifi_password_textarea));assert(lv_textarea_get_max_length(s_wifi_password_textarea)==64);
    lv_textarea_set_text(s_wifi_password_textarea,"wifi-secret");ui_network_tools_wifi_password_set_status("Connection startup failed. Try again.");
    char copy[65];ui_network_tools_wifi_password_copy_owned(copy,sizeof(copy));assert(!strcmp(copy,"wifi-secret"));
    assert(strstr(lv_label_get_text(s_wifi_password_status),"failed"));
    lv_obj_delete(s_wifi_scan_popup);assert(!s_wifi_scan_popup&&!s_wifi_scan_list&&!s_wifi_selected_btn&&!s_wifi_password_popup&&!s_wifi_password_textarea&&!s_wifi_password_status);
    ui_network_port_popup_show(7125,port_saved);layout(s_moon_port_popup);saved_port=0;
    lv_textarea_set_text(s_moon_port_ta,"0");lv_obj_send_event(lv_obj_get_child(s_moon_port_popup,2),LV_EVENT_READY,NULL);assert(s_moon_port_popup&&!saved_port&&strstr(lv_label_get_text(s_port_status),"65535"));
    lv_textarea_set_text(s_moon_port_ta,"65536");lv_obj_send_event(lv_obj_get_child(s_moon_port_popup,2),LV_EVENT_READY,NULL);assert(s_moon_port_popup&&!saved_port);
    lv_textarea_set_text(s_moon_port_ta,"7126");network_save_cb(NULL);assert(!s_moon_port_popup&&saved_port==7126);
    ui_network_port_popup_show(7125,port_saved);lv_obj_delete(s_moon_port_popup);assert(!s_moon_port_ta&&!s_save_cb);
    ui_network_tools_show_test_moonraker_popup("CONNECTION TEST FAILED","Complete connection error and troubleshooting details remain reachable with large text in a narrow viewport.",false);
    lv_obj_t *popup=lv_obj_get_child(lv_layer_top(),-1);layout(popup);
    lv_obj_send_event(lv_obj_get_child(lv_obj_get_child(popup,-1),0),LV_EVENT_CLICKED,NULL);
}
static void backup(void)
{
    allowed=true;worker_allowed=true;ui_settings_backup_popup_show();layout(s_popup);
    lv_obj_t *actions=lv_obj_get_child(ui_form_body(s_popup),-1);
    lv_obj_update_layout(s_popup);
    for(unsigned i=0;i<lv_obj_get_child_count(actions);i++) {
        lv_obj_t *button=lv_obj_get_child(actions,i),*label=lv_obj_get_child(button,0);
        inside(button,actions);inside(label,button);
        assert(lv_obj_get_height(label)==lv_obj_get_style_text_font(label,0)->line_height);
        lv_point_t natural;lv_text_get_size(&natural,lv_label_get_text(label),lv_obj_get_style_text_font(label,0),0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
        assert(natural.x<=lv_obj_get_width(label));
    }
    unsigned before=restored;allowed=false;restore_request_cb(NULL);assert(!s_restore_confirmation&&restored==before);
    allowed=true;restore_request_cb(NULL);layout(s_restore_confirmation);assert(restored==before);
    allowed=false;restore_confirm_cb(NULL);assert(s_restore_confirmation&&strstr(lv_label_get_text(s_restore_status),"failed"));
    allowed=true;restore_confirm_cb(NULL);assert(!s_restore_confirmation&&restored==before+2);
    show_secret_popup(false);layout(s_secret_popup);keyboard_layout_check(s_secret_popup,s_secret_keyboard,s_secret_input);keyboard_layout_check(s_secret_popup,s_secret_keyboard,s_secret_confirm);lv_obj_send_event(s_secret_keyboard,LV_EVENT_READY,NULL);assert(s_secret_popup&&strstr(lv_label_get_text(s_secret_status),"12 characters"));
    lv_textarea_set_text(s_secret_input,passphrase);lv_textarea_set_text(s_secret_confirm,"different");secret_submit_cb(NULL);assert(s_secret_popup&&strstr(lv_label_get_text(s_secret_status),"match"));
    lv_textarea_set_text(s_secret_confirm,passphrase);worker_allowed=false;secret_submit_cb(NULL);assert(s_secret_popup&&strstr(lv_label_get_text(s_secret_status),"busy"));
    worker_allowed=true;secret_submit_cb(NULL);assert(!s_secret_popup&&s_export_progress_popup);layout(s_export_progress_popup);
    worker_state=SETTINGS_BACKUP_ENCRYPTED_WORKER_SUCCEEDED;export_progress_timer_cb(NULL);assert(!s_export_progress_popup&&!s_export_progress_timer);
    show_secret_popup(true);layout(s_secret_popup);lv_textarea_set_text(s_secret_input,passphrase);secret_submit_cb(NULL);
    assert(!s_secret_popup&&s_export_progress_popup&&!strcmp(s_restore_passphrase,passphrase));
    worker_state=SETTINGS_BACKUP_ENCRYPTED_WORKER_SUCCEEDED;export_progress_timer_cb(NULL);
    assert(s_restore_confirmation&&s_pending_encrypted_restore&&!strcmp(s_restore_passphrase,passphrase));layout(s_restore_confirmation);
    before=encrypted_restored;restore_cancel_cb(NULL);assert(encrypted_restored==before&&!s_restore_passphrase[0]);
    show_secret_popup(true);lv_textarea_set_text(s_secret_input,passphrase);secret_submit_cb(NULL);
    worker_state=SETTINGS_BACKUP_ENCRYPTED_WORKER_SUCCEEDED;export_progress_timer_cb(NULL);
    allowed=false;before=encrypted_restored;restore_confirm_cb(NULL);assert(s_restore_confirmation&&encrypted_restored==before+1&&!strcmp(s_restore_passphrase,passphrase));
    allowed=true;restore_confirm_cb(NULL);assert(!s_restore_confirmation&&encrypted_restored==before+2&&!s_restore_passphrase[0]);
    show_secret_popup(true);lv_textarea_set_text(s_secret_input,passphrase);secret_submit_cb(NULL);
    worker_state=SETTINGS_BACKUP_ENCRYPTED_WORKER_FAILED;export_progress_timer_cb(NULL);assert(!s_export_progress_popup&&!s_restore_passphrase[0]);
    show_secret_popup(true);lv_textarea_set_text(s_secret_input,passphrase);secret_submit_cb(NULL);lv_obj_delete(s_export_progress_popup);
    assert(!s_export_progress_timer&&!s_restore_passphrase[0]);
    remove_request_cb(NULL);layout(s_remove_confirmation);before=removed;remove_cancel_cb(NULL);assert(removed==before);
    remove_request_cb(NULL);remove_confirm_cb(NULL);assert(removed==before+1&&!s_remove_confirmation);
    show_secret_popup(false);lv_obj_delete(s_popup);assert(!s_popup&&!s_status&&!s_secret_popup&&!s_secret_input&&!s_secret_keyboard);
    ui_settings_backup_popup_show();ui_settings_backup_popup_close();assert(!s_popup);
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
    lv_display_delete(display);lv_deinit();puts("PASS: Network/backup five themes, densities/text sizes/viewports, full SSIDs, pinned keyboards/actions, inline validation/retry, restore review, worker ownership and deletion cleanup");return 0;
}
