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
static char command[512];
static calibration_session_snapshot_t session;
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
void calibration_session_controller_reset(void){memset(&session,0,sizeof(session));}
void calibration_session_controller_poll(void){}
void calibration_capability_controller_snapshot(calibration_capabilities_t *o){memset(o,0,sizeof(*o));o->screws_tilt=o->z_tilt=o->quad_gantry_level=true;}
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
bool send_command(const char *s){sends++;snprintf(command,sizeof(command),"%s",s);return send_accept;}
bool ready(const char *s){(void)s;return state.moonraker_ok&&state.live_data_ok&&strcmp(state.printer_state,"printing")&&strcmp(state.printer_state,"paused")&&strcmp(state.printer_state,"error")&&strcmp(state.printer_state,"shutdown");}
void fixture_results(const char *t,const char *s){(void)t;(void)s;}
void refresh_results(void){}
void step_event(lv_event_t *e){steps++;snprintf(command,sizeof(command),"%s",(char *)lv_event_get_user_data(e));}
void abort_event(lv_event_t *e){(void)e;steps++;}
void accept_event(lv_event_t *e){(void)e;steps++;}
static void inside(lv_obj_t *c,lv_obj_t *p){lv_area_t a,b;lv_obj_get_coords(c,&a);lv_obj_get_coords(p,&b);if(a.x1<b.x1||a.x2>b.x2||a.y1<b.y1||a.y2>b.y2){fprintf(stderr,"outside %d,%d..%d,%d vs %d,%d..%d,%d\n",a.x1,a.y1,a.x2,a.y2,b.x1,b.y1,b.x2,b.y2);abort();}}
static void buttons(lv_obj_t *row){for(uint32_t i=0;i<lv_obj_get_child_count(row);i++){lv_obj_t *b=lv_obj_get_child(row,i);if(lv_obj_has_flag(b,LV_OBJ_FLAG_HIDDEN))continue;if(lv_obj_check_type(b,&lv_button_class)){inside(lv_obj_get_child(b,0),b);assert(lv_obj_get_height(b)>=48);lv_area_t a,r;lv_obj_get_coords(b,&a);lv_obj_get_coords(row,&r);assert(a.x1>=r.x1&&a.x2<=r.x2);}else buttons(b);}}
static inline void layout(lv_obj_t *popup){assert(popup);lv_obj_update_layout(popup);inside(popup,lv_screen_active());for(unsigned i=0;i<3;i++)inside(lv_obj_get_child(popup,i),popup);lv_obj_t *body=ui_cal_dialog_body(popup),*footer=lv_obj_get_child(popup,2);buttons(body);buttons(footer);lv_area_t before,after;lv_obj_get_coords(footer,&before);lv_obj_scroll_to_y(body,lv_obj_get_scroll_y(body)+lv_obj_get_scroll_bottom(body),LV_ANIM_OFF);lv_obj_update_layout(popup);assert(lv_obj_get_scroll_bottom(body)<=1);lv_obj_get_coords(footer,&after);assert(!memcmp(&before,&after,sizeof(before)));lv_obj_scroll_to_y(body,0,LV_ANIM_OFF);capture();}
static inline void click(lv_obj_t *o){lv_obj_send_event(o,LV_EVENT_CLICKED,NULL);}
static inline lv_obj_t *action(lv_obj_t *p,unsigned i){return lv_obj_get_child(lv_obj_get_child(p,2),i);}
static inline lv_obj_t *choice(lv_obj_t *p,unsigned i){return lv_obj_get_child(lv_obj_get_user_data(ui_cal_dialog_body(p)),i);}
#if defined(TEST_FEEDBACK)
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
static bool running,update_accept;
static unsigned update_starts;
void ui_camera_set_setup_active(bool active){(void)active;}
void ui_dashboard_set_camera_quiesced(bool active){(void)active;}
bool camera_stream_busy(void){return false;}
void vTaskDelay(uint32_t ticks){(void)ticks;}
bool ota_manager_is_running(void){return running;}
bool ota_manager_start(const char *url){assert(url&&url[0]);update_starts++;return update_accept;}
void ota_manager_set_url(const char *url){(void)url;}
#include "ui_ota_popup.c"
#include "ota_ui_controller.c"
static void run(void){
 running=false;update_accept=false;update_starts=0;
 s_ota_popup=ui_popup_create(lv_layer_top(),300,200,UI_POPUP_STANDARD);
 s_ota_url_ta=lv_textarea_create(s_ota_popup);lv_textarea_set_text(s_ota_url_ta,"");
 start_cb(NULL);assert(s_ota_popup&&s_ota_url_ta&&!update_starts&&!s_deferred_start_popup);
 assert(!strcmp(lv_textarea_get_placeholder_text(s_ota_url_ta),"Firmware URL required."));
 ui_ota_popup_close();
 assert(!ota_ui_controller_start_url("https://fixture/fw.bin",false));assert(s_start_failure&&update_starts==1);
 lv_obj_update_layout(s_start_failure);inside(s_start_failure,lv_screen_active());
 lv_obj_t *body=lv_obj_get_child(s_start_failure,0),*button=lv_obj_get_child(s_start_failure,1);
 inside(button,s_start_failure);buttons(s_start_failure);
 lv_obj_scroll_to_y(body,lv_obj_get_scroll_y(body)+lv_obj_get_scroll_bottom(body),LV_ANIM_OFF);
 lv_obj_update_layout(s_start_failure);assert(lv_obj_get_scroll_bottom(body)<=1);
 click(button);assert(!s_start_failure);
 assert(!ota_ui_controller_start_url("",false)&&s_start_failure&&update_starts==1);start_failure_close(NULL);
 running=true;assert(!ota_ui_controller_start_url("https://fixture/fw.bin",false)&&!s_start_failure&&update_starts==1);
 running=false;update_accept=true;assert(ota_ui_controller_start_url("https://fixture/fw.bin",false)&&!s_start_failure&&update_starts==2);
 ui_ota_start_failure("Retry information");lv_obj_delete(s_start_failure);assert(!s_start_failure);
}
#elif defined(TEST_PID)
#include "ui_calibration_pid.c"
static void run(void){lv_obj_t *popup=NULL,*target_label=NULL;char objects[8][DEVICE_CATALOG_OBJECT_NAME_MAX],names[8][DEVICE_CATALOG_DISPLAY_NAME_MAX];size_t count=0,selected=0;int target=0,minimum=0,maximum=0;ui_calibration_pid_context_t context={.popup=&popup,.target_label=&target_label,.object_names=objects,.display_names=names,.heater_capacity=8,.heater_count=&count,.selected_index=&selected,.target=&target,.target_min=&minimum,.target_max=&maximum,.send_gcode=send_command,.show_results=fixture_results,.refresh_results=refresh_results};ui_calibration_pid_init(&context);
 ui_calibration_pid_event(NULL);layout(popup);assert(count==8);click(lv_obj_get_child(ui_cal_dialog_body(popup),7));layout(popup);assert(selected==7&&target==200);click(choice(popup,3));assert(target==210);click(action(popup,1));layout(popup);assert(!sends);click(action(popup,0));layout(popup);assert(target==210);click(action(popup,1));layout(popup);click(action(popup,1));assert(!popup&&sends==1&&strstr(command,"HEATER=extruder7 TARGET=210"));
 ui_calibration_pid_event(NULL);click(lv_obj_get_child(ui_cal_dialog_body(popup),0));click(action(popup,1));owner++;click(action(popup,1));assert(sends==1&&popup);click(action(popup,0));click(action(popup,1));click(action(popup,1));assert(sends==1);ui_calibration_pid_close();}
#elif defined(TEST_MANUAL)
#include "ui_calibration_manual_probe.c"
static void run(void){assert(ui_calibration_manual_probe_show("PROBE / Z OFFSET","Use TESTZ steps toward (-) or away (+) from the bed. Use a paper test, then ACCEPT only when the offset is correct.","ACCEPT",step_event,abort_event,accept_event));layout(s_probe.popup);for(unsigned i=0;i<10;i++)click(choice(s_probe.popup,i));assert(steps==10&&!strcmp(command,"TESTZ Z=1.0"));owner++;click(choice(s_probe.popup,0));assert(steps==10);ui_calibration_manual_probe_hide();assert(!s_probe.popup);}
#elif defined(TEST_MOTION)
#include "ui_calibration_motion.c"
void ui_motion_diagnostics_create(lv_obj_t *c){(void)c;}
void ui_motion_diagnostics_hide(void){}
static void run(void){lv_obj_t *card=lv_obj_create(lv_screen_active());lv_obj_set_size(card,390,190);ui_calibration_motion_create(card,send_command,ready,fixture_results,refresh_results);ui_calibration_motion_refresh(true,true,true);lv_obj_update_layout(card);for(unsigned i=0;i<3;i++){lv_obj_t *b=lv_obj_get_child(card,i);inside(lv_obj_get_child(b,0),b);assert(lv_obj_get_height(lv_obj_get_child(b,0))<=UI_FONT_BODY->line_height);}
 click(s_motion.input_shaper_button);layout(s_motion.input_shaper_popup);click(action(s_motion.input_shaper_popup,0));assert(!s_motion.input_shaper_popup&&!sends);click(s_motion.resonance_test_button);layout(s_motion.resonance_test_popup);click(choice(s_motion.resonance_test_popup,0));assert(sends==1&&!strcmp(command,"TEST_RESONANCES AXIS=X"));click(s_motion.accelerometer_check_button);layout(s_motion.accelerometer_check_popup);click(action(s_motion.accelerometer_check_popup,1));assert(sends==2&&!strcmp(command,"MEASURE_AXES_NOISE"));ui_calibration_motion_hide();lv_obj_delete(card);}
#elif defined(TEST_PROBE)
#include "ui_probe_accuracy.c"
void ui_calibration_results_show(const char *t,const char *s){(void)t;(void)s;}
void ui_calibration_results_refresh(void){}
static void run(void){lv_obj_t *card=lv_obj_create(lv_screen_active());ui_probe_accuracy_create(card,send_command,ready);ui_probe_accuracy_refresh(true);click(s_button);layout(s_popup);assert(!lv_obj_has_state(s_run,LV_STATE_DISABLED));state.homed_axes[0]=0;refresh_status(NULL);assert(lv_obj_has_state(s_run,LV_STATE_DISABLED));run_cb(NULL);assert(!sends);strcpy(state.homed_axes,"xyz");refresh_status(NULL);lv_dropdown_set_selected(s_samples,2);click(s_run);assert(sends==1&&!strcmp(command,"PROBE_ACCURACY SAMPLES=20")&&!s_popup&&!s_status_timer);ui_probe_accuracy_hide();lv_obj_delete(card);}
#elif defined(TEST_ENDSTOP)
#include "ui_endstop_status.c"
static void run(void){ui_endstop_status_show();layout(s_popup);assert(queries==1);endstops.waiting=false;endstops.valid=true;endstops.count=12;for(unsigned i=0;i<12;i++){snprintf(endstops.items[i].name,sizeof(endstops.items[i].name),"stepper_%u_long_endstop_name",i);endstops.items[i].triggered=i%2;}refresh(NULL);layout(s_popup);assert(strstr(lv_label_get_text(s_body),"TRIGGERED")&&strstr(lv_label_get_text(s_body),"OPEN"));strcpy(state.printer_state,"printing");refresh(NULL);assert(queries==1&&strstr(lv_label_get_text(s_body),"paused"));owner++;refresh(NULL);assert(strstr(lv_label_get_text(s_body),"changed"));click(action(s_popup,0));assert(!s_popup&&!s_timer);}
#elif defined(TEST_RESULTS)
#include "ui_calibration_results.c"
static void run(void){lv_obj_t *popup=NULL,*confirm=NULL,*label=NULL,*apply=NULL;calibration_session_snapshot_t snap;uint32_t gen=0;char display[1200];ui_calibration_results_context_t context={.results_popup=&popup,.save_confirm_popup=&confirm,.results_label=&label,.apply_restart_button=&apply,.session_snapshot=&snap,.session_generation=&gen,.display=display,.display_size=sizeof(display),.send_gcode=send_command};ui_calibration_results_init(&context);ui_calibration_results_show("CALIBRATION RESULTS","Waiting for Klipper...");layout(popup);assert(lv_obj_has_flag(apply,LV_OBJ_FLAG_HIDDEN));session.status=CALIBRATION_SESSION_RESULTS;session.save_available=session.completed=true;session.generation=2;strcpy(session.results,"Probe calibration complete.\nReview all measurements before saving.\n");for(int i=0;i<10;i++)strcat(session.results,"Repeated result line with printer-specific information.\n");ui_calibration_results_refresh();layout(popup);assert(!lv_obj_has_flag(apply,LV_OBJ_FLAG_HIDDEN));click(apply);layout(confirm);click(action(confirm,0));assert(!confirm&&popup&&!sends);click(apply);click(action(confirm,1));assert(sends==1&&!strcmp(command,"SAVE_CONFIG")&&popup&&confirm);
layout(confirm);assert(lv_obj_has_state(action(confirm,1),LV_STATE_DISABLED));ui_calibration_results_close();}
#elif defined(TEST_PA)
#include "ui_calibration_pressure_advance.c"
static void run(void){lv_obj_t *card=lv_obj_create(lv_screen_active());ui_calibration_pressure_advance_create(card,send_command,ready);ui_calibration_pressure_advance_refresh(true,true);click(s_pa.button);layout(s_pa.popup);click(choice(s_pa.popup,1));assert(sends==1&&strstr(command,"FACTOR=.020"));layout(s_pa.popup);click(action(s_pa.popup,0));assert(!s_pa.popup);ui_calibration_pressure_advance_hide();lv_obj_delete(card);}
#elif defined(TEST_CUSTOM)
#include "ui_calibration_custom.c"
static void run(void){lv_obj_t *popup=NULL;char names[16][MACRO_CONTROLLER_NAME_MAX];size_t count=0,selected=0;ui_calibration_custom_context_t context={.popup=&popup,.names=names,.names_capacity=16,.count=&count,.selected=&selected,.send=send_command,.ready=ready,.show_results=fixture_results,.refresh_results=refresh_results};ui_calibration_custom_init(&context);ui_calibration_custom_event(NULL);layout(popup);assert(count==16);click(lv_obj_get_child(ui_cal_dialog_body(popup),15));layout(popup);click(action(popup,1));assert(sends==1&&!popup&&strstr(command,"MACRO_15"));ui_calibration_custom_close();}
#elif defined(TEST_GEOMETRY)
#include "ui_calibration_geometry.c"
static void run(void){lv_obj_t *screws=NULL,*gantry=NULL,*label=NULL;bool home=false,ghome=false,qgl=false;char text[1200];uint32_t gen=0;calibration_session_snapshot_t snap;ui_calibration_geometry_context_t context={.screws_popup=&screws,.gantry_popup=&gantry,.screws_results_label=&label,.screws_home_required=&home,.gantry_home_required=&ghome,.gantry_use_qgl=&qgl,.screws_display=text,.screws_display_size=sizeof(text),.session_snapshot=&snap,.session_generation=&gen,.send_gcode=send_command};ui_calibration_geometry_init(&context);ui_calibration_geometry_screws_event(NULL);layout(screws);click(action(screws,1));assert(sends==1&&strstr(command,"SCREWS_TILT_CALCULATE"));layout(screws);ui_calibration_geometry_close();ui_calibration_geometry_gantry_event(NULL);layout(gantry);click(action(gantry,0));assert(!gantry&&sends==1);ui_calibration_geometry_close();}
#endif
static void flush(lv_display_t *d,const lv_area_t *a,uint8_t *p){uint16_t *v=(uint16_t *)p;for(int y=a->y1;y<=a->y2;y++)for(int x=a->x1;x<=a->x2;x++)raster[y*1024+x]=*v++;lv_display_flush_ready(d);}
static void capture(void){const char *folder=getenv("CALIBRATION_SCREENSHOTS");if(!folder||lv_display_get_horizontal_resolution(NULL)!=1024||ui_theme_get_density()!=1||!ui_theme_get_accessibility().large_text)return;lv_refr_now(NULL);char path[512];snprintf(path,sizeof(path),"%s/%s-theme%u-frame%u.ppm",folder,TEST_NAME,(unsigned)ui_theme_get_active(),render_frame++);FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n1024 600\n255\n");for(unsigned i=0;i<1024*600;i++){uint16_t v=raster[i];unsigned char c[3]={(v>>11)*255/31,((v>>5)&63)*255/63,(v&31)*255/31};fwrite(c,1,3,f);}fclose(f);}
int main(void){lv_init();lv_display_t *d=lv_display_create(1024,600);static uint8_t pixels[1024*50*2];lv_display_set_buffers(d,pixels,NULL,sizeof(pixels),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);lv_display_set_flush_cb(d,flush);
 for(unsigned t=0;t<5;t++)for(unsigned density=0;density<3;density++)for(unsigned large=0;large<2;large++)for(unsigned narrow=0;narrow<3;narrow++){lv_display_set_resolution(d,(int[]){1024,640,480}[narrow],narrow==2?400:600);ui_theme_set_active(t);ui_theme_set_density(density);ui_theme_set_accessibility((ui_accessibility_t){.large_text=large});memset(&state,0,sizeof(state));state.moonraker_ok=state.live_data_ok=true;strcpy(state.printer_state,"ready");strcpy(state.homed_axes,"xyz");strcpy(state.active_hotend,"extruder");owner++;render_frame=0;sends=steps=queries=0;send_accept=true;memset(&session,0,sizeof(session));run();assert(!lv_obj_get_child_count(lv_layer_top()));}
 lv_display_delete(d);lv_deinit();puts("PASS: real calibration owner, five themes/densities/text sizes/viewports, pinned actions, scroll reach and command flow");return 0;}
