#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lvgl.h"
static void canvas_set(lv_obj_t *,void *,int32_t,int32_t,lv_color_format_t);
static void label_set(lv_obj_t *,const char *);
#define lv_canvas_set_buffer canvas_set
#define lv_label_set_text label_set
#include "ui_active_print.c"
#undef lv_canvas_set_buffer
#undef lv_label_set_text
#include "thumbnail_preview_coordinator.c"
#include "custom_theme.h"
static lv_obj_t *panel;
static void (*operation_hook)(void),(*lightbox_hook)(void);
static size_t bytes;static unsigned lightboxes;
static char last_file[160];
typedef struct {size_t size;max_align_t alignment;} allocation_t;
void *heap_caps_malloc(size_t size,uint32_t caps){(void)caps;allocation_t*p=malloc(sizeof(*p)+size);assert(p);p->size=size;bytes+=size;return p+1;}
void heap_caps_free(void *ptr){if(ptr){allocation_t*p=(allocation_t*)ptr-1;bytes-=p->size;free(p);}}
static void hook(void (**slot)(void)){if(*slot){void(*fn)(void)=*slot;*slot=NULL;fn();}}
static void canvas_set(lv_obj_t *object,void *buf,int32_t w,int32_t h,lv_color_format_t cf){lv_canvas_set_buffer(object,buf,w,h,cf);hook(&operation_hook);}
static void label_set(lv_obj_t *object,const char *text){lv_label_set_text(object,text);hook(&operation_hook);}
void ui_preview_lightbox_show_file_object(lv_obj_t *object,const char *file){assert(object);lightboxes++;snprintf(last_file,sizeof(last_file),"%s",file);hook(&lightbox_hook);assert(!strcmp(file,last_file));}
bool custom_theme_color_override(uint32_t a,uint32_t b,uint32_t c,uint32_t*d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_metric_override(int32_t a,int32_t b,int32_t c,int32_t*d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_accent_override(uint8_t a,uint32_t*b){(void)a;(void)b;return false;}
bool custom_theme_surface_opacity(uint8_t*a){(void)a;return false;}
static void create(void){panel=ui_active_print_create(lv_screen_active(),0,0,650,350);assert(panel);}
static void destroy(void){lv_obj_delete(panel);panel=NULL;}
static void replace(void){destroy();create();}
static void nested(void){ui_active_print_set(panel,"nested","00:01","REM 00:01");ui_active_print_thumb_apply_canvas_from_buffer(panel,4,2,"nested.gcode");}
static lv_point_t touch;static bool down;
static void read_pointer(lv_indev_t *input,lv_indev_data_t *data){(void)input;data->point=touch;data->state=down?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED;}
static void tap(lv_indev_t *input,lv_obj_t *object){lv_obj_update_layout(panel);lv_area_t area;lv_obj_get_coords(object,&area);touch=(lv_point_t){(area.x1+area.x2)/2,(area.y1+area.y2)/2};down=true;lv_indev_read(input);down=false;lv_indev_read(input);}
static lv_obj_t *retired_slots[4];
static void replace_slots(lv_event_t *event){(void)event;for(unsigned i=0;i<4;i++)retired_slots[i]=lv_obj_create(lv_screen_active());}
static int64_t preview_now;
static bool preview_running;
static thumbnail_manager_result_t preview_result;
static unsigned metadata_calls, start_calls, free_calls;
int64_t esp_timer_get_time(void){return preview_now;}
bool thumbnail_manager_task_running(void){return preview_running;}
thumbnail_manager_result_t thumbnail_manager_result(void){return preview_result;}
void thumbnail_manager_set_force_refresh(bool force){assert(force);}
bool printer_controller_is_live_state(const char *state){return !strcmp(state,"printing")||!strcmp(state,"paused");}
static void refresh_metadata(const char *file,char *out,size_t size){assert(!strcmp(file,"same.gcode")||!strcmp(file,"next.gcode"));snprintf(out,size,"metadata");metadata_calls++;}
static void start_preview(void){start_calls++;}
static void free_preview(void){free_calls++;}
static void cached_live_preview_checks(void){
 char selected[160]="",path[160]="old.png",dashboard_file[160]="same.gcode",printer_file[160]="other.gcode",metadata[160]="";
 lv_obj_t *dashboard=lv_canvas_create(lv_screen_active()),*printer=lv_canvas_create(lv_screen_active());
 uint16_t pixels[4]={0x1234,0x5678,0x9abc,0xdef0};lv_canvas_set_buffer(dashboard,pixels,2,2,LV_COLOR_FORMAT_RGB565);
 lv_obj_t *dashboard_image=lv_image_create(lv_screen_active()),*printer_image=lv_image_create(lv_screen_active()),*kept=dashboard;
 thumbnail_preview_coordinator_context_t context={.printer_state="printing",.printer_file="same.gcode",.moonraker_host="printer.local",.moonraker_port=7125,.selected_print_file=selected,.selected_print_file_size=sizeof(selected),.selected_thumbnail_path=path,.dashboard_canvas_file=dashboard_file,.printer_canvas_file=printer_file,.dashboard_canvas=&dashboard,.dashboard_image=&dashboard_image,.printer_canvas=&printer,.printer_image=&printer_image,.metadata_info=metadata,.metadata_info_size=sizeof(metadata),.build_metadata=refresh_metadata,.start_delayed=start_preview,.free_thumbnail=free_preview};
 thumbnail_preview_coordinator_reset();preview_result=THUMBNAIL_MANAGER_RESULT_FAILED;preview_now=0;
 thumbnail_preview_coordinator_update(&context);
 assert(dashboard==kept&&!printer&&!dashboard_image&&!printer_image&&!strcmp(dashboard_file,"same.gcode")&&!printer_file[0]&&pixels[0]==0x1234&&metadata_calls==1&&start_calls==1&&free_calls==1);
 thumbnail_preview_coordinator_update(&context);assert(start_calls==1&&dashboard==kept);
 context.printer_state="paused";preview_now=5000001;thumbnail_preview_coordinator_update(&context);assert(start_calls==2&&dashboard==kept);
 preview_result=THUMBNAIL_MANAGER_RESULT_READY;preview_now=10000002;thumbnail_preview_coordinator_update(&context);assert(start_calls==2&&dashboard==kept);
 context.printer_file="next.gcode";thumbnail_preview_coordinator_update(&context);assert(!dashboard&&!dashboard_file[0]&&start_calls==3&&!strcmp(selected,"next.gcode"));
 /* A changed profile invalidates the name even for identical filenames. */
 dashboard=lv_canvas_create(lv_screen_active());dashboard_file[0]=0;context.printer_file="same.gcode";thumbnail_preview_coordinator_reset();thumbnail_preview_coordinator_update(&context);assert(!dashboard&&!dashboard_file[0]&&start_calls==4);
 thumbnail_preview_coordinator_reset();
}
int main(void)
{
 lv_init();lv_display_t *display=lv_display_create(1024,600);lv_indev_t *input=lv_indev_create();lv_indev_set_type(input,LV_INDEV_TYPE_POINTER);lv_indev_set_read_cb(input,read_pointer);
 assert(!ui_active_print_thumb_ensure_canvas_buffer(0));assert(!ui_active_print_thumb_ensure_canvas_buffer(286*215+1));assert(ui_active_print_thumb_ensure_canvas_buffer(8));assert(bytes==286*215*2);
 for(unsigned theme=0;theme<5;theme++)for(unsigned density=0;density<3;density++)for(unsigned large=0;large<2;large++){
  ui_theme_set_active(theme);ui_theme_set_density(density);ui_theme_set_accessibility((ui_accessibility_t){.large_text=large});create();active_print_ctx_t *ctx=lv_obj_get_user_data(panel);
  ui_active_print_thumb_apply_canvas_from_buffer(panel,4,2,"shown.gcode");assert(ctx->canvas==s_active_print_thumb_canvas);unsigned before=lightboxes;tap(input,ctx->canvas);assert(lightboxes==before+1&&!strcmp(last_file,"shown.gcode"));
  lv_obj_add_state(ctx->preview_box,LV_STATE_DISABLED);lv_obj_send_event(ctx->preview_box,LV_EVENT_CLICKED,NULL);assert(lightboxes==before+1);lv_obj_remove_state(ctx->preview_box,LV_STATE_DISABLED);
  lv_obj_t*fake=lv_button_create(ctx->preview_box);lv_obj_add_flag(fake,LV_OBJ_FLAG_EVENT_BUBBLE);lv_obj_send_event(fake,LV_EVENT_CLICKED,NULL);assert(lightboxes==before+1);lv_obj_delete(fake);
  lv_obj_delete(ctx->canvas);assert(!ctx->canvas&&!s_active_print_thumb_canvas);ui_active_print_thumb_apply_canvas_from_buffer(panel,4,2,"again.gcode");assert(ctx->canvas);
  lv_obj_t *canvas=ctx->canvas;ui_active_print_thumb_apply_canvas_from_buffer(panel,0,2,"bad");ui_active_print_thumb_apply_canvas_from_buffer(panel,287,2,"bad");assert(ctx->canvas==canvas);
  operation_hook=nested;ui_active_print_thumb_apply_canvas_from_buffer(panel,4,2,"outer.gcode");assert(!strcmp(s_active_print_thumb_canvas_file,"outer.gcode"));
  lightbox_hook=destroy;lv_obj_send_event(ctx->preview_box,LV_EVENT_CLICKED,NULL);assert(!panel&&!s_active_print_panel&&!s_active_print_thumb_canvas);
  create();operation_hook=replace;ui_active_print_thumb_apply_canvas_from_buffer(panel,4,2,"retired.gcode");assert(s_active_print_panel==panel&&!s_active_print_thumb_canvas);destroy();
  create();operation_hook=destroy;ui_active_print_thumb_set_placeholder(panel,"close during label update");assert(!s_active_print_panel);
  create();ctx=lv_obj_get_user_data(panel);if(ctx->remaining){lv_obj_delete(ctx->remaining);assert(!ctx->remaining);}lv_obj_delete(ctx->footer);assert(!ctx->footer);ui_active_print_set(panel,"2/10","00:01","REM 00:04");lv_obj_delete(ctx->preview_label);assert(!ctx->preview_label);ui_active_print_thumb_set_placeholder(panel,"gone");ui_active_print_thumb_apply_canvas_from_buffer(panel,4,2,"leaf.gcode");lv_obj_delete(ctx->preview_box);assert(!ctx->preview_box&&!ctx->canvas&&!s_active_print_thumb_canvas);ui_active_print_thumb_apply_canvas_from_buffer(panel,4,2,"no box");destroy();
  create();lv_obj_t *old=panel;create();lv_obj_t *replacement=panel;ui_active_print_thumb_apply_canvas_from_buffer(panel,4,2,"new.gcode");lv_obj_delete(old);assert(s_active_print_panel==replacement&&s_active_print_thumb_canvas);destroy();
 }
 cached_live_preview_checks();
 for(unsigned i=0;i<4;i++)retired_slots[i]=lv_obj_create(lv_screen_active());
 lv_obj_add_event_cb(retired_slots[0],replace_slots,LV_EVENT_DELETE,NULL);
 thumbnail_preview_coordinator_context_t context={.dashboard_canvas=&retired_slots[0],.dashboard_image=&retired_slots[1],.printer_canvas=&retired_slots[2],.printer_image=&retired_slots[3]};retire_preview_objects(&context,false,false);for(unsigned i=0;i<4;i++){assert(retired_slots[i]);lv_obj_delete(retired_slots[i]);}
 heap_caps_free(s_active_print_thumb_canvas_buf);s_active_print_thumb_canvas_buf=NULL;assert(!bytes);lv_indev_delete(input);lv_display_delete(display);lv_deinit();puts("PASS: actual Active Print native routing, child/root ownership, interrupted/nested updates, bounded persistent buffer, replaced panels and coordinator replacement-slot preservation across all themes/densities/text sizes");
}
