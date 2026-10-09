#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "custom_theme.h"
#include "ui_printer_popups.c"
bool custom_theme_color_override(uint32_t a,uint32_t b,uint32_t c,uint32_t *d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_metric_override(int32_t a,int32_t b,int32_t c,int32_t *d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_accent_override(uint8_t a,uint32_t *b){(void)a;(void)b;return false;}
bool custom_theme_surface_opacity(uint8_t *a){(void)a;return false;}
static char sent[128];static unsigned sends;
static void send(const char *s){snprintf(sent,sizeof(sent),"%s",s);sends++;}
static void click(lv_obj_t *o){lv_obj_send_event(o,LV_EVENT_CLICKED,NULL);}
static uint16_t raster[1024*600];static int viewport_width=1024;
static void flush(lv_display_t *d,const lv_area_t *a,uint8_t *p){uint16_t *v=(uint16_t*)p;for(int y=a->y1;y<=a->y2;y++)for(int x=a->x1;x<=a->x2;x++)raster[y*viewport_width+x]=*v++;lv_display_flush_ready(d);}
static void snapshot(int theme,const char *name){const char *folder=getenv("CANCEL_LAYOUT_SCREENSHOTS");if(!folder)return;lv_refr_now(NULL);char path[512];snprintf(path,sizeof(path),"%s/%s-theme%d.ppm",folder,name,theme);FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n1024 600\n255\n");for(int i=0;i<1024*600;i++){uint16_t v=raster[i];unsigned char c[3]={(v>>11)*255/31,((v>>5)&63)*255/63,(v&31)*255/31};fwrite(c,1,3,f);}fclose(f);}
static void inside(lv_obj_t *child,lv_obj_t *parent){lv_area_t a,b;lv_obj_get_coords(child,&a);lv_obj_get_coords(parent,&b);if(!(a.x1>=b.x1&&a.x2<=b.x2&&a.y1>=b.y1&&a.y2<=b.y2)){fprintf(stderr,"Outside %d,%d..%d,%d vs %d,%d..%d,%d\n",a.x1,a.y1,a.x2,a.y2,b.x1,b.y1,b.x2,b.y2);abort();}}
static void layout(void){lv_obj_update_layout(s_cancel_confirm_popup);lv_obj_t *body=lv_obj_get_child(s_cancel_confirm_popup,1),*footer=lv_obj_get_child(s_cancel_confirm_popup,-1);inside(body,s_cancel_confirm_popup);inside(footer,s_cancel_confirm_popup);assert(lv_obj_get_child_count(footer)==2);for(unsigned i=0;i<2;i++){lv_obj_t *button=lv_obj_get_child(footer,i);inside(button,footer);inside(lv_obj_get_child(button,0),button);assert(lv_obj_get_height(button)>=48);}lv_area_t a,b;lv_obj_get_coords(footer,&a);lv_obj_scroll_to_y(body,300,LV_ANIM_OFF);lv_obj_update_layout(s_cancel_confirm_popup);lv_obj_get_coords(footer,&b);assert(!memcmp(&a,&b,sizeof(a)));lv_obj_scroll_to_y(body,0,LV_ANIM_OFF);}
int main(void){lv_init();lv_display_t *d=lv_display_create(1024,600);static uint8_t buffer[1024*40*2];lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);lv_display_set_buffers(d,buffer,NULL,sizeof(buffer),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(d,flush);
for(int size=0;size<4;size++)for(int theme=0;theme<4;theme++)for(int density=0;density<3;density++)for(int large=0;large<2;large++){
 viewport_width=(int[]){1024,640,480,480}[size];lv_display_set_resolution(d,viewport_width,size==3?240:600);ui_theme_set_active(theme);ui_theme_set_density(density);ui_theme_set_accessibility((ui_accessibility_t){.large_text=large});unsigned before=sends;
 ui_printer_popups_show_cancel(send);layout();lv_obj_t *popup=s_cancel_confirm_popup;ui_printer_popups_show_cancel(send);assert(s_cancel_confirm_popup==popup&&sends==before);lv_obj_t *body=lv_obj_get_child(popup,1),*warning=lv_obj_get_child(body,0);assert(!strcmp(lv_label_get_text(warning),"This will stop the active print job."));inside(warning,body);if(!size&&density==1&&large)snapshot(theme,"cancel-print");
 // Exercise actual scroll ownership with an overflowing warning body.
 lv_label_set_text(warning,"This will stop the active print job.\n\n1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n11\n12");layout();assert(lv_obj_get_scroll_bottom(body)>0);click(lv_obj_get_child(lv_obj_get_child(popup,-1),0));assert(!s_cancel_confirm_popup&&sends==before);
 ui_printer_popups_show_cancel(send);layout();click(lv_obj_get_child(lv_obj_get_child(s_cancel_confirm_popup,-1),1));assert(!s_cancel_confirm_popup&&sends==before+1&&!strcmp(sent,"CANCEL_PRINT"));
 ui_printer_popups_show_cancel(send);lv_obj_delete(s_cancel_confirm_popup);assert(!s_cancel_confirm_popup&&sends==before+1);ui_printer_popups_show_cancel(NULL);layout();click(lv_obj_get_child(lv_obj_get_child(s_cancel_confirm_popup,-1),1));assert(!s_cancel_confirm_popup&&sends==before+1);
}
puts("PASS: whole-print cancellation layout, four themes/densities/text sizes, 1024/640/480 widths and short viewport; wrapping/scroll/pinned actions, Back/no command, exact CANCEL_PRINT, repeat-open and deletion/reopen");return 0;}
