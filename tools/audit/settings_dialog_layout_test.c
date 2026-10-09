#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
static unsigned erased,restarted,delayed,changed;static bool fail_selection;static size_t selected,last_index;
esp_err_t nvs_flash_erase(void){assert(!erased&&!restarted&&!delayed);erased++;return ESP_OK;}
void vTaskDelay(uint32_t ticks){assert(erased==1&&!restarted&&ticks==300);delayed++;}
void esp_restart(void){assert(erased==1&&delayed==1);restarted++;}
static void on_changed(void){changed++;}
static const timezone_config_entry_t entries[]={
 {"central","Central (Chicago)","CST/CDT","CST6CDT,M3.2.0,M11.1.0"},
 {"pacific","Pacific region with a deliberately long descriptive city and daylight-saving label to verify wrapping without ellipsizing","PST/PDT","PST8PDT,M3.2.0,M11.1.0"},
 {"utc","Coordinated Universal Time","UTC","UTC0"}};
size_t timezone_config_count(void){return 24;}
const timezone_config_entry_t *timezone_config_entry(size_t index){return index==5?NULL:&entries[index%3];}
size_t timezone_config_selected_index(void){return selected;}
bool timezone_config_select(size_t index){last_index=index;if(fail_selection)return false;selected=index;return true;}
static void layout(lv_obj_t *popup){lv_obj_update_layout(popup);lv_obj_t *body=lv_obj_get_child(popup,1),*footer=lv_obj_get_child(popup,-1);inside(body,popup);inside(footer,popup);for(unsigned i=0;i<lv_obj_get_child_count(footer);i++){lv_obj_t *button=lv_obj_get_child(footer,i);inside(button,footer);inside(lv_obj_get_child(button,0),button);assert(lv_obj_get_height(button)>=48);}lv_area_t a,b;lv_obj_get_coords(footer,&a);lv_obj_scroll_to_y(body,300,LV_ANIM_OFF);lv_obj_update_layout(popup);lv_obj_get_coords(footer,&b);assert(!memcmp(&a,&b,sizeof(a)));lv_obj_scroll_to_y(body,0,LV_ANIM_OFF);}
int main(void){lv_init();lv_display_t *d=lv_display_create(1024,600);static uint8_t buffer[1024*40*2];lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);lv_display_set_buffers(d,buffer,NULL,sizeof(buffer),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(d,flush);
for(int size=0;size<4;size++)for(int theme=0;theme<4;theme++)for(int density=0;density<3;density++)for(int large=0;large<2;large++){
 viewport_width=(int[]){1024,640,480,480}[size];lv_display_set_resolution(d,viewport_width,size==3?240:600);ui_theme_set_active(theme);ui_theme_set_density(density);ui_theme_set_accessibility((ui_accessibility_t){.large_text=large});selected=0;fail_selection=false;unsigned before=changed;
 reset_settings_cb(NULL);assert(s_reset_settings_popup);layout(s_reset_settings_popup);lv_obj_t *popup=s_reset_settings_popup;reset_settings_cb(NULL);assert(popup==s_reset_settings_popup);lv_obj_t *body=lv_obj_get_child(popup,1),*warning=lv_obj_get_child(body,0);assert(strstr(lv_label_get_text(warning),"erase saved WiFi, Moonraker, OTA URL"));assert(strstr(lv_label_get_text(warning),"Firmware, OTA slots, and rollback recovery will NOT be erased."));if(!size&&density==1&&large)snapshot(theme,"reset");click(lv_obj_get_child(lv_obj_get_child(popup,-1),0));assert(!s_reset_settings_popup&&!erased&&!restarted&&!delayed);
 reset_settings_cb(NULL);lv_obj_delete(s_reset_settings_popup);assert(!s_reset_settings_popup&&!erased);reset_settings_cb(NULL);reset_settings_cancel_cb(NULL);
 ui_settings_popups_show_timezone(on_changed);assert(s_timezone_popup);layout(s_timezone_popup);popup=s_timezone_popup;ui_settings_popups_show_timezone(on_changed);assert(popup==s_timezone_popup);body=lv_obj_get_child(popup,1);assert(lv_obj_get_child_count(body)==24);assert(lv_obj_get_scroll_bottom(body)>0);for(unsigned i=1;i<24;i++){lv_obj_t *row=lv_obj_get_child(body,i);assert(lv_obj_get_height(row)>=48);inside(lv_obj_get_child(row,0),row);}assert(strstr(lv_label_get_text(lv_obj_get_child(lv_obj_get_child(body,2),0)),entries[1].label));assert(strstr(lv_label_get_text(lv_obj_get_child(lv_obj_get_child(body,2),0)),entries[1].abbreviation));if(!size&&density==1&&large)snapshot(theme,"time-zone");
 fail_selection=true;click(lv_obj_get_child(body,2));assert(s_timezone_popup&&changed==before&&last_index==1);fail_selection=false;click(lv_obj_get_child(body,2));assert(!s_timezone_popup&&!s_timezone_changed_cb&&selected==1&&changed==before+1);
 ui_settings_popups_show_timezone(on_changed);layout(s_timezone_popup);click(lv_obj_get_child(lv_obj_get_child(s_timezone_popup,-1),0));assert(!s_timezone_popup&&changed==before+1);
 ui_settings_popups_show_timezone(on_changed);body=lv_obj_get_child(s_timezone_popup,1);click(lv_obj_get_child(body,6));assert(last_index==6&&!s_timezone_popup&&changed==before+2); // Missing entry index 5 must not renumber subsequent commands.
 ui_settings_popups_show_timezone(on_changed);lv_obj_delete(s_timezone_popup);assert(!s_timezone_popup&&!s_timezone_changed_cb);ui_settings_popups_show_timezone(NULL);body=lv_obj_get_child(s_timezone_popup,1);click(lv_obj_get_child(body,1));assert(!s_timezone_popup&&selected==0&&changed==before+2);
}
// Verify the existing erase-delay-restart sequence once, entirely through stubs.
reset_settings_cb(NULL);click(lv_obj_get_child(lv_obj_get_child(s_reset_settings_popup,-1),1));assert(erased==1&&delayed==1&&restarted==1);reset_settings_cancel_cb(NULL);
puts("PASS: Settings wrapping/scroll/pinned actions, four themes/densities/text sizes, 1024/640/480 widths and short viewport; timezone identity/failed-save/callback/close, reset Cancel/no erase and stubbed erase-delay-restart, repeat-open and deletion/reopen");return 0;}
