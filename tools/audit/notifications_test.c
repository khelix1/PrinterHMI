#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "custom_theme.h"
#include "ui_toast.c"
static unsigned log_count,changes,owner=1,opens;
static ui_shell_page_t opened;
uint32_t moonraker_config_generation(void){return owner;}
void ui_shell_page_action(ui_shell_page_t page){opened=page;opens++;}
void ui_shell_set_active_nav(int page){assert(page==UI_SHELL_PAGE_CONSOLE);}
static uint8_t stored,staged;
static bool has_saved,fail_write;
static char printer[80]="Printer A";
bool custom_theme_color_override(uint32_t a,uint32_t b,uint32_t c,uint32_t *d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_metric_override(int32_t a,int32_t b,int32_t c,int32_t *d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_accent_override(uint8_t a,uint32_t *b){(void)a;(void)b;return false;}
bool custom_theme_surface_opacity(uint8_t *a){(void)a;return false;}
const char *moonraker_config_active_profile_name(void){return printer;}
esp_err_t nvs_open(const char *name,int mode,nvs_handle_t *h){assert(!strcmp(name,"notifications"));*h=mode;return ESP_OK;}
esp_err_t nvs_get_u8(nvs_handle_t h,const char *key,uint8_t *v){assert(h==NVS_READONLY&&!strcmp(key,"confirm"));*v=stored;return has_saved?ESP_OK:ESP_FAIL;}
esp_err_t nvs_set_u8(nvs_handle_t h,const char *key,uint8_t value){assert(h==NVS_READWRITE&&!strcmp(key,"confirm"));staged=value;return ESP_OK;}
esp_err_t nvs_commit(nvs_handle_t h){assert(h==NVS_READWRITE);if(fail_write)return ESP_FAIL;stored=staged;has_saved=true;return ESP_OK;}
void nvs_close(nvs_handle_t h){(void)h;}
void operator_event_log_add(operator_event_level_t level,const char *format,...){(void)level;(void)format;log_count++;}
static void changed(void){changes++;}
static void inside(lv_obj_t *c,lv_obj_t *p){lv_area_t a,b;lv_obj_get_coords(c,&a);lv_obj_get_coords(p,&b);assert(a.x1>=b.x1&&a.x2<=b.x2&&a.y1>=b.y1&&a.y2<=b.y2);}
static void bounds(lv_obj_t *root){lv_obj_update_layout(root);inside(root,lv_screen_active());for(unsigned i=0;i<lv_obj_get_child_count(root);i++)inside(lv_obj_get_child(root,i),root);lv_obj_t *body=lv_obj_get_child(root,lv_obj_get_child_count(root)==2?0:1),*footer=lv_obj_get_child(root,lv_obj_get_child_count(root)-1);lv_area_t a,b;lv_obj_get_coords(footer,&a);lv_obj_scroll_to_y(body,lv_obj_get_scroll_bottom(body),LV_ANIM_OFF);lv_obj_update_layout(root);assert(lv_obj_get_scroll_bottom(body)<=1);lv_obj_get_coords(footer,&b);assert(!memcmp(&a,&b,sizeof(a)));for(unsigned i=0;i<lv_obj_get_child_count(footer);i++){lv_obj_t *button=lv_obj_get_child(footer,i);if(lv_obj_check_type(button,&lv_button_class)){inside(button,footer);inside(lv_obj_get_child(button,0),button);assert(lv_obj_get_height(button)==48);lv_obj_add_state(button,LV_STATE_PRESSED);assert(!lv_obj_get_style_transform_width(button,0)&&!lv_obj_get_style_outline_width(button,0));lv_obj_remove_state(button,LV_STATE_PRESSED);}}}
static uint16_t raster[1024*600];
static void flush(lv_display_t *d,const lv_area_t *a,uint8_t *p){uint16_t *v=(uint16_t *)p;for(int y=a->y1;y<=a->y2;y++)for(int x=a->x1;x<=a->x2;x++)raster[y*1024+x]=*v++;lv_display_flush_ready(d);}
static void capture(const char *name){const char *folder=getenv("NOTIFICATION_SCREENSHOTS");if(!folder||lv_display_get_horizontal_resolution(NULL)!=1024||ui_theme_get_density()!=1||!ui_theme_get_accessibility().large_text)return;lv_refr_now(NULL);char path[512];snprintf(path,sizeof(path),"%s/%s-theme%u.ppm",folder,name,(unsigned)ui_theme_get_active());FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n1024 600\n255\n");for(unsigned i=0;i<1024*600;i++){uint16_t v=raster[i];unsigned char c[3]={(v>>11)*255/31,((v>>5)&63)*255/63,(v&31)*255/31};fwrite(c,1,3,f);}fclose(f);}

int main(void){lv_init();lv_display_t *d=lv_display_create(1024,600);static uint8_t pixels[1024*50*2];lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);lv_display_set_buffers(d,pixels,NULL,sizeof(pixels),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(d,flush);
 for(unsigned theme=0;theme<5;theme++)for(unsigned density=0;density<3;density++)for(unsigned large=0;large<2;large++)for(unsigned narrow=0;narrow<3;narrow++){
  lv_display_set_resolution(d,(int[]){1024,640,480}[narrow],narrow==2?400:600);ui_theme_set_active(theme);ui_theme_set_density(density);ui_theme_set_accessibility((ui_accessibility_t){.large_text=large,.reduced_motion=true});has_saved=false;stored=0;fail_write=false;log_count=changes=0;ui_toast_init();assert(!ui_toast_confirmations_enabled());
  ui_toast_show(UI_STATUS_OK,"SAVED","No essential instructions here");assert(!s_toast&&!s_timer);
  ui_toast_show(UI_STATUS_DANGER,"STOP NOT CONFIRMED","Use the printer power switch if needed.");assert(s_toast&&!s_timer&&s_notice_count==1&&log_count==1);bounds(s_toast);capture("notification-fault");lv_tick_inc(10000);lv_timer_handler();assert(s_toast&&s_notice_count==1);
  ui_toast_show(UI_STATUS_DANGER,"STOP NOT CONFIRMED","Use the printer power switch if needed.");assert(s_notice_count==1&&log_count==1);
  assert(ui_toast_set_confirmations(true));ui_toast_show(UI_STATUS_OK,"SAVED","");assert(s_notice_count==1&&s_toast&&!s_timer);ui_toast_init();assert(ui_toast_confirmations_enabled());
  strcpy(printer,"Printer B");ui_toast_show(UI_STATUS_WARNING,"OFFLINE","Check connection");assert(s_notice_count==2&&!strcmp(s_notices[0].printer,"Printer A")&&!strcmp(s_notices[1].printer,"Printer B"));dismiss_notice(NULL);assert(s_notice_count==1&&!strcmp(s_notices[0].printer,"Printer B"));dismiss_notice(NULL);assert(!s_toast&&!s_notice_count);
  ui_toast_show(UI_STATUS_OK,"SAVED","Optional confirmation");assert(s_toast&&s_timer);lv_tick_inc(3000);lv_timer_handler();assert(!s_toast&&!s_timer);
  ui_toast_preferences_show(changed);bounds(s_preference_popup);capture("notification-preferences");preference_toggle(NULL);assert(!s_pending_confirmations);fail_write=true;preference_save(NULL);assert(s_preference_popup&&s_confirmations&&strstr(lv_label_get_text(s_preference_status),"Could not save"));fail_write=false;preference_save(NULL);assert(!s_preference_popup&&!s_confirmations&&changes==1);ui_toast_init();assert(!s_confirmations);
  for(unsigned i=0;i<6;i++){char title[32];snprintf(title,sizeof(title),"FAULT %u",i);ui_toast_show(UI_STATUS_DANGER,title,"Important instructions remain visible until dismissed.");}assert(s_notice_count==4&&s_overflow==2);bounds(s_toast);unsigned pending=s_notice_count;ui_toast_show(UI_STATUS_INFO,"INFO","Additional context");assert(s_notice_count==pending&&s_overflow==3);assert(!strcmp(s_notices[0].title,"FAULT 5"));
  lv_obj_delete(s_toast);assert(!s_toast&&s_notice_count==4);render_notice();assert(s_toast);ui_toast_close();assert(!s_toast&&!s_notice_count);
  ui_toast_show_link(UI_STATUS_DANGER,"COMMAND FAILED","Check the failed command.",UI_SHELL_PAGE_CONSOLE);bounds(s_toast);
  unsigned before=opens;owner++;open_notice(NULL);assert(opens==before&&s_notice_count==1);
  owner--;open_notice(NULL);assert(opens==before+1&&opened==UI_SHELL_PAGE_CONSOLE&&!s_notice_count);
  ui_toast_preferences_show(NULL);lv_obj_delete(s_preference_popup);assert(!s_preference_popup&&!s_preference_value);assert(!lv_obj_get_child_count(lv_layer_top()));strcpy(printer,"Printer A");
 }
 lv_display_delete(d);lv_deinit();puts("PASS: global quiet/confirmation policy, persistent faults, deduplication/priority/bounded queue, event history, profile identity, preference persistence/failure, layouts and lifecycle across all themes/densities/text sizes/viewports");return 0;}
