#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "custom_theme.h"
#include "ui_motion_diagnostics.c"
bool custom_theme_color_override(uint32_t a,uint32_t b,uint32_t c,uint32_t *d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_metric_override(int32_t a,int32_t b,int32_t c,int32_t *d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_accent_override(uint8_t a,uint32_t *b){(void)a;(void)b;return false;}
bool custom_theme_surface_opacity(uint8_t *a){(void)a;return false;}
static uint32_t generation=1;
static moonraker_state_t state;
static motion_diagnostics_snapshot_t motion;
uint32_t moonraker_config_generation(void){return generation;}
void moonraker_state_snapshot(moonraker_state_t *o){*o=state;}
void motion_diagnostics_controller_snapshot(motion_diagnostics_snapshot_t *o){*o=motion;}
void ui_toast_show(ui_status_kind_t kind,const char *title,const char *detail){(void)kind;(void)title;(void)detail;}
static uint16_t raster[1024*600];static int width=1024;
static void flush(lv_display_t *d,const lv_area_t *a,uint8_t *p){uint16_t *v=(uint16_t*)p;for(int y=a->y1;y<=a->y2;y++)for(int x=a->x1;x<=a->x2;x++)raster[y*width+x]=*v++;lv_display_flush_ready(d);}
static void snapshot(int theme,const char *name){const char *folder=getenv("MOTION_LAYOUT_SCREENSHOTS");if(!folder)return;lv_refr_now(NULL);char path[512];snprintf(path,sizeof(path),"%s/%s-theme%d.ppm",folder,name,theme);FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n1024 600\n255\n");for(int i=0;i<1024*600;i++){uint16_t v=raster[i];unsigned char c[3]={(v>>11)*255/31,((v>>5)&63)*255/63,(v&31)*255/31};fwrite(c,1,3,f);}fclose(f);}
static void inside(lv_obj_t *child,lv_obj_t *parent){lv_area_t a,b;lv_obj_get_coords(child,&a);lv_obj_get_coords(parent,&b);if(!(a.x1>=b.x1&&a.x2<=b.x2&&a.y1>=b.y1&&a.y2<=b.y2)){fprintf(stderr,"outside %d,%d..%d,%d vs %d,%d..%d,%d\n",a.x1,a.y1,a.x2,a.y2,b.x1,b.y1,b.x2,b.y2);abort();}}
static void click(lv_obj_t *o){lv_obj_send_event(o,LV_EVENT_CLICKED,NULL);}
static void layout(lv_obj_t *popup){lv_obj_update_layout(popup);lv_obj_t *footer=lv_obj_get_child(popup,-1),*body=lv_obj_get_child(popup,1);inside(footer,popup);inside(body,popup);for(uint32_t i=0;i<lv_obj_get_child_count(footer);i++){lv_obj_t *b=lv_obj_get_child(footer,i);assert(lv_obj_get_height(b)>=48);inside(b,footer);inside(lv_obj_get_child(b,0),b);}lv_area_t before,after;lv_obj_get_coords(footer,&before);lv_obj_scroll_to_y(body,300,LV_ANIM_OFF);lv_obj_update_layout(popup);lv_obj_get_coords(footer,&after);assert(!memcmp(&before,&after,sizeof(before)));lv_obj_scroll_to_y(body,0,LV_ANIM_OFF);}
static void reset(void){memset(&motion,0,sizeof(motion));memset(&state,0,sizeof(state));state.moonraker_ok=state.live_data_ok=true;strcpy(state.printer_state,"ready");motion.discovered=true;for(int i=0;i<4;i++){motion.limit_valid[i]=true;motion.limits[i]=(double[]){200,2500,6,0.5}[i];}motion.driver_count=2;strcpy(motion.drivers[0].name,"tmc2209 stepper_x");strcpy(motion.drivers[1].name,"tmc2209 stepper_y");motion.drivers[0].run_valid=motion.drivers[0].hold_valid=motion.drivers[0].status_valid=true;motion.drivers[0].run_current=0.8;motion.drivers[0].hold_current=0.3;strcpy(motion.drivers[0].flags,"Example driver warning text spanning multiple words");}
int main(void){lv_init();lv_display_t *d=lv_display_create(1024,600);static uint8_t pixels[1024*50*2];lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);lv_display_set_buffers(d,pixels,NULL,sizeof(pixels),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(d,flush);motion_ui_state_t storage={0};s=&storage;
 for(int narrow=0;narrow<3;narrow++)for(int theme=0;theme<5;theme++)for(int density=0;density<3;density++)for(int large=0;large<2;large++){
  width=(int[]){1024,640,480}[narrow];lv_display_set_resolution(d,width,narrow==2?400:600);ui_theme_set_active(theme);ui_theme_set_density(density);ui_theme_set_accessibility((ui_accessibility_t){.large_text=large});reset();
  lv_obj_t *card=lv_obj_create(lv_screen_active());lv_obj_remove_style_all(card);lv_obj_set_size(card,390,190);ui_motion_diagnostics_create(card);lv_obj_update_layout(card);
  for(unsigned i=0;i<3;i++){lv_obj_t*b=lv_obj_get_child(card,i);lv_obj_t*l=lv_obj_get_child(b,0);inside(b,card);const lv_font_t*f=lv_obj_get_style_text_font(l,0);lv_point_t n;lv_text_get_size(&n,lv_label_get_text(l),f,0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);assert(n.x<=lv_obj_get_width(l));assert(lv_obj_get_height(l)==f->line_height);inside(l,b);assert(lv_obj_get_height(b)==44);}
  /* The real guided Motion owner is covered by calibration_dialog_layout_test. */
  lv_obj_delete(card);
  show_view(VIEW_LIMITS);layout(s->popup);assert(!strcmp(lv_label_get_text(s->limits[0]),"200.00 mm/s"));for(int i=0;i<4;i++){lv_obj_t *row=lv_obj_get_parent(s->limits[i]);assert(lv_obj_get_height(row)<100);inside(s->limits[i],row);assert(lv_obj_get_height(s->limits[i])<=lv_obj_get_style_text_font(s->limits[i],0)->line_height);inside(lv_obj_get_child(row,0),row);}if(!narrow&&density==1&&large)snapshot(theme,"limits");close_popup();assert(!s->popup&&!s->timer);
  show_view(VIEW_DRIVERS);layout(s->popup);if(!narrow&&density==1&&large)snapshot(theme,"drivers");motion.drivers[0].fault=true;refresh(NULL);assert(strstr(lv_label_get_text(s->status),"FAULT"));state.live_data_ok=false;refresh(NULL);assert(lv_obj_has_state(s->driver_selector,LV_STATE_DISABLED));close_popup();state.live_data_ok=true;
  show_view(VIEW_DISTANCE);layout(s->popup);lv_obj_update_layout(s->popup);inside(s->pitch,lv_obj_get_parent(s->pitch));inside(s->pitch_label,lv_obj_get_parent(s->pitch));inside(s->count,lv_obj_get_parent(s->count));assert(lv_obj_get_height(s->pitch)>=56&&lv_obj_get_height(s->count)>=56);
  lv_area_t pitch_area,count_area;lv_obj_get_coords(s->pitch,&pitch_area);lv_obj_get_coords(s->count,&count_area);assert(pitch_area.y1==count_area.y1);
  if(!narrow&&density==1&&large){snapshot(theme,"distance");}calculate_cb(NULL);assert(strstr(lv_label_get_text(s->result),"40.000000"));lv_dropdown_set_selected(s->mode,1);lv_obj_send_event(s->mode,LV_EVENT_VALUE_CHANGED,NULL);lv_obj_update_layout(s->popup);lv_obj_get_coords(s->pitch,&pitch_area);lv_obj_get_coords(s->count,&count_area);assert(pitch_area.y1==count_area.y1);if(!narrow&&density==1&&large)snapshot(theme,"distance-screw");calculate_cb(NULL);assert(strstr(lv_label_get_text(s->result),"8.000000"));
  click(s->pitch);assert(s->editor);layout(s->editor);assert(lv_obj_get_height(s->editor_value)>=56);if(!narrow&&density==1&&large)snapshot(theme,"editor");lv_textarea_set_text(s->editor_value,"1.25");click(lv_obj_get_child(lv_obj_get_child(s->editor,-1),1));assert(!s->editor&&!strcmp(lv_textarea_get_text(s->pitch),"1.25"));calculate_cb(NULL);assert(strstr(lv_label_get_text(s->result),"5.000000"));lv_textarea_set_text(s->count,"0");calculate_cb(NULL);assert(strstr(lv_label_get_text(s->result),"whole count"));
  click(s->pitch);generation++;refresh(NULL);assert(!s->editor&&strstr(lv_label_get_text(s->result),"Printer changed"));close_popup();assert(!s->timer&&!s->pitch);
 }
 width=1024;lv_display_set_resolution(d,1024,600);reset();show_view(VIEW_DISTANCE);click(s->pitch);lv_obj_delete(s->editor);assert(!s->editor&&!s->editor_target);click(s->pitch);lv_obj_delete(s->popup);assert(!s->popup&&!s->timer&&!s->editor);show_view(VIEW_LIMITS);click(lv_obj_get_child(lv_obj_get_child(s->popup,-1),0));assert(!s->popup&&!s->timer);
 s=NULL;puts("PASS: Motion layouts, fields/keyboard/footer bounds, all themes/densities/text sizes, scrolling, calculations, driver/offline/profile states and deletion");return 0;}
