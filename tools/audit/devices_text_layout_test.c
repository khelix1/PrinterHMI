#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "custom_theme.h"
#include "moonraker.h"
#include "ui_devices_catalog_view.c"
static device_descriptor_t entries[96];static device_catalog_status_t status;
static moonraker_state_t state;
void device_catalog_controller_status(device_catalog_status_t*out){*out=status;}
bool device_catalog_controller_get(size_t i,device_descriptor_t*out){if(i>=status.stored_count)return false;*out=entries[i];return true;}
bool device_catalog_controller_has_live_value_source(const char*n){(void)n;return true;}
const char*device_catalog_kind_label(device_kind_t k){const char*names[]={"THERMAL","AIR","POWER","SENSOR","OUTPUT","MOTION","OTHER"};return names[k];}
void moonraker_state_snapshot(moonraker_state_t*out){*out=state;}
void moonraker_filament_state_snapshot(moonraker_filament_state_t*out){memset(out,0,sizeof(*out));}
bool custom_theme_color_override(uint32_t a,uint32_t b,uint32_t c,uint32_t *d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_metric_override(int32_t a,int32_t b,int32_t c,int32_t *d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_accent_override(uint8_t a,uint32_t *b){(void)a;(void)b;return false;}
bool custom_theme_surface_opacity(uint8_t *a){(void)a;return false;}
static uint16_t raster[1024*600];static int width=1024;
static void flush(lv_display_t *d,const lv_area_t *a,uint8_t *p){uint16_t *v=(uint16_t*)p;for(int y=a->y1;y<=a->y2;y++)for(int x=a->x1;x<=a->x2;x++)raster[y*width+x]=*v++;lv_display_flush_ready(d);}
static void snapshot(int theme,const char *name){const char *folder=getenv("MOTION_LAYOUT_SCREENSHOTS");if(!folder)return;lv_refr_now(NULL);char path[512];snprintf(path,sizeof(path),"%s/%s-theme%d.ppm",folder,name,theme);FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n1024 600\n255\n");for(int i=0;i<1024*600;i++){uint16_t v=raster[i];unsigned char c[3]={(v>>11)*255/31,((v>>5)&63)*255/63,(v&31)*255/31};fwrite(c,1,3,f);}fclose(f);}
static void inside(lv_obj_t *child,lv_obj_t *parent){lv_area_t a,b;lv_obj_get_coords(child,&a);lv_obj_get_coords(parent,&b);if(!(a.x1>=b.x1&&a.x2<=b.x2&&a.y1>=b.y1&&a.y2<=b.y2)){fprintf(stderr,"outside %d,%d..%d,%d vs %d,%d..%d,%d\n",a.x1,a.y1,a.x2,a.y2,b.x1,b.y1,b.x2,b.y2);abort();}}

static void single(lv_obj_t*l,bool require_fit){lv_obj_update_layout(l);const lv_font_t*f=lv_obj_get_style_text_font(l,0);assert(lv_obj_get_height(l)==f->line_height);if(require_fit){lv_point_t n;lv_text_get_size(&n,lv_label_get_text(l),f,0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);assert(n.x<=lv_obj_get_width(l));}}
int main(void){lv_init();lv_display_t*d=lv_display_create(1024,600);static uint8_t pixels[1024*50*2];lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);lv_display_set_buffers(d,pixels,NULL,sizeof(pixels),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(d,flush);
ui_devices_catalog_state_t storage={0};s_devices=&storage;
for(int theme=0;theme<5;theme++)for(int density=0;density<3;density++)for(int large=0;large<2;large++){
ui_theme_set_active(theme);ui_theme_set_density(density);ui_theme_set_accessibility((ui_accessibility_t){.large_text=large});memset(&status,0,sizeof(status));status.discovered=true;status.stored_count=status.total_object_count=96;status.kind_count[DEVICE_KIND_THERMAL]=96;status.generation++;
for(int i=0;i<96;i++){snprintf(entries[i].display_name,sizeof(entries[i].display_name),"Extruder chamber temperature sensor %d",i);snprintf(entries[i].object_name,sizeof(entries[i].object_name),"temperature_sensor chamber_%d",i);entries[i].kind=DEVICE_KIND_THERMAL;entries[i].live_value_valid=true;strcpy(entries[i].live_value,"205.0 / 205.0 C  ACTIVE");}
lv_obj_t*root=lv_obj_create(lv_screen_active());lv_obj_remove_style_all(root);lv_obj_set_size(root,ui_theme_is_studio()?976:854,ui_theme_is_studio()?424:528);lv_obj_t*banner=lv_label_create(root);lv_obj_set_width(banner,190);ui_devices_catalog_view_create(root,banner);lv_obj_update_layout(root);
for(int i=0;i<8;i++){lv_obj_t*b=s_devices->filter_buttons[i];assert(lv_obj_get_height(b)>=44);if(ui_theme_is_studio())inside(b,s_devices->filter_strip);single(lv_obj_get_child(b,0),true);}
for(int i=0;i<12;i++){lv_obj_t*card=s_devices->rows[i].card;assert(card);lv_obj_t*labels[]={s_devices->rows[i].name,s_devices->rows[i].object,s_devices->rows[i].kind,s_devices->rows[i].value};for(int j=0;j<4;j++){single(labels[j],j>=2);inside(labels[j],card);}lv_area_t a,b;lv_obj_get_coords(labels[2],&a);lv_obj_get_coords(labels[3],&b);assert(a.x2<b.x1);}
if(ui_theme_is_studio()){inside(s_devices->filter_strip,root);inside(s_devices->list,root);inside(s_devices->previous_button,root);inside(s_devices->next_button,root);assert(lv_obj_get_width(s_devices->list)==792 && lv_obj_get_height(s_devices->list)==304 && lv_obj_has_flag(s_devices->list,LV_OBJ_FLAG_SCROLLABLE));assert(s_devices->page_count==8);
lv_obj_scroll_to_y(s_devices->list,10000,LV_ANIM_OFF);lv_obj_update_layout(root);inside(s_devices->rows[11].card,s_devices->list);assert(lv_obj_get_scroll_y(s_devices->list)>0);
lv_obj_send_event(s_devices->next_button,LV_EVENT_CLICKED,NULL);lv_obj_update_layout(root);assert(s_devices->page_index==1 && lv_obj_get_scroll_y(s_devices->list)==0);assert(strstr(lv_label_get_text(s_devices->rows[0].name),"sensor 12"));}
if(density==1&&large){snapshot(theme,"devices");}status.stored_count=0;status.generation++;ui_devices_catalog_view_refresh();lv_obj_update_layout(root);assert(s_devices->empty);inside(s_devices->empty,s_devices->list);ui_devices_catalog_view_close();assert(!s_devices->refresh_timer&&!s_devices->root);lv_obj_delete(root);}
s_devices=NULL;puts("PASS: Devices category counts, complete live readings, long names, card bounds, empty state and cleanup across all stock themes/densities/text sizes");return 0;}
