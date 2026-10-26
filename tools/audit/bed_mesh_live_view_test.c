/* Real Bed Mesh page; snapshot/renderer stubs isolate view/action availability. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lvgl.h"
#include "custom_theme.h"
#include "ui_bed_mesh.c"
static moonraker_state_t live;
static uint32_t generation=1;
static unsigned sends,renders;
static bool available;
static float fixture[9]={0,.1f,.2f,.1f,0,-.1f,.2f,.1f,0};
bool custom_theme_color_override(uint32_t a,uint32_t b,uint32_t c,uint32_t*d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_metric_override(int32_t a,int32_t b,int32_t c,int32_t*d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_accent_override(uint8_t a,uint32_t*b){(void)a;(void)b;return false;}
bool custom_theme_surface_opacity(uint8_t*a){(void)a;return false;}
uint32_t moonraker_config_generation(void){return generation;}
void moonraker_state_snapshot(moonraker_state_t *out){*out=live;}
int64_t esp_timer_get_time(void){return 0;}
static bool command(const char *text){(void)text;sends++;return true;}
bool bed_mesh_controller_snapshot(bed_mesh_snapshot_t *out,float *values,size_t capacity,bed_mesh_profile_name_t *names,size_t name_capacity){
 (void)names;(void)name_capacity;memset(out,0,sizeof(*out));if(!available)return false;assert(capacity>=9);memcpy(values,fixture,sizeof(fixture));out->valid=true;out->rows=out->cols=3;out->values=values;out->minimum=-.1;out->maximum=.2;out->range=.3;strcpy(out->profile_name,"printing_mesh");return true;
}
void ui_bed_mesh_profiles_init(lv_obj_t *owner,ui_bed_mesh_profiles_command_cb_t cb){(void)owner;(void)cb;}
void ui_bed_mesh_profiles_update(const bed_mesh_snapshot_t *mesh){(void)mesh;}
void ui_bed_mesh_profiles_refresh_availability(void){}
void ui_bed_mesh_profiles_show_cb(lv_event_t *e){(void)e;}
void ui_bed_mesh_profiles_close(void){}
void ui_bed_mesh_renderer_render(const ui_bed_mesh_renderer_config_t *config){assert(config->mesh.valid&&config->values);renders++;}
void ui_bed_mesh_gestures_init(ui_bed_mesh_view_transform_t *view,ui_bed_mesh_gesture_render_cb_t cb,int32_t w,int32_t h){(void)cb;(void)w;(void)h;memset(view,0,sizeof(*view));}
void ui_bed_mesh_gestures_close(void){}
void ui_bed_mesh_gestures_multitouch_update(uint8_t n,int32_t x,int32_t y,int32_t a,int32_t b){(void)n;(void)x;(void)y;(void)a;(void)b;}
void ui_bed_mesh_gestures_event_cb(lv_event_t *e){(void)e;}
void ui_bed_mesh_gestures_reset(void){}
void ui_bed_mesh_gestures_zoom_in(void){}
void ui_bed_mesh_gestures_zoom_out(void){}
bool ui_bed_mesh_gestures_is_active(void){return false;}
static void flush(lv_display_t *display,const lv_area_t *area,uint8_t *pixels){(void)area;(void)pixels;lv_display_flush_ready(display);}
int main(void){
 lv_init();lv_display_t *display=lv_display_create(1024,600);static uint16_t pixels[1024*50];lv_display_set_buffers(display,pixels,NULL,sizeof(pixels),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB565);lv_display_set_flush_cb(display,flush);
 for(unsigned theme=0;theme<5;theme++){
  ui_theme_set_active(theme);live.moonraker_ok=live.live_data_ok=true;strcpy(live.printer_state,"printing");available=false;unsigned sent=sends;ui_bed_mesh_show(command);assert(!s.mesh.valid&&lv_obj_has_state(s.calibrate_button,LV_STATE_DISABLED));
  available=true;unsigned before=renders;lv_tick_inc(260);lv_timer_handler();assert(s.mesh.valid&&renders==before+1&&strstr(lv_label_get_text(s.stats),"printing_mesh")&&!strstr(lv_label_get_text(s.stats),"wait"));assert(lv_label_get_text(s.metrics)[0]);
  before=renders;lv_tick_inc(260);lv_timer_handler();assert(renders==before);fixture[4]+=.01f;lv_tick_inc(260);lv_timer_handler();assert(renders==before+1&&s.values[4]==fixture[4]);calibrate_cb(NULL);confirm_calibrate_cb(NULL);assert(!s.calibrate_confirm&&sends==sent);
  strcpy(live.printer_state,"paused");mesh_connection_refresh(NULL);assert(s.mesh.valid&&strstr(lv_label_get_text(s.stats),"printing_mesh"));calibrate_cb(NULL);assert(!s.calibrate_confirm&&sends==sent);
  strcpy(live.printer_state,"ready");mesh_connection_refresh(NULL);assert(!lv_obj_has_state(s.calibrate_button,LV_STATE_DISABLED));calibrate_cb(NULL);assert(s.calibrate_confirm);strcpy(live.printer_state,"printing");confirm_calibrate_cb(NULL);assert(sends==sent);close_calibrate_confirm_cb(NULL);
  live.moonraker_ok=false;mesh_connection_refresh(NULL);assert(s.mesh.valid&&strstr(lv_label_get_text(s.stats),"Last reported mesh"));live.moonraker_ok=true;
  generation++;before=renders;fixture[4]+=.01f;mesh_connection_refresh(NULL);assert(renders==before&&strstr(lv_label_get_text(s.stats),"Printer changed")&&lv_obj_has_state(s.calibrate_button,LV_STATE_DISABLED));ui_bed_mesh_close();assert(!s.popup&&!s.values&&!s.buf&&!s.connection_timer);lv_obj_clean(lv_layer_top());
 }
 lv_display_delete(display);lv_deinit();puts("PASS: five-theme live mesh viewing during printing/paused, late mesh arrival, unchanged snapshot suppression, cached/offline labeling, owner fence and calibration command restrictions");
}
