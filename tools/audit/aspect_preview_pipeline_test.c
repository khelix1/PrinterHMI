#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ui_active_print.c"
#include "ui_thumbnail.h"
#include "thumbnail_render.h"
bool custom_theme_color_override(uint32_t a,uint32_t b,uint32_t c,uint32_t *d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_metric_override(int32_t a,int32_t b,int32_t c,int32_t *d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_accent_override(uint8_t a,uint32_t *b){(void)a;(void)b;return false;}
bool custom_theme_surface_opacity(uint8_t *a){(void)a;return false;}
static bool fail_heap;
void *heap_caps_malloc(size_t n,uint32_t caps){(void)caps;return fail_heap?NULL:malloc(n);}
void *heap_caps_calloc(size_t n,size_t size,uint32_t caps){(void)caps;return calloc(n,size);}
void heap_caps_free(void *p){free(p);}
static unsigned lightboxes;
void ui_preview_lightbox_show_file_object(lv_obj_t *object,const char *file){assert(object&&file);lightboxes++;}
static lv_obj_t *card;
static lv_image_dsc_t cached;
#define dash_thumb_canvas s_active_print_thumb_canvas
#define dash_thumb_canvas_buf s_active_print_thumb_canvas_buf
#define DASH_THUMB_CANVAS_W THUMBNAIL_PREVIEW_WIDTH
#define DASH_THUMB_CANVAS_H THUMBNAIL_PREVIEW_HEIGHT
static int dash_thumb_packed_width,dash_thumb_packed_height;
static bool dash_thumb_render_ready,dash_thumb_render_failed,dash_thumb_render_running;
static lv_timer_t *dash_thumb_render_timer;
static uint32_t dash_thumb_render_generation=1;
static int dash_thumb_render_profile_index;
static lv_image_dsc_t raw_source;
static bool locked;
#define TAG "fixture"
#define ESP_LOGW(t,format,...) do {(void)(t);if(0)fprintf(stderr,"W: " format "\n", ##__VA_ARGS__);} while(0)
void vTaskDelete(void *task){(void)task;}
bool bsp_display_lock(unsigned timeout){(void)timeout;assert(!locked);locked=true;return true;}
void bsp_display_unlock(void){assert(locked);locked=false;}
static uint32_t config_generation=1;
uint32_t moonraker_config_generation(void){return config_generation;}
bool thumbnail_manager_has_png(void){return true;}
const lv_image_dsc_t *thumbnail_manager_image_dsc(void){return &raw_source;}
const uint8_t *thumbnail_manager_png_data(void){return (const uint8_t*)"fixture";}
size_t thumbnail_manager_png_size(void){return 7;}
bool printer_preview_cache_publish_active(const char *f,const uint16_t *p,int w,int h){(void)f;(void)p;assert(locked);assert(w==286 && h==165);return true;}
bool printer_preview_store_store_active(const char *f,const uint8_t *p,size_t n){(void)f;(void)p;(void)n;return true;}
static char dash_thumb_render_file[160]="packed.gcode";
bool ui_dashboard_thumb_ready(void){return card && ui_active_print_thumb_box(card);}
uint32_t ui_dashboard_thumb_generation(void){return ui_active_print_thumb_generation();}
static char selected_file[160]="packed.gcode";
char *thumbnail_session_selected_file(void){return selected_file;}
void ui_dashboard_thumb_apply_canvas_from_buffer(int w,int h,const char*f){ui_active_print_thumb_apply_canvas_from_buffer(card,w,h,f);}
lv_obj_t *ui_dashboard_thumb_box(void){return ui_active_print_thumb_box(card);}
void ui_dashboard_thumb_delete_canvas(void){ui_active_print_thumb_delete_canvas();}
void ui_dashboard_thumb_set_placeholder(const char *t){ui_active_print_thumb_set_placeholder(card,t);}
bool ui_dashboard_thumb_ensure_canvas_buffer(size_t n){return ui_active_print_thumb_ensure_canvas_buffer(n);}
void ui_dashboard_thumb_show_canvas_from_buffer(int w,int h,const char *f){ui_active_print_thumb_show_canvas_from_buffer(card,w,h,f);}

char *ui_dashboard_thumb_canvas_file(void){return ui_active_print_thumb_canvas_file();}

int moonraker_config_active_profile_index(void){return 0;}
const lv_image_dsc_t *printer_preview_cache_image(int profile,const char **file,uint32_t *revision){(void)profile;if(file)*file="packed.gcode";if(revision)*revision=1;return &cached;}
static void safe_copy(char *d,size_t n,const char *s){snprintf(d,n,"%s",s);}
bool ui_dashboard_thumb_canvas_matches(const char *file){return ui_active_print_thumb_canvas_matches(file);}
static void (*render_hook)(void);
static bool checked_render(const lv_image_dsc_t *source,uint16_t *out,int w,int h,int *pw,int *ph){bool ok=thumbnail_render_to_rgb565_aspect(source,out,w,h,pw,ph);if(render_hook){void(*hook)(void)=render_hook;render_hook=NULL;hook();}return ok;}
#define thumbnail_render_to_rgb565_aspect checked_render
#include "aspect_dashboard_functions.h"
#undef thumbnail_render_to_rgb565_aspect
static void change_profile(void){config_generation++;}
static void change_file(void){strcpy(selected_file,"other.gcode");}
static void replace_panel(void){lv_obj_delete(card);card=ui_active_print_create_profile(lv_screen_active(),&(ui_dashboard_rect_t){0,0,600,350},&ui_dashboard_layout_profile_for_theme(ui_theme_get_active())->active_content);}

static void flush(lv_display_t *d,const lv_area_t *a,uint8_t *p){(void)a;(void)p;lv_display_flush_ready(d);}
int main(void){
 lv_init();lv_display_t *display=lv_display_create(1024,600);static uint8_t pixels[1024*40*2];lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB565);lv_display_set_buffers(display,pixels,NULL,sizeof(pixels),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(display,flush);
 static uint16_t source[286*215];source[0]=0x4321;
 for(unsigned theme=0;theme<5;theme++)for(unsigned large=0;large<2;large++){
  ui_theme_set_active((ui_theme_id_t)theme);ui_theme_set_accessibility((ui_accessibility_t){.large_text=large});
  card=ui_active_print_create_profile(lv_screen_active(),&(ui_dashboard_rect_t){0,0,600,350},&ui_dashboard_layout_profile_for_theme(ui_theme_get_active())->active_content);active_print_ctx_t *ctx=lv_obj_get_user_data(card);lv_obj_set_size(ctx->preview_box,420,180);
  const int sizes[][2]={{286,165},{124,215},{215,215},{286,165}};
  for(unsigned i=0;i<4;i++){
   int w=sizes[i][0],h=sizes[i][1];cached=(lv_image_dsc_t){.header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_RGB565,.w=w,.h=h,.stride=w*2},.data_size=w*h*2,.data=(uint8_t*)source};
   dashboard_restore_active_profile_preview();assert(dash_thumb_canvas);
   lv_obj_t *previous=dash_thumb_canvas;lv_image_header_t header;assert(lv_image_decoder_get_info(lv_image_get_src(previous),&header)==LV_RESULT_OK);assert(header.w==w && header.h==h && header.stride==w*2);
   assert(((uint16_t*)dash_thumb_canvas_buf)[0]==0x4321);
   dashboard_apply_rendered_thumbnail();assert(dash_thumb_canvas==previous);
   ui_active_print_thumb_apply_canvas_from_buffer(card,w,h,"reused.gcode");assert(dash_thumb_canvas==previous);
   assert(lv_image_decoder_get_info(lv_image_get_src(previous),&header)==LV_RESULT_OK && header.w==w && header.h==h && header.stride==w*2);
   lv_obj_update_layout(card);int scale=lv_image_get_scale(previous);assert((int64_t)w*scale<=lv_obj_get_content_width(ctx->preview_box)*256);assert((int64_t)h*scale<=lv_obj_get_content_height(ctx->preview_box)*256);
  }
  static uint16_t wide[900*520];wide[0]=0x7654;
  raw_source=(lv_image_dsc_t){.header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_RGB565,.w=900,.h=520,.stride=1800},.data_size=sizeof(wide),.data=(uint8_t*)wide};
  dash_thumb_render_generation=1;dash_thumb_render_task(NULL);assert(dash_thumb_render_ready && !dash_thumb_render_failed && !locked);
  assert(dash_thumb_packed_width==286 && dash_thumb_packed_height==165 && dash_thumb_canvas_buf[0]==0x7654);
  dash_thumb_render_generation=2;wide[0]=0x1111;dash_thumb_render_task(NULL);assert(!dash_thumb_render_ready && dash_thumb_canvas_buf[0]==0x7654);wide[0]=0x7654;
  dash_thumb_render_generation=1;uint16_t retained=dash_thumb_canvas_buf[0];wide[0]=0x9999;render_hook=change_profile;dash_thumb_render_task(NULL);assert(!dash_thumb_render_ready&&dash_thumb_canvas_buf[0]==retained);config_generation=1;
  render_hook=change_file;dash_thumb_render_task(NULL);assert(!dash_thumb_render_ready&&dash_thumb_canvas_buf[0]==retained);strcpy(selected_file,"packed.gcode");
  fail_heap=true;dash_thumb_render_task(NULL);assert(!dash_thumb_render_ready&&dash_thumb_canvas_buf[0]==retained);fail_heap=false;
  render_hook=replace_panel;dash_thumb_render_task(NULL);assert(!dash_thumb_render_ready&&dash_thumb_canvas_buf[0]==retained&&!dash_thumb_canvas);
  wide[0]=0x7654;ui_active_print_thumb_forget_canvas_file();locked=true;dashboard_show_loaded_thumbnail();locked=false;assert(dash_thumb_canvas&&dash_thumb_canvas_buf[0]==0x7654);
  dash_thumb_render_ready=true;dash_thumb_render_failed=false;dash_thumb_render_timer=lv_timer_create(dash_thumb_render_ui_poll_cb,100,NULL);lv_timer_t *owned_timer=dash_thumb_render_timer,*other_timer=lv_timer_create(dash_thumb_render_ui_poll_cb,100,NULL);dash_thumb_render_ui_poll_cb(NULL);dash_thumb_render_ui_poll_cb(other_timer);assert(dash_thumb_render_timer==owned_timer);lv_timer_delete(other_timer);dash_thumb_render_ui_poll_cb(owned_timer);assert(!dash_thumb_render_timer);
  cached.header.stride++;dashboard_restore_active_profile_preview();assert(!dash_thumb_canvas);lv_obj_delete(card);s_active_print_thumb_canvas=NULL;heap_caps_free(s_active_print_thumb_canvas_buf);s_active_print_thumb_canvas_buf=NULL;
 }
 lv_display_delete(display);lv_deinit();puts("PASS: production Dashboard restore/apply and Active Print canvas reuse across wide/portrait/square previews, stride/header rebinding, containment, invalid descriptors and five themes/text sizes");
}
