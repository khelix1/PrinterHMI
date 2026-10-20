#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "custom_theme.h"
#include "ui_global_estop.c"
static uint32_t owner=1;
uint32_t moonraker_config_generation(void){return owner;}
bool custom_theme_color_override(uint32_t a,uint32_t b,uint32_t c,uint32_t*d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_metric_override(int32_t a,int32_t b,int32_t c,int32_t*d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_accent_override(uint8_t a,uint32_t*b){(void)a;(void)b;return false;}
bool custom_theme_surface_opacity(uint8_t*a){(void)a;return false;}
static unsigned stops,restarts,alternate_calls;
static bool accept,require_stop_before_ui;
static void (*send_hook)(void);
static void click(lv_obj_t *object){lv_obj_send_event(object,LV_EVENT_CLICKED,NULL);}
static bool send(const char *command){if(!strcmp(command,"M112")){if(require_stop_before_ui)assert(!s_estop->popup);stops++;}else{assert(!strcmp(command,"FIRMWARE_RESTART"));restarts++;}if(send_hook){void(*hook)(void)=send_hook;send_hook=NULL;hook();}return accept;}
static bool alternate(const char *command){(void)command;alternate_calls++;return false;}
static void nested_restart(void){click(s_estop->restart);}
static void nested_stop(void){click(s_estop->button);}
static void replace_review(void){estop_close_popup();ui_global_estop_show_restart_confirmation();}
static void switch_printer(void){owner++;ui_global_estop_set_printer_name("Same friendly name");}
static void replacement_on_delete(lv_event_t *event){(void)event;ui_global_estop_show_restart_confirmation();}
static void stop_during_restart(void){click(s_estop->button);}
static uint16_t raster[1024*600];
static void flush(lv_display_t *display,const lv_area_t *area,uint8_t *pixels){
 uint16_t *p=(uint16_t*)pixels;int width=lv_display_get_horizontal_resolution(display);
 for(int y=area->y1;y<=area->y2;y++)for(int x=area->x1;x<=area->x2;x++)raster[y*width+x]=*p++;
 lv_display_flush_ready(display);
}
static void snapshot(const char *name){
 const char *folder=getenv("RECOVERY_SCREENSHOTS");unsigned theme=ui_theme_get_active();
 if(!folder||lv_display_get_horizontal_resolution(NULL)!=1024||ui_theme_get_density()!=UI_DENSITY_COMFORTABLE||(theme!=UI_THEME_OPERATOR&&theme!=UI_THEME_STUDIO_DARK))return;
 lv_obj_t *body=lv_obj_get_child(s_estop->popup,1);int scroll=lv_obj_get_scroll_y(body);lv_obj_scroll_to_y(body,0,LV_ANIM_OFF);lv_refr_now(NULL);
 char path[512];snprintf(path,sizeof(path),"%s/%s-theme%u-large%u.ppm",folder,name,theme,ui_theme_get_accessibility().large_text);
 FILE *f=fopen(path,"wb");assert(f);fputs("P6\n1024 600\n255\n",f);
 for(unsigned i=0;i<1024*600;i++){uint16_t v=raster[i];unsigned char c[3]={(v>>11)*255/31,((v>>5)&63)*255/63,(v&31)*255/31};fwrite(c,1,3,f);}
 fclose(f);lv_obj_scroll_to_y(body,scroll,LV_ANIM_OFF);
}
static void inside(lv_obj_t *child,lv_obj_t *parent){lv_area_t a,b;lv_obj_get_coords(child,&a);lv_obj_get_coords(parent,&b);assert(a.x1>=b.x1&&a.x2<=b.x2&&a.y1>=b.y1&&a.y2<=b.y2);}
static void layout(void){lv_obj_update_layout(s_estop->popup);inside(s_estop->popup,lv_screen_active());lv_obj_t *body=lv_obj_get_child(s_estop->popup,1),*footer=lv_obj_get_child(s_estop->popup,2);inside(body,s_estop->popup);inside(footer,s_estop->popup);lv_obj_t *title=lv_obj_get_child(s_estop->popup,0);inside(title,s_estop->popup);assert(lv_obj_get_style_border_side(title,0)==LV_BORDER_SIDE_BOTTOM&&lv_obj_get_style_border_width(title,0)>0);assert(lv_obj_get_scrollbar_mode(body)==LV_SCROLLBAR_MODE_OFF);assert(lv_obj_get_style_border_side(footer,0)==LV_BORDER_SIDE_TOP);lv_area_t ta,ba,fa;lv_obj_get_coords(title,&ta);lv_obj_get_coords(body,&ba);lv_obj_get_coords(footer,&fa);assert(ta.y2<ba.y1&&ba.y2<fa.y1);for(unsigned i=0;i<lv_obj_get_child_count(footer);i++){lv_obj_t *button=lv_obj_get_child(footer,i);inside(button,footer);inside(lv_obj_get_child(button,0),button);assert(lv_obj_get_height(button)>=48);}lv_area_t a,b;lv_obj_get_coords(footer,&a);lv_obj_scroll_to_y(body,10000,LV_ANIM_OFF);lv_obj_update_layout(s_estop->popup);assert(lv_obj_get_scroll_bottom(body)<=1);lv_obj_get_coords(footer,&b);assert(!memcmp(&a,&b,sizeof(a)));}
int main(void){
 lv_init();lv_display_t *display=lv_display_create(1024,600);static uint8_t pixels[1024*20*2];lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB565);lv_display_set_buffers(display,pixels,NULL,sizeof(pixels),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(display,flush);
 assert(ui_global_estop_init(send));
 for(unsigned theme=0;theme<5;theme++)for(unsigned density=0;density<3;density++)for(unsigned large=0;large<2;large++)for(unsigned width=0;width<3;width++){
  owner++;lv_display_set_resolution(display,(int[]){1024,640,480}[width],width==2?400:600);ui_theme_set_active(theme);ui_theme_set_density(density);ui_theme_set_accessibility((ui_accessibility_t){.large_text=large});
  ui_global_estop_set_printer_name("Same friendly name");lv_obj_t *host=lv_obj_create(lv_screen_active());ui_global_estop_create(host);accept=false;
  unsigned before=stops;require_stop_before_ui=true;send_hook=nested_stop;click(s_estop->button);require_stop_before_ui=false;assert(stops==before+1&&s_estop->popup&&strstr(lv_label_get_text(s_estop->title),"not confirmed"));layout();snapshot("stop-not-confirmed");
  before=restarts;send_hook=nested_restart;click(s_estop->restart);assert(restarts==before+1&&s_estop->popup&&strstr(lv_label_get_text(s_estop->message),"Restart not sent"));snapshot("restart-failed");
  send_hook=replace_review;click(s_estop->restart);assert(s_estop->popup&&strstr(lv_label_get_text(s_estop->message),"interrupts"));
  before=restarts;owner++;click(s_estop->restart);assert(restarts==before&&strstr(lv_label_get_text(s_estop->message),"Printer changed"));estop_close_popup();
  ui_global_estop_show_restart_confirmation();snapshot("restart-review");ui_global_estop_init(alternate);before=alternate_calls;click(s_estop->restart);assert(alternate_calls==before&&strstr(lv_label_get_text(s_estop->message),"Restart not sent"));ui_global_estop_init(send);estop_close_popup();
  ui_global_estop_show_restart_confirmation();accept=true;before=restarts;click(s_estop->restart);assert(restarts==before+1&&lv_obj_has_state(s_estop->restart,LV_STATE_DISABLED));snapshot("restart-requested");estop_close_popup();ui_global_estop_show_restart_confirmation();click(s_estop->restart);assert(restarts==before+1);layout();click(s_estop->button);assert(strstr(lv_label_get_text(s_estop->message),"Verify")&&lv_obj_has_state(s_estop->restart,LV_STATE_DISABLED));snapshot("stop-requested");
  lv_tick_inc(5001);estop_close_popup();ui_global_estop_show_restart_confirmation();accept=false;click(s_estop->restart);assert(restarts==before+2);
  before=stops;send_hook=stop_during_restart;click(s_estop->restart);assert(stops==before+1&&strstr(lv_label_get_text(s_estop->title),"not confirmed"));
  estop_close_popup();ui_global_estop_show_restart_confirmation();lv_obj_delete(s_estop->message);assert(!s_estop->message);before=restarts;click(s_estop->restart);assert(restarts==before);estop_close_popup();
  ui_global_estop_show_restart_confirmation();send_hook=switch_printer;click(s_estop->restart);assert(s_estop->popup&&!strstr(lv_label_get_text(s_estop->message),"Restart not sent"));estop_close_popup();
  send_hook=replace_review;click(s_estop->button);assert(s_estop->popup&&strstr(lv_label_get_text(s_estop->title),"Restart Klipper?"));estop_close_popup();
  ui_global_estop_show_restart_confirmation();lv_obj_add_event_cb(s_estop->popup,replacement_on_delete,LV_EVENT_DELETE,NULL);click(s_estop->button);assert(s_estop->popup&&strstr(lv_label_get_text(s_estop->title),"Restart Klipper?"));estop_close_popup();
  lv_obj_delete(host);assert(!s_estop->button);assert(!lv_obj_get_child_count(lv_layer_top()));
 }
 heap_caps_free(s_estop);s_estop=NULL;lv_display_delete(display);lv_deinit();puts("PASS: recovery dialog owned printer/fields/sender, failure retry, nested actions, accepted reopen guard, M112 before feedback and during restart, replacement callbacks and all five themes/densities/text sizes/viewports");
}
