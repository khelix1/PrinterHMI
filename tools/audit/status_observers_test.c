#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ui_status_banner.c"
bool custom_theme_color_override(uint32_t a,uint32_t b,uint32_t c,uint32_t *d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_metric_override(int32_t a,int32_t b,int32_t c,int32_t *d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_accent_override(uint8_t a,uint32_t *b){(void)a;(void)b;return false;}
bool custom_theme_surface_opacity(uint8_t *a){(void)a;return false;}
static unsigned state_notifications, message_notifications;
static void state_changed(lv_observer_t *o,lv_subject_t *s){(void)o;(void)s;state_notifications++;}
static void message_changed(lv_observer_t *o,lv_subject_t *s){(void)o;(void)s;message_notifications++;}
static void flush(lv_display_t *d,const lv_area_t *a,uint8_t *p){(void)a;(void)p;lv_display_flush_ready(d);}
int main(void){
 lv_init();lv_display_t *d=lv_display_create(1024,600);static uint8_t pixels[1024*50*2];
 lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);lv_display_set_buffers(d,pixels,NULL,sizeof(pixels),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(d,flush);
 for(unsigned theme=0;theme<3;theme++)for(unsigned large=0;large<2;large++){
  ui_theme_set_active((ui_theme_id_t)theme);ui_theme_set_accessibility((ui_accessibility_t){.large_text=large});
  for(unsigned cycle=0;cycle<5;cycle++){
   lv_obj_t *parent=lv_obj_create(lv_screen_active());
   lv_obj_t *a=ui_status_banner_create(parent,0,0,854,54);
   lv_obj_t *b=ui_status_banner_create(parent,0,60,854,54);
   status_banner_ctx_t *ctx=lv_obj_get_user_data(a);assert(ctx && ctx->state_bound && ctx->message_bound);
   lv_subject_add_observer(&ctx->state_subject,state_changed,NULL);
   lv_subject_add_observer(&ctx->message_subject,message_changed,NULL);
   ui_status_banner_set(a,"READY","NETWORK LINKED",NULL,NULL);
   assert(!strcmp(lv_label_get_text(ctx->state),"READY"));assert(!strcmp(lv_label_get_text(ctx->file),"NETWORK LINKED"));
   unsigned states=state_notifications,messages=message_notifications;
   for(unsigned i=0;i<100;i++)ui_status_banner_set(a,"READY","NETWORK LINKED",NULL,NULL);
   assert(state_notifications==states && message_notifications==messages);
   ui_status_banner_set(a,"PRINTING","part_100%.gcode","12:34","37 %");
   assert(state_notifications==states+1 && message_notifications==messages+1);
   assert(!strcmp(lv_label_get_text(ctx->file),"part_100%.gcode"));assert(lv_bar_get_value(ctx->bar)==37);
   assert(lv_color_eq(lv_obj_get_style_text_color(ctx->state,0),ui_status_color(UI_STATUS_OK)));
   ui_status_banner_set(b,"OFFLINE","NETWORK OFFLINE",NULL,NULL);
   assert(!strcmp(lv_label_get_text(ctx->state),"PRINTING"));
   ui_status_banner_set(a,"PAUSED",NULL,NULL,NULL);assert(lv_obj_has_flag(ctx->file,LV_OBJ_FLAG_HIDDEN));
   ui_status_banner_set(a,"PAUSED","",NULL,NULL);assert(!strcmp(lv_label_get_text(ctx->file),""));
   ui_status_banner_set(a,NULL,"Moonraker: reconnecting...",NULL,NULL);
   assert(!strcmp(lv_label_get_text(ctx->state),"--"));assert(!lv_obj_has_flag(ctx->file,LV_OBJ_FLAG_HIDDEN));
   char long_text[600];memset(long_text,'X',sizeof(long_text)-1);long_text[599]=0;
   ui_status_banner_set(a,"READY","short",NULL,NULL);
   ui_status_banner_set(a,long_text,long_text,NULL,NULL);
   assert(!strcmp(lv_label_get_text(ctx->state),long_text));assert(!strcmp(lv_label_get_text(ctx->file),long_text));
   ui_status_banner_set(a,"READY","short",NULL,NULL); /* subject did not change during fallback */
   assert(!strcmp(lv_label_get_text(ctx->state),"READY"));assert(!strcmp(lv_label_get_text(ctx->file),"short"));
   lv_label_set_text(ctx->state,"compatibility writer");
   ui_status_banner_set(a,"READY","short",NULL,NULL);assert(!strcmp(lv_label_get_text(ctx->state),"READY"));
   ctx->state_bound=false;ctx->message_bound=false; /* missing-binding fallback */
   ui_status_banner_set(a,"ERROR","failed",NULL,NULL);assert(!strcmp(lv_label_get_text(ctx->state),"ERROR"));
   ctx->state_bound=true;ctx->message_bound=true;
   lv_refr_now(d);
   lv_obj_delete(parent); /* deinit before child cleanup */
  }
 }
 /* Widget deletion removes its object-bound observer before subject teardown. */
 lv_obj_t *parent=lv_obj_create(lv_screen_active());lv_obj_t *banner=ui_status_banner_create(parent,0,0,854,54);
 status_banner_ctx_t *ctx=lv_obj_get_user_data(banner);lv_obj_delete(ctx->file);ctx->file=NULL;
 lv_subject_copy_string(&ctx->message_subject,"after child deletion");lv_obj_delete(parent);
 lv_display_delete(d);lv_deinit();
 puts("PASS: production banner observers, repeated-value silence, state/message independence, themes/text sizes, percentages, null/hidden text, long/fallback repair and repeated parent/child teardown");
}
