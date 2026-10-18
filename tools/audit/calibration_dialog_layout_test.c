/* Real calibration owners with typed transport/catalog/session fixtures. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lvgl.h"
#include "custom_theme.h"
#include "moonraker.h"
#include "ui_theme.h"
#include "ui_toast.h"
#include "ui_calibration_dialog.h"
#include "calibration_session_controller.h"
#include "calibration_capability_controller.h"
#include "console_controller.h"
#include "device_catalog_controller.h"
#include "macro_controller.h"
#include "endstop_status_controller.h"
static moonraker_state_t state;
static uint32_t owner=1;
static unsigned sends,steps,queries;
static bool send_accept=true;
static void (*send_hook)(void);
static unsigned result_views, session_resets;
static bool fixture_screws=true,fixture_z_tilt=true,fixture_qgl=true;
static char command[512];
static calibration_session_snapshot_t session;
static calibration_capabilities_t page_caps;
static endstop_status_snapshot_t endstops;
static uint16_t raster[1024*600];
static unsigned render_frame;
static void capture(void);
bool custom_theme_color_override(uint32_t a,uint32_t b,uint32_t c,uint32_t *d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_metric_override(int32_t a,int32_t b,int32_t c,int32_t *d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_accent_override(uint8_t a,uint32_t *b){(void)a;(void)b;return false;}
bool custom_theme_surface_opacity(uint8_t *a){(void)a;return false;}
uint32_t moonraker_config_generation(void){return owner;}
void moonraker_state_snapshot(moonraker_state_t *o){*o=state;}
void ui_toast_show(ui_status_kind_t k,const char *t,const char *d){(void)k;(void)t;(void)d;}
uint32_t console_controller_latest_sequence(void){return 7;}
void console_controller_add_command(const char *s){(void)s;}
void calibration_session_controller_begin(calibration_session_kind_t k,uint32_t seq){session.kind=k;session.start_sequence=seq;session.status=CALIBRATION_SESSION_WAITING;}
void calibration_session_controller_begin_screws_tilt(uint32_t seq){calibration_session_controller_begin(CALIBRATION_SESSION_SCREWS_TILT,seq);}
void calibration_session_controller_mark_error(const char *s){session.status=CALIBRATION_SESSION_ERROR;snprintf(session.results,sizeof(session.results),"%s",s);}
void calibration_session_controller_snapshot(calibration_session_snapshot_t *o){*o=session;}
void calibration_session_controller_reset(void){session_resets++;memset(&session,0,sizeof(session));}
void calibration_session_controller_poll(void){}
void calibration_capability_controller_snapshot(calibration_capabilities_t *o){*o=page_caps;o->screws_tilt=fixture_screws;o->z_tilt=fixture_z_tilt;o->quad_gantry_level=fixture_qgl;}
void device_catalog_controller_status(device_catalog_status_t *o){memset(o,0,sizeof(*o));o->discovered=true;o->stored_count=8;}
bool device_catalog_controller_get(size_t i,device_descriptor_t *o){if(i>=8)return false;memset(o,0,sizeof(*o));snprintf(o->object_name,sizeof(o->object_name),"extruder%u",(unsigned)i);snprintf(o->display_name,sizeof(o->display_name),"Tool %u with a long printer-specific heater name",(unsigned)i);return true;}
void macro_controller_status(macro_controller_status_t *o){memset(o,0,sizeof(*o));o->count=16;o->discovered=true;}
bool macro_controller_get(size_t i,char *o,size_t n){if(i>=16)return false;snprintf(o,n,"CALIBRATE_LONG_PRINTER_DEFINED_MACRO_%u",(unsigned)i);return true;}
int64_t esp_timer_get_time(void){return 1000000;}
bool moonraker_live_websocket_connected(void){return true;}
bool moonraker_live_websocket_request_endstops(uint32_t id){(void)id;queries++;return true;}
void endstop_status_controller_reset(void){memset(&endstops,0,sizeof(endstops));}
uint32_t endstop_status_controller_begin(uint32_t o){endstops.owner_generation=o;endstops.waiting=true;return 1;}
void endstop_status_controller_failed(const char *s){snprintf(endstops.error,sizeof(endstops.error),"%s",s);}
void endstop_status_controller_snapshot(endstop_status_snapshot_t *o){*o=endstops;}
bool send_command(const char *s){sends++;snprintf(command,sizeof(command),"%s",s);if(send_hook)send_hook();return send_accept;}
bool ready(const char *s){(void)s;return state.moonraker_ok&&state.live_data_ok&&strcmp(state.printer_state,"printing")&&strcmp(state.printer_state,"paused")&&strcmp(state.printer_state,"error")&&strcmp(state.printer_state,"shutdown");}
void fixture_results(const char *t,const char *s){(void)t;(void)s;result_views++;}
void refresh_results(void){}
void step_event(lv_event_t *e){steps++;snprintf(command,sizeof(command),"%s",(char *)lv_event_get_user_data(e));}
void abort_event(lv_event_t *e){(void)e;steps++;}
void accept_event(lv_event_t *e){(void)e;steps++;}
static void inside(lv_obj_t *c,lv_obj_t *p){lv_area_t a,b;lv_obj_get_coords(c,&a);lv_obj_get_coords(p,&b);if(a.x1<b.x1||a.x2>b.x2||a.y1<b.y1||a.y2>b.y2){fprintf(stderr,"outside %d,%d..%d,%d vs %d,%d..%d,%d\n",a.x1,a.y1,a.x2,a.y2,b.x1,b.y1,b.x2,b.y2);abort();}}
static void buttons(lv_obj_t *row){for(uint32_t i=0;i<lv_obj_get_child_count(row);i++){lv_obj_t *b=lv_obj_get_child(row,i);if(lv_obj_has_flag(b,LV_OBJ_FLAG_HIDDEN))continue;if(lv_obj_check_type(b,&lv_button_class)){inside(lv_obj_get_child(b,0),b);assert(lv_obj_get_height(b)>=48);lv_area_t a,r;lv_obj_get_coords(b,&a);lv_obj_get_coords(row,&r);assert(a.x1>=r.x1&&a.x2<=r.x2);}else buttons(b);}}
static inline void layout(lv_obj_t *popup){assert(popup);lv_obj_update_layout(popup);inside(popup,lv_screen_active());for(unsigned i=0;i<3;i++)inside(lv_obj_get_child(popup,i),popup);lv_obj_t *body=ui_cal_dialog_body(popup),*footer=lv_obj_get_child(popup,2);buttons(body);buttons(footer);lv_area_t before,after;lv_obj_get_coords(footer,&before);lv_obj_scroll_to_y(body,lv_obj_get_scroll_y(body)+lv_obj_get_scroll_bottom(body),LV_ANIM_OFF);lv_obj_update_layout(popup);assert(lv_obj_get_scroll_bottom(body)<=1);lv_obj_get_coords(footer,&after);assert(!memcmp(&before,&after,sizeof(before)));lv_obj_scroll_to_y(body,0,LV_ANIM_OFF);capture();}
static inline void click(lv_obj_t *o){lv_obj_send_event(o,LV_EVENT_CLICKED,NULL);}
static inline unsigned diagnostic_timers(void){unsigned n=0;lv_timer_t *t=NULL;while((t=lv_timer_get_next(t)))n++;return n;}
static inline lv_obj_t *action(lv_obj_t *p,unsigned i){return lv_obj_get_child(lv_obj_get_child(p,2),i);}
static inline lv_obj_t *choice(lv_obj_t *p,unsigned i){return lv_obj_get_child(lv_obj_get_user_data(ui_cal_dialog_body(p)),i);}
#if defined(TEST_PAGE)
#include "motion_diagnostics_controller.h"
void motion_diagnostics_controller_snapshot(motion_diagnostics_snapshot_t *o){memset(o,0,sizeof(*o));}
bool motion_axis_distance_calculate(const char *p,const char *c,double *d,const char **e){(void)p;(void)c;(void)d;(void)e;return false;}
#include "ui_calibration.c"
static bool overlaps(lv_obj_t *a,lv_obj_t *b){lv_area_t x,y;lv_obj_get_coords(a,&x);lv_obj_get_coords(b,&y);return x.x1<=y.x2&&x.x2>=y.x1&&x.y1<=y.y2&&x.y2>=y.y1;}
static void page_bounds(lv_obj_t *p){
 for(unsigned i=0;i<lv_obj_get_child_count(p);i++){
  lv_obj_t *c=lv_obj_get_child(p,i);if(lv_obj_has_flag(c,LV_OBJ_FLAG_HIDDEN))continue;
  inside(c,p);
  if(lv_obj_check_type(c,&lv_button_class)){
   assert(lv_obj_get_height(c)>=44);
   lv_obj_t *label=lv_obj_get_child(c,0);
   if(label&&lv_obj_check_type(label,&lv_label_class)){
    lv_point_t text_size;
    lv_text_get_size(&text_size,lv_label_get_text(label),lv_obj_get_style_text_font(label,0),lv_obj_get_style_text_letter_space(label,0),0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
    assert(text_size.x<=lv_obj_get_content_width(label));
    assert(text_size.y<=lv_obj_get_height(label));
   }
   for(unsigned j=0;j<lv_obj_get_child_count(p);j++){
    lv_obj_t *other=lv_obj_get_child(p,j);
    if(c==other||lv_obj_has_flag(other,LV_OBJ_FLAG_HIDDEN))continue;
    assert(!overlaps(c,other));
   }
  }
  if(lv_obj_check_type(c,&lv_label_class)){
   for(unsigned j=i+1;j<lv_obj_get_child_count(p);j++){
    lv_obj_t *other=lv_obj_get_child(p,j);
    if(!lv_obj_has_flag(other,LV_OBJ_FLAG_HIDDEN)&&lv_obj_check_type(other,&lv_label_class))assert(!overlaps(c,other));
   }
  }
  page_bounds(c);
 }
}
static void run(void){
 for(unsigned mode=0;mode<3;mode++){
  page_caps=(calibration_capabilities_t){.discovered=mode!=0,.bed_mesh=mode==2,.screws_tilt=true,.z_tilt=true,.quad_gantry_level=true,.axis_twist=mode==2,.input_shaper=mode==2,.accelerometer=mode==2,.hotend_pid=mode==2,.bed_pid=mode==2,.generic_heater_pid=mode==2,.pressure_advance=mode==2,.probe_accuracy=mode==2,.probe=mode==2,.bltouch=mode==2,.calibration_macro_count=mode==2?16:0,.tool_count=mode==2?12:0};
  ui_calibration_show(NULL,send_command);lv_obj_update_layout(s_calibration->root);
  inside(s_calibration->root,lv_screen_active());page_bounds(s_calibration->root);
  ui_calibration_card_refs_t *sections[]={&s_calibration->bed,&s_calibration->motion,&s_calibration->thermal,&s_calibration->probe};
  for(unsigned i=0;i<4;i++){
   lv_obj_t *summary=sections[i]->summary,*section=lv_obj_get_parent(summary);
   if(ui_theme_is_studio()){
    assert(lv_obj_get_style_radius(section,0)==0);
    assert(lv_obj_get_style_border_width(section,0)==0);
    assert(lv_obj_get_style_shadow_width(section,0)==0);
   }
   if(mode==2){
    assert(sections[i]->purpose&&!strcmp(lv_label_get_text(summary),sections[i]->purpose));
    lv_point_t size;lv_text_get_size(&size,sections[i]->purpose,lv_obj_get_style_text_font(summary,0),0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
    assert(size.x<=lv_obj_get_content_width(summary));
   }
   lv_area_t text;lv_obj_get_coords(summary,&text);
   for(unsigned j=0;j<lv_obj_get_child_count(section);j++){
    lv_obj_t *button=lv_obj_get_child(section,j);
    if(!lv_obj_check_type(button,&lv_button_class)||lv_obj_has_flag(button,LV_OBJ_FLAG_HIDDEN))continue;
    lv_area_t bounds;lv_obj_get_coords(button,&bounds);assert(bounds.y1-text.y2>=12);
   }
  }
  capture();assert(!sends);ui_calibration_hide();assert(!lv_obj_get_child_count(lv_layer_top()));
 }
}
#elif defined(TEST_FEEDBACK)
const char *moonraker_config_active_profile_name(void){return "Printer A";}
bool macro_controller_is_favorite(const char *name){(void)name;return false;}
bool macro_controller_toggle_favorite(const char *name){(void)name;return true;}
bool macro_controller_parameters(const char *name,macro_parameter_catalog_t *out){(void)name;memset(out,0,sizeof(*out));out->count=1;strcpy(out->names[0],"TEMP");return true;}
void console_controller_add(console_entry_type_t kind,const char *format,...){(void)kind;(void)format;}
#include "ui_macros.c"
#define TAG estop_fixture_tag
#include "ui_global_estop.c"
#undef TAG
static void run(void){
 ui_macros_show(send_command);click(s_macros->rows[0]);assert(s_confirm);
 lv_textarea_set_text(s_macros->parameter_fields[0],"205");
 lv_textarea_set_text(s_macros->parameter_names[0],"BAD NAME");
 lv_textarea_set_text(s_macros->parameter_fields[1],"1");
 review_macro_cb(NULL);assert(s_confirm&&!sends);
 assert(!strcmp(lv_textarea_get_text(s_macros->parameter_fields[0]),"205"));
 lv_textarea_set_text(s_macros->parameter_names[0],"");lv_textarea_set_text(s_macros->parameter_fields[1],"");
 review_macro_cb(NULL);assert(s_confirm&&strstr(s_macros->pending_command,"TEMP=205"));
 send_accept=false;run_macro_cb(NULL);assert(s_confirm&&sends==1);layout(s_confirm);
 send_accept=true;run_macro_cb(NULL);assert(!s_confirm&&sends==2);
 click(s_macros->rows[0]);owner++;refresh_timer_cb(NULL);assert(s_confirm);unsigned before=sends;
 run_macro_cb(NULL);assert(sends==before);close_confirm();ui_macros_hide();
 assert(ui_global_estop_init(send_command));ui_global_estop_set_printer_name("Printer A");
 lv_obj_t *host=lv_obj_create(lv_screen_active());ui_global_estop_create(host);
 send_accept=false;click(s_estop->button);assert(strstr(command,"M112")&&s_estop->popup);
 assert(strstr(lv_label_get_text(s_estop->title),"NOT CONFIRMED"));layout(s_estop->popup);
 lv_obj_t *footer=lv_obj_get_child(s_estop->popup,2);click(lv_obj_get_child(footer,1));
 assert(strstr(lv_label_get_text(s_estop->message),"RESTART NOT SENT"));
 send_accept=true;click(lv_obj_get_child(footer,1));
 assert(strstr(lv_label_get_text(s_estop->message),"Restart requested"));
 assert(lv_obj_has_state(lv_obj_get_child(footer,1),LV_STATE_DISABLED));layout(s_estop->popup);
 close_popup_cb(NULL);lv_obj_delete(host);
 /* Local feedback registration unwinds through nested popup deletion. */
 lv_obj_t *parent=ui_cal_dialog_create(lv_layer_top(),650,360,UI_POPUP_STANDARD);
 ui_cal_dialog_title(parent,"CALIBRATION");
 lv_obj_t *nested=ui_cal_dialog_create(lv_layer_top(),650,360,UI_POPUP_STANDARD);
 ui_cal_dialog_notice(UI_STATUS_DANGER,"FAILED","Fix the value and retry.");layout(nested);
 lv_obj_delete(nested);ui_cal_dialog_notice(UI_STATUS_WARNING,"RETRY","Previous values are retained.");layout(parent);
 lv_obj_delete(parent);
 ui_cal_dialog_notice(UI_STATUS_WARNING,"UNAVAILABLE","Printer offline.");
 lv_obj_t *fallback=ui_popup_find_owner(lv_obj_get_child(lv_layer_top(),lv_obj_get_child_count(lv_layer_top())-1));
 assert(fallback);layout(fallback);click(action(fallback,0));
}
#elif defined(TEST_OTAFAILURE)
#include "esp_app_desc.h"
#include "esp_ota_ops.h"
static bool running,update_accept;
static unsigned update_starts;
static esp_app_desc_t running_app={.version="6.5.6",.date="Oct 11 2026",.time="13:12:00"};
const esp_app_desc_t *esp_app_get_description(void){return &running_app;}
const esp_partition_t *esp_ota_get_running_partition(void){static esp_partition_t p={.label="ota_0"};return &p;}
const char *ota_manager_get_url(void){return "https://fixture/custom.bin";}
size_t ota_manager_url_capacity(void){return 192;}

void ui_camera_set_setup_active(bool active){(void)active;}
void ui_dashboard_set_camera_quiesced(bool active){(void)active;}
bool camera_stream_busy(void){return true;}
void vTaskDelay(uint32_t ticks){(void)ticks;assert(!"OTA UI must not wait for camera transport");}
bool ota_manager_is_running(void){return running;}
bool ota_manager_start(const char *url){assert(url&&url[0]);update_starts++;return update_accept;}
void ota_manager_set_url(const char *url){(void)url;}
static bool async_rejected;
static lv_result_t reserve_async(lv_async_cb_t cb,void *data){return async_rejected ? LV_RESULT_INVALID : lv_async_call(cb,data);}
#define lv_async_call reserve_async
#define close_cb ota_editor_close_cb
#include "ui_ota_popup.c"
#include "keyboard_layout_checks.h"
#undef close_cb
#include "ota_ui_controller.c"
static ota_release_catalog_snapshot_t catalog;
static unsigned catalog_fetches,chosen_updates,cancel_updates,catalog_reads;
static bool catalog_start_loading;
static void (*catalog_hook)(void);
static char chosen_url[256];
void ota_release_catalog_snapshot(ota_release_catalog_snapshot_t *out){*out=catalog;}
bool ota_release_catalog_start(void){catalog_fetches++;if(catalog_start_loading)catalog.state=OTA_RELEASE_CATALOG_LOADING;return true;}
bool ota_release_catalog_entry(size_t index,ota_release_entry_t *out){
 catalog_reads++;if(catalog_hook){void(*hook)(void)=catalog_hook;catalog_hook=NULL;hook();}
 if(index>=4)return false;
 memset(out,0,sizeof(*out));out->channel=index<2?OTA_RELEASE_CHANNEL_STABLE:OTA_RELEASE_CHANNEL_NIGHTLY;
 out->relation=index==1?OTA_RELEASE_OLDER:OTA_RELEASE_NEWER;
 snprintf(out->version,sizeof(out->version),"nightly-2026-10-10-012345abcdef");
 strcpy(out->notes,"Long release notes describing this update and everything the operator needs to know. Settings and printer profiles are retained. Review the selected build before installing.");
 strcpy(out->published,"2026-10-10");snprintf(out->asset_url,sizeof(out->asset_url),"https://fixture/fw%u.bin",(unsigned)index);out->asset_size=2500000;return true;
}
#include "ui_ota_release_browser.c"
#undef lv_async_call
static void chosen_update(const char *url){chosen_updates++;snprintf(chosen_url,sizeof(chosen_url),"%s",url);}
static void cancel_update(void){cancel_updates++;}
static void retire_catalog(void){browser_close();}
static void replace_catalog(void){browser_close();ui_ota_release_browser_show(chosen_update,NULL);}
static void run(void){
 running=false;update_accept=false;update_starts=0;
 s_ota_popup=ui_popup_create(lv_layer_top(),300,200,UI_POPUP_STANDARD);
 s_ota_url_ta=lv_textarea_create(s_ota_popup);lv_textarea_set_text(s_ota_url_ta,"");s_start_cb=chosen_update;
 start_cb(NULL);assert(s_ota_popup&&s_ota_url_ta&&!update_starts&&!s_deferred_start_popup);
 assert(!strcmp(lv_textarea_get_placeholder_text(s_ota_url_ta),"Firmware URL required."));
 ui_ota_popup_close();
 assert(!ota_ui_controller_start_url("https://fixture/fw.bin",false));assert(s_start_failure&&update_starts==1);
 layout(s_start_failure);inside(s_start_failure,lv_screen_active());
 lv_obj_t *body=ui_ota_body(s_start_failure),*button=lv_obj_get_child(lv_obj_get_child(s_start_failure,2),0);
 inside(button,s_start_failure);buttons(s_start_failure);
 lv_obj_scroll_to_y(body,lv_obj_get_scroll_y(body)+lv_obj_get_scroll_bottom(body),LV_ANIM_OFF);
 lv_obj_update_layout(s_start_failure);assert(lv_obj_get_scroll_bottom(body)<=1);
 click(button);assert(!s_start_failure);
 assert(!ota_ui_controller_start_url("",false)&&s_start_failure&&update_starts==1);start_failure_close(NULL);
 running=true;assert(!ota_ui_controller_start_url("https://fixture/fw.bin",false)&&!s_start_failure&&update_starts==1);
 running=false;update_accept=true;assert(ota_ui_controller_start_url("https://fixture/fw.bin",false)&&!s_start_failure&&update_starts==2);
 ui_ota_start_failure("Retry information");lv_obj_delete(s_start_failure);assert(!s_start_failure);
 /* URL editor and progress keep their native footer reachable at every size. */
 ui_ota_popup_show("https://fixture/fw.bin","Current: 6.5.6. HTTPS verifies the server. Review firmware and keep power connected during installation.",255,chosen_update,NULL);
 layout(s_ota_popup);keyboard_layout_check(s_ota_popup,s_ota_kb,s_ota_url_ta);lv_obj_send_event(s_ota_kb,LV_EVENT_CANCEL,NULL);assert(!s_ota_popup);
 ui_ota_popup_show("https://fixture/fw.bin","Update information",255,chosen_update,NULL);
 unsigned before_updates=chosen_updates;lv_obj_send_event(s_ota_kb,LV_EVENT_READY,NULL);assert(s_deferred_start_popup&&!s_ota_popup);
 lv_obj_delete(s_deferred_start_popup);deferred_start_cb(NULL);assert(chosen_updates==before_updates);
 running=true;ui_ota_progress_show(cancel_update);
 ui_ota_progress_set_transfer("Average: 1234.5 KiB/s | Elapsed: 12345 s");
 assert(strstr(lv_label_get_text(s_progress_transfer_label),"1234.5"));
 ui_ota_progress_pump("Downloading firmware\nKeep power connected",40,1000000,2500000,true);layout(s_progress_popup);
 assert(strstr(lv_label_get_text(s_progress_label),"Keep power connected"));
 /* Fractional progress, mid-animation retargeting and repeated snapshots. */
 assert(lv_bar_get_max_value(s_progress_bar)==10000);
 lv_tick_inc(180);lv_timer_handler();assert(lv_bar_get_value(s_progress_bar)==4000);
 ui_ota_progress_pump("Downloading firmware",41,1025000,2500000,true);
 lv_tick_inc(50);lv_timer_handler();
 int32_t partial=lv_bar_get_value(s_progress_bar);assert(partial>4000&&partial<4100);
 ui_ota_progress_pump("Downloading firmware",41,1027500,2500000,true);
 assert(s_progress_bar_target==4110&&lv_bar_get_value(s_progress_bar)==partial);
 for(unsigned repeat=0;repeat<2;repeat++) {
  lv_tick_inc(50);lv_timer_handler();
  ui_ota_progress_pump("Downloading firmware",41,1027500,2500000,true);
 }
 lv_tick_inc(60);lv_timer_handler();assert(lv_bar_get_value(s_progress_bar)==4110);
 assert(!strcmp(lv_label_get_text(s_progress_pct_label),"41%"));
 ui_ota_progress_pump("Validating firmware",99,2500000,2500000,false);
 lv_tick_inc(180);lv_timer_handler();assert(lv_bar_get_value(s_progress_bar)==9900);
 ui_ota_progress_pump("Downloading firmware",40,1000000,2500000,true);
 unsigned cancelled=cancel_updates;click(s_progress_cancel_btn);assert(cancel_updates==cancelled+1&&lv_obj_has_state(s_progress_cancel_btn,LV_STATE_DISABLED));
 progress_cancel_cb(NULL);ui_ota_progress_show(cancel_update);progress_cancel_cb(NULL);assert(cancel_updates==cancelled+1);
 ui_ota_progress_pump("Cancelling update...",40,1000000,2500000,true);assert(lv_obj_has_state(s_progress_cancel_btn,LV_STATE_DISABLED));
 running=false;ui_ota_progress_pump("Download failed. Check connection and retry.",40,1000000,2500000,false);
 assert(!strcmp(lv_label_get_text(lv_obj_get_child(s_progress_cancel_btn,0)),"Close"));click(s_progress_cancel_btn);assert(!s_progress_popup);
 ui_ota_popup_show("https://fixture/fw.bin","fixture",255,chosen_update,NULL);
 lv_obj_delete(s_ota_url_ta);assert(!s_ota_url_ta&&lv_keyboard_get_textarea(s_ota_kb)==NULL);before_updates=chosen_updates;start_cb(NULL);assert(chosen_updates==before_updates);ui_ota_popup_close();
 running=true;ui_ota_progress_show(cancel_update);lv_obj_delete(s_progress_cancel_btn);assert(!s_progress_cancel_btn);progress_cancel_cb(NULL);ui_ota_progress_pump("Working",20,1,2,true);
 lv_obj_t *deleted_bar=s_progress_bar;lv_obj_delete(s_progress_bar);assert(!s_progress_bar&&!lv_anim_get(deleted_bar,progress_bar_frame));lv_tick_inc(180);lv_timer_handler();ui_ota_progress_pump("Working",30,1,2,true);ui_ota_progress_close();running=false;ui_ota_popup_resume_camera();
 catalog=(ota_release_catalog_snapshot_t){.state=OTA_RELEASE_CATALOG_READY,.count=4};
 ui_ota_release_browser_show(chosen_update,NULL);layout(s_popup);assert(lv_obj_get_child_count(s_list)==4);
 assert(lv_obj_get_scrollbar_mode(s_list)==LV_SCROLLBAR_MODE_OFF);
 if(lv_display_get_horizontal_resolution(NULL)==1024) {
  assert(lv_obj_get_style_text_font(lv_obj_get_child(s_stable_tab,0),0)->line_height>=UI_FONT_BODY->line_height);
  assert(lv_obj_get_style_text_font(lv_obj_get_child(s_nightly_tab,0),0)->line_height>=UI_FONT_BODY->line_height);
 }
 assert(lv_obj_has_state(s_stable_tab,LV_STATE_CHECKED)&&!lv_obj_has_state(s_nightly_tab,LV_STATE_CHECKED));
 lv_obj_t *rows[4];for(unsigned i=0;i<4;i++)rows[i]=lv_obj_get_child(s_list,i);
 unsigned reads=catalog_reads,tab_fetches=catalog_fetches;
 lv_obj_scroll_to_y(s_list,lv_obj_get_scroll_bottom(s_list),LV_ANIM_OFF);
 int32_t stable_scroll=lv_obj_get_scroll_y(s_list);
 for(unsigned cycle=0;cycle<20;cycle++) {
  click(s_nightly_tab);assert(lv_obj_has_flag(rows[0],LV_OBJ_FLAG_HIDDEN));
  assert(!lv_obj_has_flag(rows[2],LV_OBJ_FLAG_HIDDEN));
  click(s_stable_tab);assert(lv_obj_get_scroll_y(s_list)==stable_scroll);
  assert(!lv_obj_has_flag(rows[0],LV_OBJ_FLAG_HIDDEN));
  assert(lv_obj_has_flag(rows[2],LV_OBJ_FLAG_HIDDEN));
  for(unsigned i=0;i<4;i++)assert(rows[i]==lv_obj_get_child(s_list,i));
 }
 assert(catalog_reads==reads && catalog_fetches==tab_fetches);
 click(lv_obj_get_child(s_list,0));assert(s_confirm);
 assert(strstr(lv_label_get_text(lv_obj_get_child(ui_ota_body(s_confirm),0)),"Installed: Custom"));
 assert(strstr(lv_label_get_text(lv_obj_get_child(ui_ota_body(s_confirm),1)),"Selected: Stable"));
 layout(s_confirm);click(lv_obj_get_child(lv_obj_get_child(s_confirm,2),0));assert(!s_confirm&&s_popup);
 click(s_nightly_tab);assert(s_channel==OTA_RELEASE_CHANNEL_NIGHTLY);layout(s_popup);
 click(lv_obj_get_child(s_list,2));layout(s_confirm);before_updates=chosen_updates;click(lv_obj_get_child(lv_obj_get_child(s_confirm,2),1));
 assert(chosen_updates==before_updates+1&&!strcmp(chosen_url,"https://fixture/fw2.bin")&&!s_popup&&!s_confirm&&!s_timer);
 unsigned reopen_fetches=catalog_fetches;
 ui_ota_release_browser_show(chosen_update,NULL);assert(catalog_fetches==reopen_fetches);lv_obj_delete(s_popup);assert(!s_popup&&!s_list&&!s_timer&&!s_selected);
 ui_ota_release_browser_show(chosen_update,NULL);catalog.state=OTA_RELEASE_CATALOG_ERROR;strcpy(catalog.error,"Could not retrieve releases. Check the network.");timer_cb(s_timer);assert(strstr(lv_label_get_text(s_status),"network"));assert(!strcmp(lv_label_get_text(lv_obj_get_child(s_refresh_button,0)),"Retry"));
 catalog_start_loading=true;unsigned fetches=catalog_fetches;click(s_refresh_button);assert(catalog_fetches==fetches+1&&lv_obj_has_state(s_refresh_button,LV_STATE_DISABLED)&&!lv_obj_get_child_count(s_list));refresh_cb(NULL);assert(catalog_fetches==fetches+1);
 catalog.state=OTA_RELEASE_CATALOG_READY;catalog.count=0;timer_cb(s_timer);assert(!lv_obj_has_state(s_refresh_button,LV_STATE_DISABLED)&&strstr(lv_label_get_text(s_status),"No compatible"));
 catalog.state=OTA_RELEASE_CATALOG_IDLE;timer_cb(s_timer);assert(strstr(lv_label_get_text(s_status),"idle"));browser_close();catalog_start_loading=false;
 for(unsigned cycle=0;cycle<12;cycle++){
  ui_ota_release_browser_show(chosen_update,ota_open_custom_url_bridge);
  assert(strstr(lv_label_get_text(lv_obj_get_child(ui_ota_body(s_popup),0)),"Installed: Custom"));
  lv_obj_t *footer=lv_obj_get_child(s_popup,2),*custom_button=lv_obj_get_child(footer,0);
  async_rejected=true;click(custom_button);assert(!s_custom_pending&&s_popup&&!s_ota_popup);
  async_rejected=false;
  click(custom_button);click(custom_button);assert(s_custom_pending&&s_popup&&!s_ota_popup);
  unsigned transition_reads=catalog_reads;timer_cb(s_timer);assert(catalog_reads==transition_reads);
  lv_tick_inc(1);lv_timer_handler();assert(!s_popup&&!s_custom_pending&&s_ota_popup&&s_camera_quiesced);
  layout(s_ota_popup);keyboard_layout_check(s_ota_popup,s_ota_kb,s_ota_url_ta);
  assert(!strcmp(lv_textarea_get_text(s_ota_url_ta),"https://fixture/custom.bin"));
  lv_obj_t *url_footer=lv_obj_get_child(s_ota_popup,-1);
  lv_obj_t *url_button=lv_obj_get_child(url_footer,0),*child=lv_obj_get_child(url_button,0);
  lv_obj_add_flag(child,LV_OBJ_FLAG_EVENT_BUBBLE);lv_obj_send_event(child,LV_EVENT_SIZE_CHANGED,NULL);
  async_rejected=true;start_cb(NULL);assert(s_ota_popup&&!s_deferred_start_popup);
  assert(!strcmp(lv_textarea_get_text(s_ota_url_ta),"https://fixture/custom.bin"));async_rejected=false;
  if(cycle%3==0) {
   remote_cb(NULL);assert(s_remote_pending&&s_ota_popup&&!s_popup);
   lv_tick_inc(1);lv_timer_handler();assert(!s_ota_popup&&!s_remote_pending&&s_popup&&s_camera_quiesced);
   browser_close();
  } else if(cycle%2)lv_obj_delete(s_ota_popup);else ui_ota_popup_close();
  assert(!s_ota_popup&&!s_ota_url_ta&&!s_ota_kb&&!s_camera_quiesced);
 }
 ui_ota_release_browser_show(chosen_update,ota_open_custom_url_bridge);
 click(lv_obj_get_child(lv_obj_get_child(s_popup,2),0));assert(s_custom_pending);
 lv_obj_delete(s_popup);lv_tick_inc(1);lv_timer_handler();assert(!s_popup&&!s_custom_pending&&!s_ota_popup);
 ui_ota_popup_show("https://fixture/fw.bin","Update information",255,chosen_update,NULL);
 before_updates=chosen_updates;start_cb(NULL);assert(s_deferred_start_popup);ui_ota_popup_close();
 lv_tick_inc(1);lv_timer_handler();assert(!s_deferred_start_popup&&chosen_updates==before_updates);
 catalog=(ota_release_catalog_snapshot_t){.state=OTA_RELEASE_CATALOG_READY,.count=4};
 ui_ota_release_browser_show(chosen_update,NULL);catalog_hook=retire_catalog;click(lv_obj_get_child(s_list,0));assert(!s_popup&&!s_confirm&&!s_selected);
 ui_ota_release_browser_show(chosen_update,NULL);catalog_hook=replace_catalog;s_rendered_count=SIZE_MAX;timer_cb(s_timer);assert(s_popup&&s_list&&!s_confirm);timer_cb(s_timer);assert(lv_obj_get_child_count(s_list)==4);
 lv_obj_delete(s_stable_tab);assert(!s_stable_tab);select_tab(OTA_RELEASE_CHANNEL_NIGHTLY);timer_cb(s_timer);
 lv_obj_delete(s_list);assert(!s_list&&!s_timer);timer_cb(NULL);browser_close();
 assert(!strcmp(relation_text(&(ota_release_entry_t){.channel=OTA_RELEASE_CHANNEL_NIGHTLY,.relation=OTA_RELEASE_NEWER}),"Update available"));
 assert(!strcmp(relation_text(&(ota_release_entry_t){.channel=OTA_RELEASE_CHANNEL_STABLE,.relation=OTA_RELEASE_SWITCH_CHANNEL}),"Switch to Stable"));


}
#elif defined(TEST_PID)
#include "ui_calibration_pid.c"
static void pid_reenter(void){run_pid_tune_cb(NULL);}
static void pid_retire_during_send(void){lv_obj_delete(*s_context->popup);}
static void switch_during_send(void){owner++;}
static void pid_warning(void){ui_calibration_pid_event(NULL);click(lv_obj_get_child(ui_cal_dialog_body(*s_context->popup),7));click(choice(*s_context->popup,3));click(action(*s_context->popup,1));}
static void run(void){
 lv_obj_t *popup=NULL,*target_label=NULL;char objects[8][DEVICE_CATALOG_OBJECT_NAME_MAX],names[8][DEVICE_CATALOG_DISPLAY_NAME_MAX];size_t count=0,selected=0;int target=0,minimum=0,maximum=0;
 ui_calibration_pid_context_t context={.popup=&popup,.target_label=&target_label,.object_names=objects,.display_names=names,.heater_capacity=8,.heater_count=&count,.selected_index=&selected,.target=&target,.target_min=&minimum,.target_max=&maximum,.send_gcode=send_command,.show_results=fixture_results,.refresh_results=refresh_results};ui_calibration_pid_init(&context);
 ui_calibration_pid_event(NULL);layout(popup);assert(count==8);click(lv_obj_get_child(ui_cal_dialog_body(popup),7));layout(popup);assert(selected==7&&target==200);click(choice(popup,3));assert(target==210);
 lv_obj_t *initial=popup;ui_calibration_pid_event(NULL);assert(popup==initial&&target==210);
 click(action(popup,1));layout(popup);assert(!sends);click(action(popup,0));layout(popup);assert(target==210);click(action(popup,1));layout(popup);
 send_accept=false;click(action(popup,1));assert(popup&&sends==1&&target==210&&selected==7&&result_views==0);assert(strstr(lv_label_get_text(s_pid_feedback),"retained"));layout(popup);
 click(action(popup,0));assert(target==210);click(action(popup,1));
 send_accept=true;send_hook=pid_reenter;click(action(popup,1));send_hook=NULL;
 assert(!popup&&!target_label&&!s_pid_feedback&&sends==2&&result_views==1&&strstr(command,"HEATER=extruder7 TARGET=210"));
 unsigned before=sends;run_pid_tune_cb(NULL);assert(sends==before);
 for(unsigned i=0;i<10;i++){pid_warning();lv_obj_delete(popup);assert(!popup&&!target_label&&!s_pid_feedback);run_pid_tune_cb(NULL);assert(sends==before);}
 pid_warning();send_hook=pid_retire_during_send;run_pid_tune_cb(NULL);send_hook=NULL;assert(!popup&&sends==before+1&&result_views==1);
 pid_warning();send_hook=switch_during_send;run_pid_tune_cb(NULL);send_hook=NULL;assert(popup&&sends==before+2&&result_views==1);run_pid_tune_cb(NULL);assert(sends==before+2);ui_calibration_pid_close();
 context.heater_capacity=3;ui_calibration_pid_event(NULL);assert(count==3);ui_calibration_pid_close();
 ui_calibration_pid_init(NULL);
}
#elif defined(TEST_MANUAL)
#include "ui_calibration_manual_probe.c"
static unsigned manual_callbacks;
static void reentrant_step(lv_event_t *e){manual_callbacks++;assert(strstr((char*)lv_event_get_user_data(e),"TESTZ Z="));manual_probe_step(e);manual_probe_accept(e);}
static void retiring_accept(lv_event_t *e){(void)e;manual_callbacks++;lv_obj_delete(s_probe.popup);manual_probe_accept(e);ui_calibration_manual_probe_set_status("late callback ignored");}
static void reopen_during_dispatch(lv_event_t *e){(void)e;manual_callbacks++;ui_calibration_manual_probe_hide();assert(!ui_calibration_manual_probe_show("MANUAL PROBE","Draft","ACCEPT",step_event,abort_event,accept_event));}
static void run(void){
 assert(ui_calibration_manual_probe_show("PROBE / Z OFFSET","Use TESTZ steps toward (-) or away (+) from the bed. Use a paper test, then ACCEPT only when the offset is correct.","ACCEPT",step_event,abort_event,accept_event));layout(s_probe.popup);
 for(unsigned i=0;i<10;i++){click(choice(s_probe.popup,i));}
 assert(steps==10&&!strcmp(command,"TESTZ Z=1.0"));
 ui_calibration_manual_probe_set_status("Point accepted. Wait for Klipper to finish moving to the next sample.");lv_obj_t *initial=s_probe.popup;
 assert(ui_calibration_manual_probe_show("PROBE / Z OFFSET","Opening instructions","ACCEPT",step_event,abort_event,accept_event));assert(s_probe.popup==initial&&strstr(lv_label_get_text(s_probe.status_label),"Point accepted"));layout(s_probe.popup);
 owner++;click(choice(s_probe.popup,0));click(action(s_probe.popup,0));click(action(s_probe.popup,1));assert(steps==10);ui_calibration_manual_probe_set_status("stale update");assert(strstr(lv_label_get_text(s_probe.status_label),"Point accepted"));ui_calibration_manual_probe_hide();assert(!s_probe.popup&&!s_probe.status_label&&!s_probe.step_cb);
 for(unsigned i=0;i<10;i++){
  assert(ui_calibration_manual_probe_show("AXIS TWIST CALIBRATION","Adjust each sample point with the paper test, then ACCEPT.","ACCEPT",step_event,abort_event,accept_event));layout(s_probe.popup);lv_obj_delete(s_probe.popup);assert(!s_probe.popup&&!s_probe.status_label&&!s_probe.abort_cb&&!s_probe.accept_cb);ui_calibration_manual_probe_set_status("late update");assert(!ui_calibration_manual_probe_is_visible());manual_probe_step(NULL);manual_probe_abort(NULL);manual_probe_accept(NULL);assert(steps==10);
 }
 manual_callbacks=0;assert(ui_calibration_manual_probe_show("MANUAL PROBE","Adjust height.","ACCEPT",reentrant_step,abort_event,retiring_accept));click(choice(s_probe.popup,0));assert(manual_callbacks==1&&steps==10&&!s_probe_dispatching);click(action(s_probe.popup,1));assert(manual_callbacks==2&&!s_probe.popup&&!s_probe.status_label&&!s_probe_dispatching);
 assert(ui_calibration_manual_probe_show("MANUAL PROBE","Adjust height.","ACCEPT",step_event,abort_event,reopen_during_dispatch));click(action(s_probe.popup,1));assert(manual_callbacks==3&&!s_probe.popup&&!s_probe_dispatching);
 assert(ui_calibration_manual_probe_show("MANUAL PROBE","New workflow.","ACCEPT",step_event,abort_event,accept_event));click(action(s_probe.popup,0));click(action(s_probe.popup,1));assert(steps==12);ui_calibration_manual_probe_hide();
}
#elif defined(TEST_MOTION)
#include "ui_calibration_motion.c"
void ui_motion_diagnostics_create(lv_obj_t *c){(void)c;}
void ui_motion_diagnostics_hide(void){}
static void measurement_reenter(void){run_input_shaper_cb(NULL);run_accelerometer_check_cb(NULL);}
static void measurement_retire(void){lv_obj_delete(s_motion.input_shaper_popup);}
static void measurement_switch(void){owner++;}
static void run(void){
 lv_obj_t *card=lv_obj_create(lv_screen_active());lv_obj_set_size(card,390,190);ui_calibration_motion_create(card,send_command,ready,fixture_results,refresh_results);ui_calibration_motion_refresh(true,true,true);lv_obj_update_layout(card);for(unsigned i=0;i<3;i++){lv_obj_t *b=lv_obj_get_child(card,i);inside(lv_obj_get_child(b,0),b);assert(lv_obj_get_height(lv_obj_get_child(b,0))<=UI_FONT_BODY_LARGE->line_height);}
 click(s_motion.input_shaper_button);layout(s_motion.input_shaper_popup);lv_obj_t *initial=s_motion.input_shaper_popup;click(s_motion.input_shaper_button);assert(initial==s_motion.input_shaper_popup);
 send_accept=false;click(action(s_motion.input_shaper_popup,1));assert(sends==1&&s_motion.input_shaper_popup&&result_views==0&&strstr(lv_label_get_text(s_shaper_feedback),"retained"));layout(s_motion.input_shaper_popup);
 send_accept=true;send_hook=measurement_reenter;click(action(s_motion.input_shaper_popup,1));send_hook=NULL;assert(sends==2&&!s_motion.input_shaper_popup&&!s_shaper_feedback&&result_views==1&&!strcmp(command,"SHAPER_CALIBRATE"));run_input_shaper_cb(NULL);assert(sends==2);
 click(s_motion.resonance_test_button);layout(s_motion.resonance_test_popup);initial=s_motion.resonance_test_popup;click(s_motion.resonance_test_button);assert(initial==s_motion.resonance_test_popup);
 send_accept=false;click(choice(s_motion.resonance_test_popup,0));assert(sends==3&&s_motion.resonance_test_popup&&result_views==1);layout(s_motion.resonance_test_popup);
 send_accept=true;click(choice(s_motion.resonance_test_popup,1));assert(sends==4&&!s_motion.resonance_test_popup&&result_views==2&&!strcmp(command,"TEST_RESONANCES AXIS=Y"));
 click(s_motion.accelerometer_check_button);layout(s_motion.accelerometer_check_popup);send_accept=false;click(action(s_motion.accelerometer_check_popup,1));assert(sends==5&&s_motion.accelerometer_check_popup&&result_views==2);send_accept=true;click(action(s_motion.accelerometer_check_popup,1));assert(sends==6&&!s_motion.accelerometer_check_popup&&result_views==3&&!strcmp(command,"MEASURE_AXES_NOISE"));
 unsigned before=sends;
 for(unsigned i=0;i<10;i++){
  click(s_motion.input_shaper_button);lv_obj_delete(s_motion.input_shaper_popup);assert(!s_motion.input_shaper_popup&&!s_shaper_feedback);run_input_shaper_cb(NULL);
  click(s_motion.resonance_test_button);lv_obj_delete(s_motion.resonance_test_popup);assert(!s_motion.resonance_test_popup&&!s_resonance_feedback);run_resonance_test_cb(NULL);
  click(s_motion.accelerometer_check_button);lv_obj_delete(s_motion.accelerometer_check_popup);assert(!s_motion.accelerometer_check_popup&&!s_sensor_feedback);run_accelerometer_check_cb(NULL);assert(sends==before);
 }
 click(s_motion.input_shaper_button);ui_calibration_motion_refresh(true,false,true);run_input_shaper_cb(NULL);assert(sends==before&&strstr(lv_label_get_text(s_shaper_feedback),"capabilities"));ui_calibration_motion_refresh(true,true,true);close_input_shaper_popup();
 click(s_motion.input_shaper_button);state.live_data_ok=false;run_input_shaper_cb(NULL);assert(sends==before&&s_motion.input_shaper_popup);state.live_data_ok=true;close_input_shaper_popup();
 click(s_motion.input_shaper_button);send_hook=measurement_retire;run_input_shaper_cb(NULL);send_hook=NULL;assert(sends==before+1&&!s_motion.input_shaper_popup&&result_views==3);
 click(s_motion.input_shaper_button);send_hook=measurement_switch;run_input_shaper_cb(NULL);send_hook=NULL;assert(sends==before+2&&s_motion.input_shaper_popup&&result_views==3);run_input_shaper_cb(NULL);assert(sends==before+2);close_input_shaper_popup();
 lv_obj_delete(card);assert(!s_motion.input_shaper_button&&!s_motion.resonance_test_button&&!s_motion.accelerometer_check_button);ui_calibration_motion_refresh(true,true,true);ui_calibration_motion_hide();
}
#elif defined(TEST_PROBE)
#include "ui_probe_accuracy.c"
static unsigned probe_results;
static int probe_dispatch_mode;
void ui_calibration_results_show(const char *t,const char *text){(void)t;(void)text;probe_results++;}
void ui_calibration_results_refresh(void){}
static bool probe_send(const char *command_text)
{
 if(probe_dispatch_mode==1)run_cb(NULL);
 else if(probe_dispatch_mode==2)lv_obj_delete(s_popup);
 else if(probe_dispatch_mode==3)owner++;
 return send_command(command_text);
}
static void run(void){
 probe_results=0;probe_dispatch_mode=0;s_accepted=false;
 lv_obj_t *card=lv_obj_create(lv_screen_active());
 ui_probe_accuracy_create(card,probe_send,ready);ui_probe_accuracy_refresh(true);
 unsigned baseline=diagnostic_timers();
 click(s_button);layout(s_popup);lv_dropdown_set_selected(s_samples,2);
 lv_obj_t *initial=s_popup;lv_timer_t *initial_timer=s_status_timer;
 open_cb(NULL);assert(s_popup==initial && s_status_timer==initial_timer && lv_dropdown_get_selected(s_samples)==2);
 lv_dropdown_open(s_samples);lv_obj_update_layout(s_popup);
 /* Native list's bottom border can lie on the exclusive display edge. */
 lv_area_t area;lv_obj_get_coords(lv_dropdown_get_list(s_samples),&area);
 assert(area.x1>=0 && area.y1>=0 && area.x2<lv_display_get_horizontal_resolution(NULL) && area.y2<=lv_display_get_vertical_resolution(NULL));
 assert(lv_obj_get_style_text_font(lv_dropdown_get_list(s_samples),0)==lv_obj_get_style_text_font(s_samples,0));
 lv_dropdown_close(s_samples);assert(!lv_obj_has_state(s_run,LV_STATE_DISABLED));
 state.homed_axes[0]=0;refresh_status(NULL);assert(lv_obj_has_state(s_run,LV_STATE_DISABLED));run_cb(NULL);assert(!sends);
 strcpy(state.homed_axes,"xyz");refresh_status(NULL);click(s_run);
 assert(sends==1 && !strcmp(command,"PROBE_ACCURACY SAMPLES=20") && !s_popup && !s_status_timer && probe_results==1);
 run_cb(NULL);refresh_status(NULL);assert(sends==1 && diagnostic_timers()==baseline);
 lv_tick_inc(5000);click(s_button);lv_dropdown_set_selected(s_samples,0);send_accept=false;click(s_run);
 assert(sends==2 && s_popup && s_status_timer && lv_dropdown_get_selected(s_samples)==0 && probe_results==1);
 assert(strstr(lv_label_get_text(s_status),"not accepted") && session.status==CALIBRATION_SESSION_ERROR);
 refresh_status(NULL);assert(strstr(lv_label_get_text(s_status),"retry"));
 send_accept=true;click(s_run);assert(sends==3 && !s_popup && probe_results==2 && strstr(command,"SAMPLES=5"));
 for(unsigned cycle=0;cycle<10;cycle++) {
  open_cb(NULL);assert(s_popup && diagnostic_timers()==baseline+1);
  lv_obj_delete(s_popup);assert(!s_popup && !s_status && !s_samples && !s_run && !s_status_timer);
  assert(diagnostic_timers()==baseline);refresh_status(NULL);run_cb(NULL);assert(sends==3);
 }
 open_cb(NULL);owner++;refresh_status(NULL);assert(lv_obj_has_state(s_run,LV_STATE_DISABLED));run_cb(NULL);assert(sends==3);close_popup();
 ui_probe_accuracy_create(card,probe_send,ready);ui_probe_accuracy_refresh(true);lv_tick_inc(5000);probe_dispatch_mode=1;open_cb(NULL);click(s_run);assert(sends==4 && probe_results==3 && !s_popup);
 lv_tick_inc(5000);probe_dispatch_mode=2;open_cb(NULL);click(s_run);assert(sends==5 && probe_results==3 && !s_popup && diagnostic_timers()==baseline);
 lv_tick_inc(5000);probe_dispatch_mode=3;open_cb(NULL);click(s_run);assert(sends==6 && probe_results==3 && s_popup);refresh_status(NULL);assert(lv_obj_has_state(s_run,LV_STATE_DISABLED));close_popup();
 lv_obj_delete(card);assert(!s_button);ui_probe_accuracy_refresh(true);ui_probe_accuracy_hide();assert(diagnostic_timers()==baseline);
}
#elif defined(TEST_ENDSTOP)
#include "ui_endstop_status.c"
static void run(void){
 unsigned baseline=diagnostic_timers();
 ui_endstop_status_show();layout(s_popup);assert(queries==1);
 lv_obj_t *initial=s_popup;lv_timer_t *initial_timer=s_timer;
 ui_endstop_status_show();assert(s_popup==initial && s_timer==initial_timer && queries==1);
 endstops.waiting=false;endstops.valid=true;endstops.count=12;
 for(unsigned i=0;i<12;i++){snprintf(endstops.items[i].name,sizeof(endstops.items[i].name),"stepper_%u_long_endstop_name",i);endstops.items[i].triggered=i%2;}
 refresh(NULL);layout(s_popup);assert(strstr(lv_label_get_text(s_body),"TRIGGERED") && strstr(lv_label_get_text(s_body),"OPEN"));
 strcpy(state.printer_state,"printing");refresh(NULL);assert(queries==1 && strstr(lv_label_get_text(s_body),"paused"));
 owner++;refresh(NULL);assert(strstr(lv_label_get_text(s_body),"changed"));click(action(s_popup,0));assert(!s_popup && !s_timer && diagnostic_timers()==baseline);
 strcpy(state.printer_state,"ready");
 for(unsigned cycle=0;cycle<10;cycle++) {
  unsigned before=queries;ui_endstop_status_show();assert(queries==before+1 && endstops.waiting && diagnostic_timers()==baseline+1);
  lv_obj_delete(s_popup);assert(!s_popup && !s_body && !s_timer && !endstops.waiting && !endstops.valid);
  assert(diagnostic_timers()==baseline);
  /* Simulate a late response and timer invocation after the owner is gone. */
  endstops.valid=true;endstops.count=1;refresh(NULL);assert(queries==before+1);
  ui_endstop_status_close();assert(!endstops.valid && !endstops.count);
 }
}
#elif defined(TEST_RESULTS)
#include "ui_calibration_results.c"
static void save_reenter(void){run_save_config_cb(NULL);}
static void save_retire_confirm(void){lv_obj_delete(*s_context->save_confirm_popup);}
static void save_retire_results(void){lv_obj_delete(*s_context->results_popup);}
static void save_switch_owner(void){owner++;}
static void save_replace_session(void){session.generation++;}
static void complete_session(void){session.status=CALIBRATION_SESSION_RESULTS;session.save_available=session.completed=true;session.generation++;strcpy(session.results,"Probe calibration complete.\nReview all measurements before saving.\n");}
static void results_and_confirm(void){ui_calibration_results_show("CALIBRATION RESULTS","Waiting for Klipper...");complete_session();ui_calibration_results_refresh();click(*s_context->apply_restart_button);}
static void run(void){
 lv_obj_t *popup=NULL,*confirm=NULL,*label=NULL,*apply=NULL;calibration_session_snapshot_t snap;uint32_t gen=0;char display[1200];
 ui_calibration_results_context_t context={.results_popup=&popup,.save_confirm_popup=&confirm,.results_label=&label,.apply_restart_button=&apply,.session_snapshot=&snap,.session_generation=&gen,.display=display,.display_size=sizeof(display),.send_gcode=send_command};ui_calibration_results_init(&context);
 ui_calibration_results_show("CALIBRATION RESULTS","Waiting for Klipper...");layout(popup);assert(lv_obj_has_flag(apply,LV_OBJ_FLAG_HIDDEN));
 complete_session();for(int i=0;i<10;i++)strcat(session.results,"Repeated result line with printer-specific information.\n");ui_calibration_results_refresh();layout(popup);assert(!lv_obj_has_flag(apply,LV_OBJ_FLAG_HIDDEN));
 session.generation++;apply_restart_button_cb(NULL);assert(!confirm&&strstr(lv_label_get_text(s_results_feedback),"Review"));
 click(apply);layout(confirm);lv_obj_t *initial=confirm;apply_restart_button_cb(NULL);assert(confirm==initial);
 click(action(confirm,0));assert(!confirm&&popup&&!sends);click(apply);
 send_accept=false;click(action(confirm,1));assert(sends==1&&popup&&confirm&&!session_resets&&!s_save_requested);assert(strstr(lv_label_get_text(s_save_feedback),"retained"));layout(confirm);
 state.live_data_ok=false;ui_calibration_results_refresh();assert(lv_obj_has_state(apply,LV_STATE_DISABLED)&&lv_obj_has_state(s_save_action,LV_STATE_DISABLED));run_save_config_cb(NULL);assert(sends==1);
 state.live_data_ok=true;ui_calibration_results_refresh();assert(!lv_obj_has_state(s_save_action,LV_STATE_DISABLED)&&!lv_obj_has_state(apply,LV_STATE_DISABLED));assert(!lv_label_get_text(s_save_feedback)[0]);
 strcpy(state.printer_state,"printing");ui_calibration_results_refresh();run_save_config_cb(NULL);assert(sends==1&&strstr(lv_label_get_text(s_save_feedback),"print"));strcpy(state.printer_state,"ready");ui_calibration_results_refresh();
 for(unsigned i=0;i<3;i++){
  strcpy(state.printer_state,(const char*[]){"paused","error","shutdown"}[i]);ui_calibration_results_refresh();run_save_config_cb(NULL);assert(sends==1&&lv_obj_has_state(s_save_action,LV_STATE_DISABLED));
 }
 strcpy(state.printer_state,"ready");ui_calibration_results_refresh();
 session.generation++;ui_calibration_results_refresh();assert(lv_obj_has_state(s_save_action,LV_STATE_DISABLED));run_save_config_cb(NULL);assert(sends==1&&strstr(lv_label_get_text(s_save_feedback),"changed"));click(action(confirm,0));click(apply);
 send_accept=true;send_hook=save_reenter;click(action(confirm,1));send_hook=NULL;
 assert(sends==2&&!strcmp(command,"SAVE_CONFIG")&&popup&&confirm&&session_resets==1&&s_save_requested);layout(confirm);assert(strstr(lv_label_get_text(s_save_feedback),"accepted"));assert(lv_obj_has_flag(apply,LV_OBJ_FLAG_HIDDEN));assert(lv_obj_has_state(action(confirm,1),LV_STATE_DISABLED));
 run_save_config_cb(NULL);apply_restart_button_cb(NULL);ui_calibration_results_refresh();assert(sends==2&&session_resets==1&&lv_obj_has_flag(apply,LV_OBJ_FLAG_HIDDEN));
 close_save_confirm_popup();assert(!confirm&&popup);apply_restart_button_cb(NULL);assert(!confirm);ui_calibration_results_close();assert(!popup&&!label&&!apply&&!s_results_feedback&&!s_save_feedback&&!s_save_action);
 unsigned before=sends,resets=session_resets;
 for(unsigned i=0;i<10;i++){
  results_and_confirm();assert(confirm);lv_obj_delete(confirm);assert(!confirm&&!s_save_feedback&&!s_save_action&&popup);run_save_config_cb(NULL);assert(sends==before);
  click(apply);assert(confirm);lv_obj_delete(popup);assert(!popup&&!confirm&&!label&&!apply&&!s_results_feedback);ui_calibration_results_refresh();run_save_config_cb(NULL);assert(sends==before);
 }
 results_and_confirm();send_hook=save_retire_confirm;run_save_config_cb(NULL);send_hook=NULL;assert(!confirm&&popup&&sends==before+1&&session_resets==resets);ui_calibration_results_close();
 results_and_confirm();send_hook=save_retire_results;run_save_config_cb(NULL);send_hook=NULL;assert(!confirm&&!popup&&sends==before+2&&session_resets==resets);
 results_and_confirm();send_hook=save_replace_session;run_save_config_cb(NULL);send_hook=NULL;assert(popup&&confirm&&sends==before+3&&session_resets==resets);ui_calibration_results_refresh();assert(lv_obj_has_state(s_save_action,LV_STATE_DISABLED));ui_calibration_results_close();
 results_and_confirm();send_hook=save_switch_owner;run_save_config_cb(NULL);send_hook=NULL;assert(popup&&confirm&&sends==before+4&&session_resets==resets);ui_calibration_results_refresh();assert(!confirm&&lv_obj_has_flag(apply,LV_OBJ_FLAG_HIDDEN)&&strstr(lv_label_get_text(s_results_feedback),"Printer changed"));run_save_config_cb(NULL);assert(sends==before+4);ui_calibration_results_close();
 /* A recreated view renders the same session generation immediately. */
 complete_session();ui_calibration_results_show("CALIBRATION RESULTS","Waiting...");ui_calibration_results_refresh();assert(strstr(lv_label_get_text(label),"Probe calibration complete"));ui_calibration_results_close();ui_calibration_results_show("CALIBRATION RESULTS","Waiting...");ui_calibration_results_refresh();assert(strstr(lv_label_get_text(label),"Probe calibration complete"));ui_calibration_results_close();
 ui_calibration_results_init(NULL);
}
#elif defined(TEST_PA)
#include "ui_calibration_pressure_advance.c"
static void pa_reenter(void){click(choice(s_pa.popup,0));}
static void pa_retire_during_send(void){lv_obj_delete(s_pa.popup);}
static void pa_switch_during_send(void){owner++;}
static void run(void){
 lv_obj_t *card=lv_obj_create(lv_screen_active());ui_calibration_pressure_advance_create(card,send_command,ready);ui_calibration_pressure_advance_refresh(true,true);
 click(s_pa.button);layout(s_pa.popup);lv_obj_t *initial=s_pa.popup;click(s_pa.button);assert(s_pa.popup==initial);
 send_accept=false;click(choice(s_pa.popup,1));assert(s_pa.popup==initial&&!s_pa.armed&&sends==1);assert(strstr(lv_label_get_text(s_pa.feedback),"Retry"));layout(s_pa.popup);
 send_accept=true;send_hook=pa_reenter;click(choice(s_pa.popup,1));send_hook=NULL;assert(sends==2&&strstr(command,"FACTOR=.020")&&s_pa.armed);layout(s_pa.popup);click(s_pa.button);assert(s_pa.armed);click(action(s_pa.popup,0));assert(!s_pa.popup&&!s_pa.feedback);
 for(unsigned i=0;i<10;i++){click(s_pa.button);lv_obj_delete(s_pa.popup);assert(!s_pa.popup&&!s_pa.feedback&&!s_pa.armed);}
 click(s_pa.button);send_hook=pa_retire_during_send;click(choice(s_pa.popup,0));send_hook=NULL;assert(!s_pa.popup&&sends==3);
 click(s_pa.button);send_hook=pa_switch_during_send;click(choice(s_pa.popup,0));send_hook=NULL;assert(s_pa.popup&&!s_pa.armed&&sends==4);click(choice(s_pa.popup,0));assert(sends==4);close_popup();
 click(s_pa.button);state.live_data_ok=false;click(choice(s_pa.popup,0));assert(sends==4);state.live_data_ok=true;close_popup();
 lv_obj_delete(card);assert(!s_pa.button);ui_calibration_pressure_advance_refresh(true,true);ui_calibration_pressure_advance_hide();
}
#elif defined(TEST_CUSTOM)
#include "ui_calibration_custom.c"
static void custom_reenter(void){run_custom_macro_cb(NULL);}
static void custom_retire_during_send(void){lv_obj_delete(*s_custom->popup);}
static void custom_switch_during_send(void){owner++;}
static void custom_review(void){ui_calibration_custom_event(NULL);click(lv_obj_get_child(ui_cal_dialog_body(*s_custom->popup),15));}
static void run(void){
 lv_obj_t *popup=NULL;char names[16][MACRO_CONTROLLER_NAME_MAX];size_t count=0,selected=0;
 ui_calibration_custom_context_t context={.popup=&popup,.names=names,.names_capacity=16,.count=&count,.selected=&selected,.send=send_command,.ready=ready,.show_results=fixture_results,.refresh_results=refresh_results};
 ui_calibration_custom_init(&context);ui_calibration_custom_event(NULL);layout(popup);assert(count==16);click(lv_obj_get_child(ui_cal_dialog_body(popup),15));layout(popup);
 lv_obj_t *initial=popup;ui_calibration_custom_event(NULL);assert(popup==initial&&selected==15);
 send_accept=false;click(action(popup,1));assert(sends==1&&popup==initial&&selected==15&&!result_views);assert(strstr(lv_label_get_text(s_custom_feedback),"retained"));layout(popup);
 send_accept=true;send_hook=custom_reenter;click(action(popup,1));send_hook=NULL;assert(sends==2&&!popup&&result_views==1&&strstr(command,"MACRO_15"));
 unsigned before=sends;run_custom_macro_cb(NULL);assert(sends==before);
 for(unsigned i=0;i<10;i++){custom_review();lv_obj_delete(popup);assert(!popup&&!s_custom_feedback);run_custom_macro_cb(NULL);assert(sends==before);}
 custom_review();send_hook=custom_retire_during_send;run_custom_macro_cb(NULL);send_hook=NULL;assert(!popup&&sends==before+1&&result_views==1);
 custom_review();send_hook=custom_switch_during_send;run_custom_macro_cb(NULL);send_hook=NULL;assert(popup&&sends==before+2&&result_views==1);run_custom_macro_cb(NULL);assert(sends==before+2);ui_calibration_custom_close();
 context.names_capacity=3;ui_calibration_custom_event(NULL);assert(count==3);ui_calibration_custom_close();ui_calibration_custom_init(NULL);
}
#elif defined(TEST_GEOMETRY)
#include "ui_calibration_geometry.c"
static void geometry_reenter(void){run_screws_tilt_cb(NULL);run_gantry_level_cb(NULL);}
static void geometry_retire_screws(void){lv_obj_delete(*s_geometry->screws_popup);}
static void geometry_retire_gantry(void){lv_obj_delete(*s_geometry->gantry_popup);}
static void geometry_switch(void){owner++;}
static void run(void){
 lv_obj_t *screws=NULL,*gantry=NULL,*label=NULL;bool home=false,ghome=false,qgl=false;char text[1200];uint32_t gen=0;calibration_session_snapshot_t snap;
 ui_calibration_geometry_context_t context={.screws_popup=&screws,.gantry_popup=&gantry,.screws_results_label=&label,.screws_home_required=&home,.gantry_home_required=&ghome,.gantry_use_qgl=&qgl,.screws_display=text,.screws_display_size=sizeof(text),.session_snapshot=&snap,.session_generation=&gen,.send_gcode=send_command};ui_calibration_geometry_init(&context);
 ui_calibration_geometry_screws_event(NULL);layout(screws);lv_obj_t *initial=screws;ui_calibration_geometry_screws_event(NULL);assert(screws==initial);
 send_accept=false;click(action(screws,1));assert(sends==1&&screws&&!label&&strstr(lv_label_get_text(s_screws_feedback),"Retry"));layout(screws);
 state.live_data_ok=false;run_screws_tilt_cb(NULL);assert(sends==1&&strstr(lv_label_get_text(s_screws_feedback),"Klipper"));state.live_data_ok=true;
 strcpy(state.printer_state,"printing");run_screws_tilt_cb(NULL);assert(sends==1);strcpy(state.printer_state,"ready");fixture_screws=false;run_screws_tilt_cb(NULL);assert(sends==1);fixture_screws=true;
 strcpy(state.homed_axes,"");run_screws_tilt_cb(NULL);assert(sends==1&&strstr(lv_label_get_text(s_screws_feedback),"homed"));close_screws_popup();ui_calibration_geometry_screws_event(NULL);assert(home);send_accept=true;send_hook=geometry_reenter;click(action(screws,1));send_hook=NULL;assert(sends==2&&label&&!strcmp(command,"G28\nSCREWS_TILT_CALCULATE"));layout(screws);
 session.status=CALIBRATION_SESSION_RESULTS;session.generation++;strcpy(session.results,"Front left: CW 00:10\nRear right: CCW 00:05");ui_calibration_geometry_refresh();assert(strstr(lv_label_get_text(label),"Front left"));run_screws_tilt_cb(NULL);assert(sends==2);owner++;ui_calibration_geometry_refresh();assert(strstr(lv_label_get_text(label),"Printer changed"));close_screws_popup();strcpy(state.homed_axes,"xyz");
 unsigned before=sends;
 for(unsigned i=0;i<10;i++){ui_calibration_geometry_screws_event(NULL);lv_obj_delete(screws);assert(!screws&&!label&&!s_screws_feedback);run_screws_tilt_cb(NULL);ui_calibration_geometry_refresh();assert(sends==before);}
 ui_calibration_geometry_screws_event(NULL);send_hook=geometry_retire_screws;run_screws_tilt_cb(NULL);send_hook=NULL;assert(!screws&&!label&&sends==before+1);
 ui_calibration_geometry_screws_event(NULL);send_hook=geometry_switch;run_screws_tilt_cb(NULL);send_hook=NULL;assert(screws&&!label&&sends==before+2);run_screws_tilt_cb(NULL);assert(sends==before+2);close_screws_popup();
 ui_calibration_geometry_gantry_event(NULL);layout(gantry);initial=gantry;ui_calibration_geometry_gantry_event(NULL);assert(gantry==initial&&qgl);send_accept=false;run_gantry_level_cb(NULL);assert(sends==before+3&&gantry&&!s_gantry_requested&&strstr(lv_label_get_text(s_gantry_feedback),"Retry"));layout(gantry);
 fixture_qgl=false;run_gantry_level_cb(NULL);assert(sends==before+3);fixture_qgl=true;send_accept=true;send_hook=geometry_reenter;run_gantry_level_cb(NULL);send_hook=NULL;assert(sends==before+4&&s_gantry_requested&&!strcmp(command,"QUAD_GANTRY_LEVEL"));layout(gantry);assert(lv_obj_has_flag(action(gantry,1),LV_OBJ_FLAG_HIDDEN));run_gantry_level_cb(NULL);assert(sends==before+4);close_gantry_level_popup();
 fixture_qgl=false;strcpy(state.homed_axes,"");ui_calibration_geometry_gantry_event(NULL);assert(!qgl&&ghome);run_gantry_level_cb(NULL);assert(sends==before+5&&!strcmp(command,"G28\nZ_TILT_ADJUST"));close_gantry_level_popup();fixture_qgl=true;strcpy(state.homed_axes,"xyz");
 for(unsigned i=0;i<10;i++){ui_calibration_geometry_gantry_event(NULL);lv_obj_delete(gantry);assert(!gantry&&!s_gantry_feedback&&!s_gantry_requested);run_gantry_level_cb(NULL);assert(sends==before+5);}
 ui_calibration_geometry_gantry_event(NULL);send_hook=geometry_retire_gantry;run_gantry_level_cb(NULL);send_hook=NULL;assert(!gantry&&sends==before+6);
 ui_calibration_geometry_gantry_event(NULL);send_hook=geometry_switch;run_gantry_level_cb(NULL);send_hook=NULL;assert(gantry&&!s_gantry_requested&&sends==before+7);run_gantry_level_cb(NULL);assert(sends==before+7);ui_calibration_geometry_close();ui_calibration_geometry_init(NULL);
}
#endif
static void flush(lv_display_t *d,const lv_area_t *a,uint8_t *p){uint16_t *v=(uint16_t *)p;for(int y=a->y1;y<=a->y2;y++)for(int x=a->x1;x<=a->x2;x++)raster[y*1024+x]=*v++;lv_display_flush_ready(d);}
static void capture(void)
{
 const char *folder=getenv("CALIBRATION_SCREENSHOTS");
 if(!folder||lv_display_get_horizontal_resolution(NULL)!=1024||ui_theme_get_density()!=1
#ifndef TEST_PAGE
 ||!ui_theme_get_accessibility().large_text
#endif
 ){return;}

#ifdef TEST_OTAFAILURE
 lv_tick_inc(250);lv_timer_handler();
#endif
 lv_refr_now(NULL);char path[512];snprintf(path,sizeof(path),"%s/%s-theme%u-large%u-frame%u.ppm",folder,TEST_NAME,(unsigned)ui_theme_get_active(),ui_theme_get_accessibility().large_text,render_frame++);FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n1024 600\n255\n");for(unsigned i=0;i<1024*600;i++){uint16_t v=raster[i];unsigned char c[3]={(v>>11)*255/31,((v>>5)&63)*255/63,(v&31)*255/31};fwrite(c,1,3,f);}fclose(f);}
int main(void){lv_init();lv_display_t *d=lv_display_create(1024,600);static uint8_t pixels[1024*50*2];lv_display_set_buffers(d,pixels,NULL,sizeof(pixels),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);lv_display_set_flush_cb(d,flush);
 for(unsigned t=0;t<5;t++)for(unsigned density=0;density<3;density++)for(unsigned large=0;large<2;large++)for(unsigned narrow=0;narrow<3;narrow++){
#ifdef TEST_PAGE
 if(narrow || (t!=UI_THEME_OPERATOR && t!=UI_THEME_STUDIO_DARK))continue;
#endif
 lv_display_set_resolution(d,(int[]){1024,640,480}[narrow],narrow==2?400:600);ui_theme_set_active(t);ui_theme_set_density(density);ui_theme_set_accessibility((ui_accessibility_t){.large_text=large});memset(&state,0,sizeof(state));state.moonraker_ok=state.live_data_ok=true;strcpy(state.printer_state,"ready");strcpy(state.homed_axes,"xyz");strcpy(state.active_hotend,"extruder");owner++;render_frame=0;sends=steps=queries=0;send_accept=true;send_hook=NULL;result_views=session_resets=0;fixture_screws=fixture_z_tilt=fixture_qgl=true;memset(&session,0,sizeof(session));run();assert(!lv_obj_get_child_count(lv_layer_top()));}
 lv_display_delete(d);lv_deinit();
#ifdef TEST_PAGE
 puts("PASS: actual Calibration page, Studio Dark/Operator, all densities/text sizes, discovery/capability states, bounds, label/button separation, 44px actions and flat Studio sections");
#else
 puts("PASS: real calibration owner, five themes/densities/text sizes/viewports, pinned actions, scroll reach and command flow");
#endif
 return 0;}
