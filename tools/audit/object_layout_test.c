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
static void snapshot(int theme,const char *name){const char *folder=getenv("OBJECT_LAYOUT_SCREENSHOTS");if(!folder)return;lv_refr_now(NULL);char path[512];snprintf(path,sizeof(path),"%s/%s-theme%d.ppm",folder,name,theme);FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n1024 600\n255\n");for(int i=0;i<1024*600;i++){uint16_t v=raster[i];unsigned char c[3]={(v>>11)*255/31,((v>>5)&63)*255/63,(v&31)*255/31};fwrite(c,1,3,f);}fclose(f);}
static void inside(lv_obj_t *child,lv_obj_t *parent){lv_area_t a,b;lv_obj_get_coords(child,&a);lv_obj_get_coords(parent,&b);if(!(a.x1>=b.x1&&a.x2<=b.x2&&a.y1>=b.y1&&a.y2<=b.y2)){fprintf(stderr,"Outside %d,%d..%d,%d vs %d,%d..%d,%d\n",a.x1,a.y1,a.x2,a.y2,b.x1,b.y1,b.x2,b.y2);abort();}}
static moonraker_exclude_state_t fixture;
static uint32_t generation=1;static unsigned status_calls;
uint32_t moonraker_config_generation(void){return generation;}
void moonraker_exclude_state_snapshot(moonraker_exclude_state_t *out){*out=fixture;}
void ui_dashboard_status_popup_show(const char *title,const char *message){assert(!strcmp(title,"CANCEL OBJECT"));assert(message&&message[0]);status_calls++;}
static void reset(void){memset(&fixture,0,sizeof(fixture));fixture.available=true;fixture.object_count=48;fixture.truncated=true;fixture.bed_bounds_valid=true;fixture.bed_max_x=280;fixture.bed_max_y=260;for(int i=0;i<48;i++){memset(fixture.objects[i].name,'a'+i%26,95);snprintf(fixture.objects[i].name+91,5,"%04d",i);fixture.objects[i].excluded=i==0;fixture.objects[i].current=i==1;fixture.objects[i].has_center=true;fixture.objects[i].center.x=20+(i%8)*30;fixture.objects[i].center.y=20+(i/8)*35;}generation++;}
static void layout(lv_obj_t *popup){lv_obj_update_layout(popup);lv_obj_t *footer=lv_obj_get_child(popup,-1),*body=lv_obj_get_child(popup,1);inside(footer,popup);inside(body,popup);for(uint32_t i=0;i<lv_obj_get_child_count(footer);i++){lv_obj_t *b=lv_obj_get_child(footer,i);inside(b,footer);inside(lv_obj_get_child(b,0),b);assert(lv_obj_get_height(b)>=48);}lv_area_t before,after;lv_obj_get_coords(footer,&before);lv_obj_scroll_to_y(body,400,LV_ANIM_OFF);lv_obj_update_layout(popup);lv_obj_get_coords(footer,&after);assert(!memcmp(&before,&after,sizeof(before)));lv_obj_scroll_to_y(body,0,LV_ANIM_OFF);}
static lv_obj_t *list(void){return lv_obj_get_child(lv_obj_get_parent(s_object_map),1);}
static void rows(void){layout(s_object_list_popup);lv_obj_t *content=lv_obj_get_parent(s_object_map),*objects=list();inside(s_object_map,content);inside(objects,content);lv_area_t a,b;lv_obj_get_coords(s_object_map,&a);lv_obj_get_coords(objects,&b);assert(a.x2<b.x1||a.y2<b.y1);for(unsigned i=0;i<48;i++){lv_obj_t *row=lv_obj_get_child(objects,i);inside(lv_obj_get_child(row,0),row);assert(lv_obj_get_height(row)>=48);assert(strstr(lv_label_get_text(lv_obj_get_child(row,0)),fixture.objects[i].name));}assert(lv_obj_has_state(lv_obj_get_child(objects,0),LV_STATE_DISABLED));assert(s_selected_object_index==1);assert(lv_obj_get_scroll_bottom(objects)>0);}
int main(void){lv_init();lv_display_t *d=lv_display_create(1024,600);static uint8_t buffer[1024*40*2];lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);lv_display_set_buffers(d,buffer,NULL,sizeof(buffer),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(d,flush);
for(int size=0;size<3;size++)for(int theme=0;theme<4;theme++)for(int density=0;density<3;density++)for(int large=0;large<2;large++){
 viewport_width=(int[]){1024,640,480}[size];lv_display_set_resolution(d,viewport_width,size==2?400:600);ui_theme_set_active(theme);ui_theme_set_density(density);ui_theme_set_accessibility((ui_accessibility_t){.large_text=large});reset();ui_printer_popups_show_cancel_object(send);assert(s_object_list_popup);rows();if(!size&&density==1&&large)snapshot(theme,"objects");moonraker_exclude_state_t *original=s_exclude_snapshot;ui_printer_popups_show_cancel_object(send);assert(s_exclude_snapshot==original);unsigned before=sends;
 // Preserve map projection and select by its screen-space center.
 double minx,miny,maxx,maxy;object_map_bounds(&minx,&miny,&maxx,&maxy);lv_area_t area;lv_obj_get_content_coords(s_object_map,&area);lv_point_precise_t point=object_map_point(&area,&fixture.objects[4].center,minx,miny,maxx,maxy);lv_point_t screen={(int32_t)point.x,(int32_t)point.y};assert(object_at_map_point(s_object_map,&screen)==4);
 click(s_object_rows[2]);assert(s_selected_object_index==2);click(s_exclude_action_button);assert(s_object_confirm_popup);layout(s_object_confirm_popup);assert(!strcmp(lv_label_get_text(lv_obj_get_child(lv_obj_get_child(s_object_confirm_popup,1),1)),fixture.objects[2].name));if(!size&&density==1&&large)snapshot(theme,"object-confirm");click(lv_obj_get_child(lv_obj_get_child(s_object_confirm_popup,-1),0));assert(!s_object_confirm_popup&&sends==before&&s_object_list_popup);
 click(s_exclude_action_button);select_object_index(3);char command[128];snprintf(command,sizeof(command),"EXCLUDE_OBJECT NAME=%s",fixture.objects[2].name);moonraker_exclude_object_t swap=fixture.objects[2];fixture.objects[2]=fixture.objects[5];fixture.objects[5]=swap;confirm_cancel_object_cb(NULL);assert(sends==before+1&&!strcmp(sent,command));assert(!s_object_list_popup&&!s_exclude_snapshot&&!s_object_map&&!s_exclude_action_button);
 reset();ui_printer_popups_show_cancel_object(send);click(s_exclude_action_button);before=sends;generation++;confirm_cancel_object_cb(NULL);assert(sends==before&&!s_object_list_popup);
 reset();ui_printer_popups_show_cancel_object(send);click(s_exclude_action_button);fixture.objects[1].excluded=true;confirm_cancel_object_cb(NULL);assert(sends==before);
 reset();ui_printer_popups_show_cancel_object(send);click(s_exclude_action_button);fixture.available=false;confirm_cancel_object_cb(NULL);assert(sends==before);
 reset();ui_printer_popups_show_cancel_object(send);click(s_exclude_action_button);fixture.object_count=1;confirm_cancel_object_cb(NULL);assert(sends==before);
 reset();ui_printer_popups_show_cancel_object(send);click(s_exclude_action_button);lv_obj_delete(s_object_confirm_popup);assert(!s_object_confirm_popup&&!s_confirm_object[0]&&s_object_list_popup);click(s_exclude_action_button);lv_obj_delete(s_object_list_popup);assert(!s_object_confirm_popup&&!s_exclude_snapshot&&!s_object_rows[1]);
 reset();strcpy(fixture.objects[1].name,"bad name");ui_printer_popups_show_cancel_object(send);assert(lv_obj_has_state(s_exclude_action_button,LV_STATE_DISABLED));click(s_exclude_action_button);assert(!s_object_confirm_popup);click(lv_obj_get_child(lv_obj_get_child(s_object_list_popup,-1),0));assert(!s_exclude_snapshot);
 reset();fixture.available=false;unsigned status_before=status_calls;ui_printer_popups_show_cancel_object(send);assert(!s_object_list_popup&&!s_exclude_snapshot&&status_calls==status_before+1);
 reset();for(unsigned i=0;i<48;i++)fixture.objects[i].excluded=true;ui_printer_popups_show_cancel_object(send);assert(!s_object_list_popup&&!s_exclude_snapshot&&status_calls==status_before+2);
}
puts("PASS: object map/list wrapping, 48 rows, scrolling and pinned actions; all themes/densities/text sizes and widths; exact captured exclusion name, Back, stale/profile/excluded guards, repeat-open and external teardown");return 0;}
