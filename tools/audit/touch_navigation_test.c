#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "lvgl.h"
#include "ui_form_dialog.h"
#include "ui_theme.h"
bool custom_theme_color_override(uint32_t a,uint32_t b,uint32_t c,uint32_t *d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_metric_override(int32_t a,int32_t b,int32_t c,int32_t *d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_accent_override(uint8_t a,uint32_t *b){(void)a;(void)b;return false;}
bool custom_theme_surface_opacity(uint8_t *a){(void)a;return false;}
static lv_point_t point;
static bool pressed;
static lv_indev_t *pointer;
static void read_touch(lv_indev_t *i,lv_indev_data_t *d){(void)i;d->point=point;d->state=pressed?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED;}
static void input(int x,int y,bool down){point=(lv_point_t){x,y};pressed=down;lv_tick_inc(20);lv_indev_read(pointer);}
static void tap_at(int x,int y){input(x,y,true);input(x,y,false);}
static void tap(lv_obj_t *o){lv_obj_update_layout(o);lv_area_t a;lv_obj_get_coords(o,&a);tap_at((a.x1+a.x2)/2,(a.y1+a.y2)/2);}
static void count_cb(lv_event_t *e){(*(unsigned*)lv_event_get_user_data(e))++;}
static void deleted_cb(lv_event_t *e){lv_obj_t **slot=lv_event_get_user_data(e);if(*slot==lv_event_get_target_obj(e))*slot=NULL;}
static void close_cb(lv_event_t *e){lv_obj_t **slot=lv_event_get_user_data(e);if(*slot)lv_obj_delete(*slot);}
static void flush(lv_display_t*d,const lv_area_t*a,uint8_t*p){(void)a;(void)p;lv_display_flush_ready(d);}
static lv_obj_t *dialog(lv_obj_t *owner,lv_obj_t **slot,int w,int h){
 *slot=ui_form_dialog("OPERATOR DIALOG",w,h,UI_POPUP_STANDARD);assert(*slot);ui_popup_set_owner(*slot,owner);
 lv_obj_add_event_cb(*slot,deleted_cb,LV_EVENT_DELETE,slot);
 ui_form_text(ui_form_body(*slot),"Use Close or Back to return. Background actions stay blocked.",UI_FONT_BODY,UI_TEXT);
 ui_form_action(*slot,UI_BUTTON_OUTLINED,"BACK",close_cb,slot);ui_form_finish(*slot);return *slot;
}
int main(void){
 lv_init();lv_display_t*d=lv_display_create(1024,600);static uint8_t pixels[1024*40*2];lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);lv_display_set_buffers(d,pixels,NULL,sizeof(pixels),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(d,flush);
 pointer=lv_indev_create();lv_indev_set_type(pointer,LV_INDEV_TYPE_POINTER);lv_indev_set_read_cb(pointer,read_touch);
 for(unsigned theme=0;theme<5;theme++)for(unsigned density=0;density<3;density++)for(unsigned large=0;large<2;large++)for(unsigned size=0;size<3;size++){
  int w=(int[]){1024,640,480}[size],h=(int[]){600,480,320}[size];lv_display_set_resolution(d,w,h);ui_theme_set_active(theme);ui_theme_set_density(density);ui_theme_set_accessibility((ui_accessibility_t){.large_text=large});
  unsigned background=0,left=0,right=0,scroll_clicks=0;
  lv_obj_t*page=lv_obj_create(lv_screen_active());lv_obj_remove_style_all(page);lv_obj_set_size(page,w,h);lv_obj_clear_flag(page,LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t*bg=ui_button_create(page,UI_BUTTON_PRIMARY,"PAGE");lv_obj_set_size(bg,100,44);lv_obj_set_pos(bg,8,8);lv_obj_add_event_cb(bg,count_cb,LV_EVENT_CLICKED,&background);
  lv_obj_t*a=ui_button_create(page,UI_BUTTON_OUTLINED,"LEFT"),*b=ui_button_create(page,UI_BUTTON_OUTLINED,"RIGHT");lv_obj_set_size(a,100,44);lv_obj_set_pos(a,20,100);lv_obj_set_size(b,100,44);lv_obj_set_pos(b,128,100);lv_obj_add_event_cb(a,count_cb,LV_EVENT_CLICKED,&left);lv_obj_add_event_cb(b,count_cb,LV_EVENT_CLICKED,&right);lv_obj_update_layout(page);
  tap(lv_obj_get_child(a,0));assert(left==1&&!right);tap_at(122,122);assert(left==2&&!right);tap_at(125,122);assert(left==2&&right==1);
  lv_obj_add_state(a,LV_STATE_DISABLED);tap(a);assert(left==2);lv_obj_remove_state(a,LV_STATE_DISABLED);lv_obj_add_flag(b,LV_OBJ_FLAG_HIDDEN);tap(b);assert(right==1);lv_obj_remove_flag(b,LV_OBJ_FLAG_HIDDEN);
  lv_obj_t*list=lv_obj_create(page);lv_obj_remove_style_all(list);lv_obj_set_size(list,240,100);lv_obj_set_pos(list,20,170);lv_obj_set_scroll_dir(list,LV_DIR_VER);lv_obj_t*row=ui_button_create(list,UI_BUTTON_OUTLINED,"SCROLL ROW");lv_obj_set_size(row,200,60);lv_obj_add_event_cb(row,count_cb,LV_EVENT_CLICKED,&scroll_clicks);lv_obj_t*spacer=lv_obj_create(list);lv_obj_set_size(spacer,200,240);lv_obj_set_pos(spacer,0,65);lv_obj_update_layout(page);
  input(90,200,true);input(90,185,true);input(90,180,true);input(90,180,false);assert(!scroll_clicks);lv_obj_scroll_to_y(list,0,LV_ANIM_OFF);
  lv_obj_t*outer=NULL,*inner=NULL;dialog(page,&outer,w-64,h-48);unsigned top_count=lv_obj_get_child_count(lv_layer_top());tap_at(15,15);assert(!background&&outer&&top_count==lv_obj_get_child_count(lv_layer_top()));
  dialog(outer,&inner,w-96,h-72);tap_at(15,15);assert(!background&&inner&&outer);tap(lv_obj_get_child(lv_obj_get_child(inner,-1),0));assert(!inner&&outer);tap(lv_obj_get_child(lv_obj_get_child(outer,-1),0));assert(!outer&&!lv_obj_get_child_count(lv_layer_top()));tap(bg);assert(background==1);
  dialog(page,&outer,w-64,h-48);dialog(outer,&inner,w-96,h-72);lv_obj_delete(page);assert(!inner&&!outer&&!lv_obj_get_child_count(lv_layer_top()));
  page=lv_obj_create(lv_screen_active());dialog(page,&outer,w-64,h-48);lv_obj_t*blocker=lv_obj_get_child(lv_layer_top(),0);lv_obj_delete(blocker);assert(!outer&&!lv_obj_get_child_count(lv_layer_top()));lv_obj_delete(page);
 }
 lv_indev_delete(pointer);lv_display_delete(d);lv_deinit();puts("PASS: real pointer label/edge taps, adjacent target separation, disabled/hidden controls, scroll suppresses clicks, modal backdrop isolation, nested Back/Close and page/blocker deletion across five themes/densities/text sizes/viewports");return 0;
}
