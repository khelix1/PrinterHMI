#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "thumbnail_render.h"
static bool test_render(const lv_image_dsc_t *,uint16_t *,int,int,int*,int*);
static void test_layout(lv_obj_t *);
static void test_delete(lv_obj_t *);
#define thumbnail_render_to_rgb565_aspect test_render
#define lv_obj_update_layout test_layout
#define lv_obj_delete test_delete
#include "ui_thumbnail.c"
#undef thumbnail_render_to_rgb565_aspect
#undef lv_obj_update_layout
#undef lv_obj_delete
#include "custom_theme.h"
static ui_thumbnail_t *owned;
static size_t bytes;
static bool fail_memory,fail_render;
static unsigned opened;
static char opened_file[160];
static void (*render_hook)(void),(*layout_hook)(void),(*open_hook)(void),(*delete_hook)(void);
static uint16_t pixels[8]={0x1234,0x1234,0x1234,0x1234,0x1234,0x1234,0x1234,0x1234};
static const lv_image_dsc_t image={.header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_RGB565,.w=4,.h=2,.stride=8},.data_size=16,.data=(uint8_t*)pixels};
typedef struct {size_t size;max_align_t alignment;} allocation_t;
void *heap_caps_malloc(size_t size,uint32_t caps){assert(caps==(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));if(fail_memory)return NULL;allocation_t*p=malloc(sizeof(*p)+size);assert(p);p->size=size;bytes+=size;return p+1;}
void heap_caps_free(void *buffer){if(buffer){allocation_t*p=(allocation_t*)buffer-1;assert(bytes>=p->size);bytes-=p->size;free(p);}}
static void hook(void (**slot)(void)){if(*slot){void(*callback)(void)=*slot;*slot=NULL;callback();}}
static bool test_render(const lv_image_dsc_t *source,uint16_t *buffer,int width,int height,int*outw,int*outh){bool result=!fail_render&&thumbnail_render_to_rgb565_aspect(source,buffer,width,height,outw,outh);hook(&render_hook);return result;}
static void test_layout(lv_obj_t *object){lv_obj_update_layout(object);hook(&layout_hook);}
static void test_delete(lv_obj_t *object){lv_obj_delete(object);hook(&delete_hook);}
void ui_preview_lightbox_show_file_object(lv_obj_t *object,const char *file){assert(object&&lv_image_get_src(object));opened++;snprintf(opened_file,sizeof(opened_file),"%s",file);hook(&open_hook);}
bool custom_theme_color_override(uint32_t a,uint32_t b,uint32_t c,uint32_t*d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_metric_override(int32_t a,int32_t b,int32_t c,int32_t*d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_accent_override(uint8_t a,uint32_t*b){(void)a;(void)b;return false;}
bool custom_theme_surface_opacity(uint8_t*a){(void)a;return false;}
static void create(void){assert(!owned);ui_thumbnail_bind_owner(ui_thumbnail_create(lv_screen_active(),20,20,270,250),&owned);assert(owned&&owned->box&&owned->label);}
static void destroy(void){lv_obj_delete(owned->box);assert(!owned);}
static void delete_canvas(void){lv_obj_delete(owned->canvas);}
static void destroy_buffered(void){assert(bytes);destroy();assert(bytes);/* Active rendering still retains the buffer. */}
static void nested_update(void){ui_thumbnail_set_placeholder(owned,"nested");ui_thumbnail_show_file_image(owned,&image,0,"nested.gcode");}
static lv_point_t touch;static bool pressed;
static void read_touch(lv_indev_t *input,lv_indev_data_t *data){(void)input;data->point=touch;data->state=pressed?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED;}
static void tap(lv_indev_t *input,lv_obj_t *object){lv_obj_update_layout(lv_screen_active());lv_area_t area;lv_obj_get_coords(object,&area);touch=(lv_point_t){(area.x1+area.x2)/2,(area.y1+area.y2)/2};pressed=true;lv_indev_read(input);pressed=false;lv_indev_read(input);}
static void checks(lv_indev_t *input)
{
 create();unsigned before=opened;tap(input,owned->box);assert(opened==before);
 lv_obj_delete(owned->label);assert(!owned->label);ui_thumbnail_set_placeholder(owned,"THUMBNAIL");assert(owned->label);
 ui_thumbnail_show_file_image(owned,&image,0,"shown.gcode");assert(owned->canvas&&!owned->label&&bytes==286*215*2);
 tap(input,owned->canvas);assert(opened==before+1&&!strcmp(opened_file,"shown.gcode"));
 lv_obj_delete(owned->canvas);assert(!owned->canvas);ui_thumbnail_show_file_image(owned,&image,0,"new.gcode");assert(owned->canvas);
 ui_thumbnail_t *same=owned;ui_thumbnail_bind_owner(same,&owned);assert(owned==same);
 before=opened;lv_obj_add_state(owned->box,LV_STATE_DISABLED);lv_obj_send_event(owned->box,LV_EVENT_CLICKED,NULL);assert(opened==before);lv_obj_remove_state(owned->box,LV_STATE_DISABLED);
 lv_obj_t *fake=lv_button_create(owned->box);lv_obj_add_flag(fake,LV_OBJ_FLAG_EVENT_BUBBLE);lv_obj_send_event(fake,LV_EVENT_CLICKED,NULL);assert(opened==before);lv_obj_delete(fake);
 fake=lv_button_create(lv_screen_active());lv_obj_add_event_cb(fake,thumbnail_clicked_cb,LV_EVENT_CLICKED,owned);lv_obj_send_event(fake,LV_EVENT_CLICKED,NULL);assert(opened==before);lv_obj_delete(fake);
 lv_obj_add_flag(owned->canvas,LV_OBJ_FLAG_HIDDEN);lv_obj_send_event(owned->box,LV_EVENT_CLICKED,NULL);assert(opened==before);lv_obj_remove_flag(owned->canvas,LV_OBJ_FLAG_HIDDEN);
 render_hook=nested_update;ui_thumbnail_show_file_image(owned,&image,0,"outer.gcode");assert(!strcmp(owned->loaded_file,"outer.gcode")&&owned->canvas);
 ui_thumbnail_show_file_image(owned,&image,0,owned->loaded_file);assert(!strcmp(owned->loaded_file,"outer.gcode"));
 ui_thumbnail_show_file_image(owned,&image,0,NULL);lv_obj_send_event(owned->box,LV_EVENT_CLICKED,NULL);assert(!opened_file[0]);
 char long_file[170];memset(long_file,'x',sizeof(long_file)-1);long_file[169]=0;ui_thumbnail_show_file_image(owned,&image,0,long_file);assert(!owned->loaded_file[0]);
 fail_render=true;ui_thumbnail_show_file_image(owned,&image,0,"fail.gcode");assert(!owned->canvas&&owned->label&&!strcmp(lv_label_get_text(owned->label),"PREVIEW UNAVAILABLE"));fail_render=false;
 ui_thumbnail_show_file_image(owned,&image,256,"again.gcode");assert(owned->canvas);open_hook=destroy;lv_obj_send_event(owned->box,LV_EVENT_CLICKED,NULL);assert(!owned&&!bytes);
 create();fail_memory=true;ui_thumbnail_show_file_image(owned,&image,0,NULL);assert(owned->label&&!owned->canvas&&!bytes);fail_memory=false;destroy();
 create();render_hook=destroy_buffered;ui_thumbnail_show_file_image(owned,&image,0,NULL);assert(!owned&&!bytes);
 create();delete_hook=destroy;ui_thumbnail_show_file_image(owned,&image,0,NULL);assert(!owned&&!bytes);
 create();ui_thumbnail_show_file_image(owned,&image,0,NULL);delete_hook=destroy;ui_thumbnail_set_placeholder(owned,"close");assert(!owned&&!bytes);
 create();ui_thumbnail_t *old=owned;owned=NULL;create();ui_thumbnail_t *replacement=owned;lv_obj_delete(old->box);assert(owned==replacement);destroy();
 /* The generic fit helpers must drop their temporary deletion hooks. */
 create();ui_thumbnail_show_file_image(owned,&image,0,NULL);unsigned box_events=lv_obj_get_event_count(owned->box),image_events=lv_obj_get_event_count(owned->canvas);
 for(unsigned i=0;i<3;i++){ui_thumbnail_fit_object(owned->canvas,owned->box,4,2,0);}
 assert(box_events==lv_obj_get_event_count(owned->box)&&image_events==lv_obj_get_event_count(owned->canvas));destroy();
 create();ui_thumbnail_show_file_image(owned,&image,0,NULL);layout_hook=destroy;
 ui_thumbnail_fit_object(owned->canvas,owned->box,4,2,0);assert(!owned&&!bytes);

 create();layout_hook=delete_canvas;ui_thumbnail_show_file_image(owned,&image,0,NULL);assert(owned&&!owned->canvas);ui_thumbnail_show_file_image(owned,&image,0,NULL);assert(owned->canvas);destroy();assert(!bytes);
 create();layout_hook=destroy;ui_thumbnail_show_file_image(owned,&image,0,NULL);assert(!owned&&!bytes);
}
int main(void){lv_init();lv_display_t *display=lv_display_create(1024,600);lv_indev_t *input=lv_indev_create();lv_indev_set_type(input,LV_INDEV_TYPE_POINTER);lv_indev_set_read_cb(input,read_touch);
 for(int theme=0;theme<5;theme++)for(int density=0;density<3;density++)for(int large=0;large<2;large++){ui_theme_set_active((ui_theme_id_t)theme);ui_theme_set_density((ui_density_id_t)density);ui_theme_set_accessibility((ui_accessibility_t){.large_text=large});checks(input);}
 lv_indev_delete(input);lv_display_delete(display);lv_deinit();puts("PASS: owned thumbnail slots, child retirement, native pinned-file routing, nested updates, render/delete/layout interruption, generic fit guards and bounded allocation cleanup across all themes/densities/text sizes");}
