#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lvgl.h"
static unsigned live_objects(lv_obj_t *p){unsigned n=1;for(uint32_t i=0;i<lv_obj_get_child_count(p);i++)n+=live_objects(lv_obj_get_child(p,i));return n;}
static unsigned live_timers(void){unsigned n=0;lv_timer_t*t=NULL;while((t=lv_timer_get_next(t)))n++;return n;}
#include "custom_theme.h"
#include "ui_settings_popups.c"
bool custom_theme_color_override(uint32_t a,uint32_t b,uint32_t c,uint32_t *d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_metric_override(int32_t a,int32_t b,int32_t c,int32_t *d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_accent_override(uint8_t a,uint32_t *b){(void)a;(void)b;return false;}
bool custom_theme_surface_opacity(uint8_t *a){(void)a;return false;}
static void click(lv_obj_t *o){lv_obj_send_event(o,LV_EVENT_CLICKED,NULL);}
static uint16_t raster[1024*600];static int viewport_width=1024;
static void flush(lv_display_t *d,const lv_area_t *a,uint8_t *p){uint16_t *v=(uint16_t*)p;for(int y=a->y1;y<=a->y2;y++)for(int x=a->x1;x<=a->x2;x++)raster[y*viewport_width+x]=*v++;lv_display_flush_ready(d);}
static void snapshot(int theme,const char *name){const char *folder=getenv("SETTINGS_DIALOG_SCREENSHOTS");if(!folder)return;lv_refr_now(NULL);char path[512];snprintf(path,sizeof(path),"%s/%s-theme%d.ppm",folder,name,theme);FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n1024 600\n255\n");for(int i=0;i<1024*600;i++){uint16_t v=raster[i];unsigned char c[3]={(v>>11)*255/31,((v>>5)&63)*255/63,(v&31)*255/31};fwrite(c,1,3,f);}fclose(f);}
static void inside(lv_obj_t *child,lv_obj_t *parent){lv_area_t a,b;lv_obj_get_coords(child,&a);lv_obj_get_coords(parent,&b);if(!(a.x1>=b.x1&&a.x2<=b.x2&&a.y1>=b.y1&&a.y2<=b.y2)){fprintf(stderr,"Outside %d,%d..%d,%d vs %d,%d..%d,%d\n",a.x1,a.y1,a.x2,a.y2,b.x1,b.y1,b.x2,b.y2);abort();}}
static bool update_active;
static settings_backup_encrypted_worker_state_t backup_state;
static void(*controller_read_hook)(void);
bool ota_manager_is_running(void){if(controller_read_hook){void(*hook)(void)=controller_read_hook;controller_read_hook=NULL;hook();}return update_active;}
settings_backup_encrypted_worker_state_t settings_backup_encrypted_worker_state(void){return backup_state;}
static unsigned erased,restarted,delayed,changed,erase_attempts;static bool erase_allowed=true;static void(*preference_hook)(void);static bool fail_selection;static size_t selected,last_index;
esp_err_t nvs_flash_erase(void){assert(!erased&&!restarted&&!delayed);erase_attempts++;if(preference_hook){void(*hook)(void)=preference_hook;preference_hook=NULL;hook();}if(!erase_allowed)return ESP_FAIL;erased++;return ESP_OK;}
void vTaskDelay(uint32_t ticks){assert(erased==1&&!restarted&&ticks==300);delayed++;}
void esp_restart(void){assert((erased==0&&delayed==0)||(erased==1&&delayed==1));restarted++;}
static void retire_reboot_review(void){reboot_cancel_cb(NULL);}
static void retire_reboot_status(void){lv_obj_delete(s_reboot_status);}
static void on_changed(void){changed++;}
static void replace_timezone(void){timezone_popup_close();ui_settings_popups_show_timezone(on_changed);}
static void nested_reset(void){reset_settings_confirm_cb(NULL);}
static void replacement_changed(void){changed++;ui_settings_popups_show_timezone(on_changed);}
static const timezone_config_entry_t entries[]={
 {"central","Central (Chicago)","CST/CDT","CST6CDT,M3.2.0,M11.1.0"},
 {"pacific","Pacific region with a deliberately long descriptive city and daylight-saving label to verify wrapping without ellipsizing","PST/PDT","PST8PDT,M3.2.0,M11.1.0"},
 {"utc","Coordinated Universal Time","UTC","UTC0"}};
size_t timezone_config_count(void){return 24;}
const timezone_config_entry_t *timezone_config_entry(size_t index){return index==5?NULL:&entries[index%3];}
size_t timezone_config_selected_index(void){return selected;}
bool timezone_config_select(size_t index){last_index=index;if(preference_hook){void(*hook)(void)=preference_hook;preference_hook=NULL;hook();}if(fail_selection)return false;selected=index;return true;}
static void layout(lv_obj_t *popup){lv_obj_update_layout(popup);lv_obj_t *body=lv_obj_get_child(popup,1),*footer=lv_obj_get_child(popup,-1);inside(body,popup);inside(footer,popup);lv_obj_t *title=lv_obj_get_child(popup,0);inside(title,popup);assert(lv_obj_get_style_border_side(title,0)==LV_BORDER_SIDE_BOTTOM);assert(lv_obj_get_style_border_width(title,0)>0);assert(lv_obj_get_scrollbar_mode(body)==LV_SCROLLBAR_MODE_OFF);lv_area_t title_area,body_area,footer_area;lv_obj_get_coords(title,&title_area);lv_obj_get_coords(body,&body_area);lv_obj_get_coords(footer,&footer_area);assert(title_area.y2<body_area.y1&&body_area.y2<footer_area.y1);for(unsigned i=0;i<lv_obj_get_child_count(footer);i++){lv_obj_t *button=lv_obj_get_child(footer,i);inside(button,footer);inside(lv_obj_get_child(button,0),button);assert(lv_obj_get_height(button)>=48);}lv_area_t a,b;lv_obj_get_coords(footer,&a);lv_obj_scroll_to_y(body,300,LV_ANIM_OFF);lv_obj_update_layout(popup);lv_obj_get_coords(footer,&b);assert(!memcmp(&a,&b,sizeof(a)));lv_obj_scroll_to_y(body,0,LV_ANIM_OFF);}
int main(void){lv_init();lv_display_t *d=lv_display_create(1024,600);static uint8_t buffer[1024*40*2];lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);lv_display_set_buffers(d,buffer,NULL,sizeof(buffer),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(d,flush);
for(int size=0;size<4;size++)for(int theme=0;theme<5;theme++)for(int density=0;density<3;density++)for(int large=0;large<2;large++){
 viewport_width=(int[]){1024,640,480,480}[size];lv_display_set_resolution(d,viewport_width,size==3?240:600);ui_theme_set_active(theme);ui_theme_set_density(density);ui_theme_set_accessibility((ui_accessibility_t){.large_text=large});selected=0;fail_selection=false;unsigned before=changed,objects_before=live_objects(lv_screen_active())+live_objects(lv_layer_top()),timers_before=live_timers();
 ui_settings_popups_show_reboot();assert(s_reboot_popup);layout(s_reboot_popup);if(!size&&density==1&&large)snapshot(theme,"reboot");lv_obj_t *reboot=s_reboot_popup;ui_settings_popups_show_reboot();assert(s_reboot_popup==reboot&&!restarted);
 update_active=true;reboot_confirm_cb(NULL);assert(!restarted&&strstr(lv_label_get_text(s_reboot_status),"firmware update"));layout(s_reboot_popup);update_active=false;
 for(backup_state=SETTINGS_BACKUP_ENCRYPTED_WORKER_PREPARING;backup_state<=SETTINGS_BACKUP_ENCRYPTED_WORKER_WRITING;backup_state++){reboot_confirm_cb(NULL);assert(!restarted&&strstr(lv_label_get_text(s_reboot_status),"backup operation"));}backup_state=SETTINGS_BACKUP_ENCRYPTED_WORKER_IDLE;
 click(lv_obj_get_child(lv_obj_get_child(s_reboot_popup,-1),0));assert(!s_reboot_popup&&!restarted);
 ui_settings_popups_show_reboot();controller_read_hook=retire_reboot_review;reboot_confirm_cb(NULL);assert(!s_reboot_popup&&!restarted);
 ui_settings_popups_show_reboot();update_active=true;controller_read_hook=retire_reboot_status;reboot_confirm_cb(NULL);assert(s_reboot_popup&&!s_reboot_status&&!restarted);update_active=false;reboot_cancel_cb(NULL);
 reset_settings_cb(NULL);assert(s_reset_settings_popup);layout(s_reset_settings_popup);lv_obj_t *popup=s_reset_settings_popup;reset_settings_cb(NULL);assert(popup==s_reset_settings_popup);lv_obj_t *body=lv_obj_get_child(popup,1),*warning=lv_obj_get_child(body,0);assert(strstr(lv_label_get_text(warning),"erase saved Wi-Fi, Moonraker, OTA URL"));assert(!strcmp(lv_label_get_text(lv_obj_get_child(body,1)),"Firmware, OTA slots, and rollback recovery will be kept."));if(!size&&density==1&&large)snapshot(theme,"reset");click(lv_obj_get_child(lv_obj_get_child(popup,-1),0));assert(!s_reset_settings_popup&&!erased&&!restarted&&!delayed);
 reset_settings_cb(NULL);update_active=true;unsigned blocked_attempts=erase_attempts;reset_settings_confirm_cb(NULL);assert(erase_attempts==blocked_attempts&&!restarted&&strstr(lv_label_get_text(s_reset_status),"firmware update"));update_active=false;backup_state=SETTINGS_BACKUP_ENCRYPTED_WORKER_WRITING;reset_settings_confirm_cb(NULL);assert(erase_attempts==blocked_attempts&&!restarted);backup_state=SETTINGS_BACKUP_ENCRYPTED_WORKER_IDLE;erase_allowed=false;unsigned attempts=erase_attempts;preference_hook=nested_reset;reset_settings_confirm_cb(NULL);assert(erase_attempts==attempts+1&&!erased&&!restarted&&s_reset_settings_popup&&strstr(lv_label_get_text(s_reset_status),"Could not erase"));erase_allowed=true;lv_obj_delete(s_reset_settings_popup);assert(!s_reset_settings_popup&&!erased);reset_settings_cb(NULL);reset_settings_cancel_cb(NULL);
 ui_settings_popups_show_timezone(on_changed);assert(s_timezone_popup);layout(s_timezone_popup);popup=s_timezone_popup;ui_settings_popups_show_timezone(on_changed);assert(popup==s_timezone_popup);body=lv_obj_get_child(popup,1);assert(lv_obj_get_child_count(body)==24);assert(lv_obj_get_scroll_bottom(body)>0);for(unsigned i=1;i<24;i++){lv_obj_t *row=lv_obj_get_child(body,i);assert(lv_obj_get_height(row)>=48);inside(lv_obj_get_child(row,0),row);inside(lv_obj_get_child(row,1),row);lv_area_t name_area,caption_area;lv_obj_get_coords(lv_obj_get_child(row,0),&name_area);lv_obj_get_coords(lv_obj_get_child(row,1),&caption_area);assert(name_area.x1==caption_area.x1&&name_area.y2<caption_area.y1);assert(lv_obj_get_style_text_align(lv_obj_get_child(row,0),0)==LV_TEXT_ALIGN_LEFT);if(ui_theme_is_studio()){assert(lv_obj_get_style_radius(row,0)==0);assert(lv_obj_get_style_border_side(row,0)==LV_BORDER_SIDE_BOTTOM);}}assert(strstr(lv_label_get_text(lv_obj_get_child(lv_obj_get_child(body,2),0)),entries[1].label));assert(!strcmp(lv_label_get_text(lv_obj_get_child(lv_obj_get_child(body,2),1)),entries[1].abbreviation));if(!size&&density==1&&large)snapshot(theme,"time-zone");
 fail_selection=true;click(lv_obj_get_child(body,2));assert(s_timezone_popup&&changed==before&&last_index==1);fail_selection=false;click(lv_obj_get_child(body,2));assert(!s_timezone_popup&&!s_timezone_changed_cb&&selected==1&&changed==before+1);
 ui_settings_popups_show_timezone(on_changed);layout(s_timezone_popup);click(lv_obj_get_child(lv_obj_get_child(s_timezone_popup,-1),0));assert(!s_timezone_popup&&changed==before+1);
 ui_settings_popups_show_timezone(on_changed);body=lv_obj_get_child(s_timezone_popup,1);click(lv_obj_get_child(body,6));assert(last_index==6&&!s_timezone_popup&&changed==before+2); // Missing entry index 5 must not renumber subsequent commands.
 ui_settings_popups_show_timezone(on_changed);lv_obj_delete(s_timezone_popup);assert(!s_timezone_popup&&!s_timezone_changed_cb);ui_settings_popups_show_timezone(NULL);body=lv_obj_get_child(s_timezone_popup,1);click(lv_obj_get_child(body,1));assert(!s_timezone_popup&&selected==0&&changed==before+2);
 ui_settings_popups_show_timezone(on_changed);preference_hook=replace_timezone;body=lv_obj_get_child(s_timezone_popup,1);click(lv_obj_get_child(body,1));assert(s_timezone_popup&&s_timezone_changed_cb&&changed==before+2);timezone_popup_close();
 ui_settings_popups_show_timezone(replacement_changed);body=lv_obj_get_child(s_timezone_popup,1);click(lv_obj_get_child(body,1));assert(s_timezone_popup&&s_timezone_changed_cb&&changed==before+3);timezone_popup_close();
 assert(live_objects(lv_screen_active())+live_objects(lv_layer_top())==objects_before && live_timers()==timers_before);
}
// A stubbed reboot returns, so verify one-shot dispatch then simulate a fresh boot.
ui_settings_popups_show_reboot();reboot_confirm_cb(NULL);assert(restarted==1&&!erased&&!delayed);reboot_confirm_cb(NULL);assert(restarted==1);reboot_cancel_cb(NULL);restarted=0;s_controller_restart_accepted=false;
// Verify the existing erase-delay-restart sequence once, entirely through stubs.
reset_settings_cb(NULL);click(lv_obj_get_child(lv_obj_get_child(s_reset_settings_popup,-1),1));assert(erased==1&&delayed==1&&restarted==1);reset_settings_confirm_cb(NULL);assert(erased==1&&delayed==1&&restarted==1);reset_settings_cancel_cb(NULL);
puts("PASS: Settings wrapping/scroll/pinned actions, five themes/densities/text sizes, 1024/640/480 widths and short viewport; timezone identity/failed-save/callback/close, reset Cancel/no erase and stubbed erase-delay-restart, repeat-open and deletion/reopen");return 0;}
