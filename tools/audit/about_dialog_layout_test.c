/* Actual About owner and compiled firmware identity, with native LVGL pointer input. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lvgl.h"
#include "custom_theme.h"
#include "ui_theme.h"
#include "esp_app_desc.h"
#include "firmware_identity.c"
#include "ui_about_popup.c"
static esp_app_desc_t app;
static bool missing_app;
static void(*about_read_hook)(void);
static uint16_t raster[1024*600];
static lv_point_t touch;
static bool pressed;
bool custom_theme_color_override(uint32_t a,uint32_t b,uint32_t c,uint32_t *d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_metric_override(int32_t a,int32_t b,int32_t c,int32_t *d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_accent_override(uint8_t a,uint32_t *b){(void)a;(void)b;return false;}
bool custom_theme_surface_opacity(uint8_t *a){(void)a;return false;}
const esp_app_desc_t *esp_app_get_description(void){if(about_read_hook){void(*hook)(void)=about_read_hook;about_read_hook=NULL;hook();}return missing_app?NULL:&app;}
static void cancel_about_read(void){ui_about_popup_close();ui_about_popup_show();}
static void nested_about_read(void){ui_about_popup_show();}
static void reopen_about_deleted(lv_event_t *event){(void)event;ui_about_popup_show();}
static void read_touch(lv_indev_t*i,lv_indev_data_t*d){(void)i;d->point=touch;d->state=pressed?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED;}
static void tap(lv_indev_t*i,lv_obj_t*b){lv_area_t a;lv_obj_get_coords(b,&a);touch=(lv_point_t){(a.x1+a.x2)/2,(a.y1+a.y2)/2};pressed=true;lv_indev_read(i);pressed=false;lv_indev_read(i);}
static void flush(lv_display_t*d,const lv_area_t*a,uint8_t*p){uint16_t*v=(uint16_t*)p;for(int y=a->y1;y<=a->y2;y++)for(int x=a->x1;x<=a->x2;x++)raster[y*1024+x]=*v++;lv_display_flush_ready(d);}
static void inside(lv_obj_t*c,lv_obj_t*p){lv_area_t a,b;lv_obj_get_coords(c,&a);lv_obj_get_coords(p,&b);assert(a.x1>=b.x1&&a.x2<=b.x2&&a.y1>=b.y1&&a.y2<=b.y2);}
static void content_widths(lv_obj_t*p){for(unsigned i=0;i<lv_obj_get_child_count(p);i++){lv_obj_t*c=lv_obj_get_child(p,i);lv_area_t a,b;lv_obj_get_coords(c,&a);lv_obj_get_coords(p,&b);assert(a.x1>=b.x1&&a.x2<=b.x2);if(!lv_obj_check_type(c,&lv_label_class))content_widths(c);}}
static bool has_text(lv_obj_t*p,const char*text){if(lv_obj_check_type(p,&lv_label_class)&&strstr(lv_label_get_text(p),text))return true;for(unsigned i=0;i<lv_obj_get_child_count(p);i++)if(has_text(lv_obj_get_child(p,i),text))return true;return false;}
static void capture(unsigned theme,unsigned channel){const char*folder=getenv("ABOUT_SCREENSHOTS");if(!folder)return;lv_refr_now(NULL);char path[512];snprintf(path,sizeof(path),"%s/about-theme%u-channel%u.ppm",folder,theme,channel);FILE*f=fopen(path,"wb");assert(f);fprintf(f,"P6\n1024 600\n255\n");for(unsigned i=0;i<1024*600;i++){uint16_t v=raster[i];unsigned char c[3]={(v>>11)*255/31,((v>>5)&63)*255/63,(v&31)*255/31};fwrite(c,1,3,f);}fclose(f);}
int main(void){
 lv_init();lv_display_t*d=lv_display_create(1024,600);static uint8_t pixels[1024*50*2];lv_display_set_buffers(d,pixels,NULL,sizeof(pixels),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);lv_display_set_flush_cb(d,flush);lv_indev_t*pointer=lv_indev_create();lv_indev_set_type(pointer,LV_INDEV_TYPE_POINTER);lv_indev_set_read_cb(pointer,read_touch);
 for(unsigned theme=0;theme<5;theme++)for(unsigned density=0;density<3;density++)for(unsigned large=0;large<2;large++)for(unsigned narrow=0;narrow<3;narrow++){
  lv_display_set_resolution(d,(int[]){1024,640,480}[narrow],narrow==2?400:600);ui_theme_set_active(theme);ui_theme_set_density(density);ui_theme_set_accessibility((ui_accessibility_t){.large_text=large});
  app=(esp_app_desc_t){0};snprintf(app.version,sizeof(app.version),"%s",PRINTERHMI_FIRMWARE_CHANNEL==2?"nightly-2026-10-11-123456789abc":PRINTERHMI_FIRMWARE_CHANNEL==1?"7.0.0":"7.0.0-dev");strcpy(app.date,"Oct 11 2026");strcpy(app.time,"14:50:00");strcpy(app.idf_ver,"v6.0.2-dirty");missing_app=false;
  ui_about_popup_show();assert(s_popup);lv_obj_update_layout(s_popup);inside(s_popup,lv_screen_active());for(unsigned i=0;i<3;i++)inside(lv_obj_get_child(s_popup,i),s_popup);content_widths(ui_form_body(s_popup));assert(has_text(s_popup,app.version)&&has_text(s_popup,firmware_identity_channel_name(PRINTERHMI_FIRMWARE_CHANNEL))&&has_text(s_popup,"abcdef123456")&&has_text(s_popup,"LVGL  9.5.0")&&has_text(s_popup,"v6.0.2-dirty"));assert(has_text(s_popup,"(modified)")==!!PRINTERHMI_SOURCE_MODIFIED);
  lv_obj_t *title=lv_obj_get_child(s_popup,0);assert(lv_obj_get_style_border_side(title,0)==LV_BORDER_SIDE_BOTTOM&&lv_obj_get_style_border_width(title,0)>0);
  lv_area_t title_area,body_area,footer_area;lv_obj_get_coords(title,&title_area);lv_obj_get_coords(ui_form_body(s_popup),&body_area);lv_obj_get_coords(lv_obj_get_child(s_popup,-1),&footer_area);assert(title_area.y2<body_area.y1&&body_area.y2<footer_area.y1);
  assert(lv_obj_get_scrollbar_mode(ui_form_body(s_popup))==LV_SCROLLBAR_MODE_OFF);
  lv_obj_t*initial=s_popup;ui_about_popup_show();assert(s_popup==initial);
  lv_obj_t*body=ui_form_body(s_popup),*footer=lv_obj_get_child(s_popup,-1),*button=lv_obj_get_child(footer,0),*label=lv_obj_get_child(button,0);lv_area_t before,after;lv_obj_get_coords(footer,&before);assert(lv_obj_get_height(button)>=48);inside(label,button);
  /* Size notifications bubbled from a child must not treat it as a button. */
  lv_obj_add_flag(label,LV_OBJ_FLAG_EVENT_BUBBLE);lv_obj_send_event(label,LV_EVENT_SIZE_CHANGED,NULL);lv_obj_update_layout(s_popup);inside(label,button);
  if(!narrow&&density==1&&large) {
   capture(theme,PRINTERHMI_FIRMWARE_CHANNEL);
   for(unsigned section=4;section<lv_obj_get_child_count(ui_form_body(s_popup));section++) {
    lv_obj_scroll_to_view(lv_obj_get_child(ui_form_body(s_popup),section),LV_ANIM_OFF);
    lv_obj_update_layout(s_popup);capture(theme,PRINTERHMI_FIRMWARE_CHANNEL+10*section);
   }
   lv_obj_scroll_to_y(ui_form_body(s_popup),0,LV_ANIM_OFF);lv_obj_update_layout(s_popup);
  }
  lv_obj_scroll_to_y(body,lv_obj_get_scroll_bottom(body),LV_ANIM_OFF);lv_obj_update_layout(s_popup);assert(lv_obj_get_scroll_bottom(body)<=1);lv_obj_get_coords(footer,&after);assert(!memcmp(&before,&after,sizeof(before)));inside(lv_obj_get_child(lv_obj_get_child(body,-1),-1),body);assert(has_text(lv_obj_get_child(body,-1),"docs/SOFTWARE_CREDITS.md"));
  const char *credits[]={"Created by khelix","Concept, product direction","AI-assisted development","OpenAI ChatGPT/Codex","release approval remain with khelix","ESP-IDF","ESP-Hosted","LVGL","cJSON","LodePNG","FreeRTOS","lwIP","Mbed TLS","FatFs","Inter Project Authors","Montserrat Project Authors","Fonticons, Inc.","DejaVu","Klipper","Moonraker","PrusaSlicer","Development tools","Source & notices"};
  for(unsigned credit=0;credit<sizeof(credits)/sizeof(credits[0]);credit++){assert(has_text(s_popup,credits[credit]));}
  tap(pointer,button);assert(!s_popup&&!lv_obj_get_child_count(lv_layer_top()));
  for(unsigned i=0;i<10;i++){ui_about_popup_show();lv_obj_delete(s_popup);assert(!s_popup);ui_about_popup_close();}
  about_read_hook=cancel_about_read;ui_about_popup_show();assert(!s_popup&&!s_about_building);about_read_hook=nested_about_read;ui_about_popup_show();assert(s_popup&&!s_about_building);ui_about_popup_close();
  ui_about_popup_show();lv_obj_add_event_cb(s_popup,reopen_about_deleted,LV_EVENT_DELETE,NULL);ui_about_popup_close();assert(s_popup);ui_about_popup_close();
  missing_app=true;ui_about_popup_show();assert(s_popup&&has_text(s_popup,"unknown")&&has_text(s_popup,"Built  --"));ui_about_popup_close();assert(!lv_obj_get_child_count(lv_layer_top()));
 }
 lv_indev_delete(pointer);lv_display_delete(d);lv_deinit();puts("PASS: About native layout, build identity, pinned Close, scroll reach, actual pointer closure and delete/reopen across all themes/densities/text/viewports");
}
