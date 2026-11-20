/* Real Bed Mesh page; snapshot/renderer stubs isolate view/action availability. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lvgl.h"
#include "custom_theme.h"
static bool reject_mesh_timer;
static lv_timer_t *mesh_test_timer(lv_timer_cb_t cb,uint32_t period,void *data)
{
 return reject_mesh_timer ? NULL : lv_timer_create(cb,period,data);
}
#define lv_timer_create mesh_test_timer
#include "ui_bed_mesh.c"
#undef lv_timer_create
static moonraker_state_t live;
static uint32_t generation=1;
static unsigned sends,renders,profiles_opened;
static bool profile_dialog_open;
static bool available, accept_command=true, reenter_command;
static uint32_t revision;
static float fixture[9]={0,.1f,.2f,.1f,0,-.1f,.2f,.1f,0};
bool custom_theme_color_override(uint32_t a,uint32_t b,uint32_t c,uint32_t*d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_metric_override(int32_t a,int32_t b,int32_t c,int32_t*d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_accent_override(uint8_t a,uint32_t*b){(void)a;(void)b;return false;}
bool custom_theme_surface_opacity(uint8_t*a){(void)a;return false;}
uint32_t moonraker_config_generation(void){return generation;}
void moonraker_state_snapshot(moonraker_state_t *out){*out=live;}
int64_t esp_timer_get_time(void){return 0;}
static bool command(const char *text){(void)text;sends++;if(reenter_command)confirm_calibrate_cb(NULL);return accept_command;}
bool bed_mesh_controller_snapshot(bed_mesh_snapshot_t *out,float *values,size_t capacity,bed_mesh_profile_name_t *names,size_t name_capacity){
 (void)names;(void)name_capacity;memset(out,0,sizeof(*out));if(!available)return false;assert(capacity>=9);memcpy(values,fixture,sizeof(fixture));out->valid=true;out->mesh_revision=revision;out->rows=out->cols=3;out->values=values;out->minimum=-.1;out->maximum=.2;out->range=.3;strcpy(out->profile_name,"printing_mesh");return true;
}
void ui_bed_mesh_profiles_init(lv_obj_t *owner,ui_bed_mesh_profiles_command_cb_t cb,void (*sync_mesh)(void)){(void)sync_mesh;(void)owner;(void)cb;}
void ui_bed_mesh_profiles_update(const bed_mesh_snapshot_t *mesh){(void)mesh;}
void ui_bed_mesh_profiles_refresh_availability(void){}
void ui_bed_mesh_profiles_show_cb(lv_event_t *e){(void)e;profiles_opened++;}
bool ui_bed_mesh_profiles_is_open(void){return profile_dialog_open;}
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
  assert(!strcmp(lv_label_get_text(s.stats),"No active mesh yet."));
  live.live_data_ok=false;mesh_connection_refresh(NULL);
  assert(!strcmp(lv_label_get_text(s.stats),"Waiting for Klipper."));
  live.live_data_ok=true;
  available=true;unsigned before=renders;lv_tick_inc(260);lv_timer_handler();assert(s.mesh.valid&&renders==before+1&&strstr(lv_label_get_text(s.stats),"printing_mesh")&&!strstr(lv_label_get_text(s.stats),"wait"));assert(lv_label_get_text(s.metrics)[0]);
  before=renders;lv_tick_inc(260);lv_timer_handler();assert(renders==before);fixture[4]+=.01f;lv_tick_inc(260);lv_timer_handler();assert(renders==before+1&&s.values[4]==fixture[4]);calibrate_cb(NULL);confirm_calibrate_cb(NULL);assert(!s.calibrate_confirm&&sends==sent);
  strcpy(live.printer_state,"paused");mesh_connection_refresh(NULL);assert(s.mesh.valid&&strstr(lv_label_get_text(s.stats),"printing_mesh"));calibrate_cb(NULL);assert(!s.calibrate_confirm&&sends==sent);
  strcpy(live.printer_state,"ready");mesh_connection_refresh(NULL);assert(!lv_obj_has_state(s.calibrate_button,LV_STATE_DISABLED));calibrate_cb(NULL);assert(s.calibrate_confirm);strcpy(live.printer_state,"printing");confirm_calibrate_cb(NULL);assert(sends==sent);close_calibrate_confirm_cb(NULL);
  live.moonraker_ok=false;mesh_connection_refresh(NULL);assert(s.mesh.valid&&!strncmp(lv_label_get_text(s.stats),"Printer offline. Last reported mesh",35));live.moonraker_ok=true;
  generation++;before=renders;fixture[4]+=.01f;mesh_connection_refresh(NULL);assert(renders==before&&strstr(lv_label_get_text(s.stats),"Printer changed")&&lv_obj_has_state(s.calibrate_button,LV_STATE_DISABLED));ui_bed_mesh_close();assert(!s.popup&&!s.values&&!s.buf&&!s.connection_timer);lv_obj_clean(lv_layer_top());
 }
 unsigned sent=sends, opened=profiles_opened;
 strcpy(live.printer_state,"ready");
 reject_mesh_timer=true;ui_bed_mesh_show(command);reject_mesh_timer=false;
 assert(s.mesh.valid&&!s.connection_timer);
 assert(strstr(lv_label_get_text(s.stats),"Live updates unavailable"));
 assert(strstr(lv_label_get_text(s.stats),"Last reported mesh"));
 assert(lv_obj_has_state(s.calibrate_button,LV_STATE_DISABLED));
 assert(lv_obj_has_state(s.profiles_button,LV_STATE_DISABLED));
 calibrate_cb(NULL);confirm_calibrate_cb(NULL);open_profiles_cb(NULL);
 assert(!s.calibrate_confirm&&sends==sent&&profiles_opened==opened);
 ui_bed_mesh_close();ui_bed_mesh_show(command);
 assert(s.connection_timer&&!lv_obj_has_state(s.calibrate_button,LV_STATE_DISABLED));
 open_profiles_cb(NULL);assert(profiles_opened==opened+1);
 confirm_calibrate_cb(NULL);assert(sends==sent);
 ui_bed_mesh_close();
 available=true;
 ui_bed_mesh_show(command);
 unsigned before_request=sends;
 profile_dialog_open=true;mesh_connection_refresh(NULL);
 assert(lv_obj_has_state(s.calibrate_button,LV_STATE_DISABLED));
 calibrate_cb(NULL);assert(!s.calibrate_confirm&&sends==before_request);
 profile_dialog_open=false;mesh_connection_refresh(NULL);
 assert(!lv_obj_has_state(s.calibrate_button,LV_STATE_DISABLED));

 calibrate_cb(NULL);assert(s.calibrate_confirm);
 lv_obj_delete(s.calibrate_confirm);confirm_calibrate_cb(NULL);assert(sends==before_request&&!s.calibrate_confirm);
 calibrate_cb(NULL);accept_command=false;confirm_calibrate_cb(NULL);
 assert(sends==before_request+1&&s.calibrate_confirm&&strstr(lv_label_get_text(s.calibrate_feedback),"not accepted"));
 accept_command=true;reenter_command=true;confirm_calibrate_cb(NULL);reenter_command=false;
 assert(sends==before_request+2&&!s.calibrate_confirm&&calibration.pending);
 calibrate_cb(NULL);assert(!s.calibrate_confirm);
 ui_bed_mesh_close();ui_bed_mesh_show(command);
 mesh_connection_refresh(NULL);assert(calibration.pending&&lv_obj_has_state(s.profiles_button,LV_STATE_DISABLED));
 ++revision;mesh_connection_refresh(NULL);assert(!calibration.pending&&!lv_obj_has_state(s.calibrate_button,LV_STATE_DISABLED));
 calibrate_cb(NULL);confirm_calibrate_cb(NULL);assert(calibration.pending);
 lv_tick_inc(MESH_CALIBRATION_WAIT_MS);mesh_connection_refresh(NULL);
 assert(calibration.expired&&!calibration.pending&&strstr(lv_label_get_text(s.stats),"Check Console"));
 ++revision;mesh_connection_refresh(NULL);assert(!calibration.expired);
 ui_bed_mesh_close();
 lv_display_delete(display);lv_deinit();puts("PASS: five-theme live mesh viewing during printing/paused, late mesh arrival, unchanged snapshot suppression, cached/offline labeling, owner fence and calibration command restrictions");
}
