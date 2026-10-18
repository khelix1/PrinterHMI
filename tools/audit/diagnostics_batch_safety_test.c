#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "custom_theme.h"
#include "moonraker.h"
#include "calibration_session_controller.h"
#include "motion_diagnostics_controller.h"
#include "endstop_status_controller.h"
bool custom_theme_color_override(uint32_t a,uint32_t b,uint32_t c,uint32_t *d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_metric_override(int32_t a,int32_t b,int32_t c,int32_t *d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_accent_override(uint8_t a,uint32_t *b){(void)a;(void)b;return false;}
bool custom_theme_surface_opacity(uint8_t *a){(void)a;return false;}
static moonraker_state_t live;
static uint32_t generation=1;
static unsigned mode, snapshot_mode;
#if defined(TEST_PROBE)
static unsigned sends,errors,results,ready_mode;
static char command[64];
static calibration_session_snapshot_t session;
#elif defined(TEST_ENDSTOP)
static unsigned queries,errors;
static endstop_status_snapshot_t endstops;
#endif
static bool accept=true,ws=true;
static int64_t now;

static motion_diagnostics_snapshot_t motion_sample;
uint32_t moonraker_config_generation(void){return generation;}
void moonraker_state_snapshot(moonraker_state_t*out){*out=live;}
void ui_toast_show(ui_status_kind_t k,const char*t,const char*d){(void)k;(void)t;(void)d;}
#if defined(TEST_PROBE)
uint32_t console_controller_latest_sequence(void){return 7;}
void console_controller_add_command(const char*c){(void)c;}
void calibration_session_controller_begin(calibration_session_kind_t kind,uint32_t seq){session.kind=kind;session.start_sequence=seq;session.status=CALIBRATION_SESSION_WAITING;session.generation++;}
void calibration_session_controller_snapshot(calibration_session_snapshot_t*out){*out=session;}
void calibration_session_controller_mark_error(const char*m){(void)m;errors++;session.status=CALIBRATION_SESSION_ERROR;}
void ui_calibration_results_show(const char*t,const char*d){(void)t;(void)d;results++;}
void ui_calibration_results_refresh(void){}
#endif
static inline void click(lv_obj_t*o){assert(o);lv_obj_send_event(o,LV_EVENT_CLICKED,NULL);}
static unsigned timers(void){unsigned n=0;for(lv_timer_t*t=lv_timer_get_next(NULL);t;t=lv_timer_get_next(t))++n;return n;}
static void setup(void){generation++;accept=ws=true;mode=snapshot_mode=0;
#if defined(TEST_PROBE)
 ready_mode=0;
#endif
now+=3000000;live=(moonraker_state_t){.moonraker_ok=true,.live_data_ok=true};strcpy(live.printer_state,"ready");strcpy(live.homed_axes,"xyz");motion_sample=(motion_diagnostics_snapshot_t){.owner_generation=generation,.discovered=true};for(unsigned i=0;i<4;i++){motion_sample.limit_valid[i]=true;motion_sample.limits[i]=100;}}
static void flush(lv_display_t*d,const lv_area_t*a,uint8_t*p){(void)a;(void)p;lv_display_flush_ready(d);}
#if defined(TEST_PROBE)
#include "ui_probe_accuracy.c"
static lv_obj_t*card;
static bool ready(const char*w){(void)w;unsigned m=ready_mode;ready_mode=0;if(m==1)run_cb(NULL);if(m==2)ui_probe_accuracy_hide();if(m==3)generation++;return true;}
static bool send(const char*c){sends++;snprintf(command,sizeof(command),"%s",c);unsigned m=mode;mode=0;if(m==1)run_cb(NULL);if(m==2){close_popup();open_cb(NULL);}if(m==3)lv_obj_delete(s_popup);if(m==4)generation++;if(m==5){session.kind=CALIBRATION_SESSION_PID;session.generation++;}return accept;}
static void reset(void){ui_probe_accuracy_hide();if(card)lv_obj_delete(card);setup();s_accepted=false;card=lv_obj_create(lv_screen_active());ui_probe_accuracy_create(card,send,ready);ui_probe_accuracy_refresh(true);open_cb(NULL);assert(s_popup);}
static void checks(void){
 reset();unsigned baseline=timers()-1,before=sends,r=results;lv_dropdown_set_selected(s_samples,2);accept=false;click(s_run);assert(sends==before+1&&s_popup&&lv_dropdown_get_selected(s_samples)==2&&strstr(lv_label_get_text(s_status),"not accepted"));accept=true;click(s_run);assert(sends==before+2&&results==r+1&&!strcmp(command,"PROBE_ACCURACY SAMPLES=20")&&!s_popup&&timers()==baseline);
 open_cb(NULL);assert(lv_obj_has_state(s_run,LV_STATE_DISABLED));run_cb(NULL);assert(sends==before+2);lv_tick_inc(5000);refresh_status(NULL);assert(!lv_obj_has_state(s_run,LV_STATE_DISABLED));
 for(unsigned m=1;m<=5;m++){reset();before=sends;r=results;unsigned e=errors;mode=m;accept=m!=5;click(s_run);assert(sends==before+1&&!s_dispatching);if(m==1)assert(results==r+1);else assert(results==r);if(m==5)assert(errors==e&&session.kind==CALIBRATION_SESSION_PID);if(m==2||m==3){open_cb(NULL);assert(s_popup&&lv_obj_has_state(s_run,LV_STATE_DISABLED));run_cb(NULL);assert(sends==before+1);}}
 for(unsigned m=1;m<=3;m++){reset();before=sends;ready_mode=m;click(s_run);assert(sends==before+(m==1?1:0)&&!s_dispatching);}
 reset();before=sends;live.live_data_ok=false;run_cb(NULL);assert(sends==before);live.live_data_ok=true;strcpy(live.printer_state,"startup");refresh_status(NULL);assert(lv_obj_has_state(s_run,LV_STATE_DISABLED));run_cb(NULL);assert(sends==before);strcpy(live.printer_state,"ready");live.homed_axes[0]=0;run_cb(NULL);assert(sends==before);
 reset();lv_obj_delete(s_samples);assert(!s_samples);refresh_status(NULL);assert(lv_obj_has_state(s_run,LV_STATE_DISABLED));run_cb(NULL);close_popup();assert(timers()==baseline);
 reset();lv_obj_delete(card);card=NULL;assert(!s_popup&&!s_button&&!s_popup_send&&!s_status_timer);refresh_status(NULL);run_cb(NULL);
 reset();lv_obj_t*old=card;card=lv_obj_create(lv_screen_active());ui_probe_accuracy_create(card,send,ready);ui_probe_accuracy_refresh(true);open_cb(NULL);lv_obj_t*popup=s_popup;lv_obj_delete(old);assert(s_popup==popup&&s_button);ui_probe_accuracy_hide();lv_obj_delete(card);card=NULL;
}
#elif defined(TEST_ENDSTOP)
#include "ui_endstop_status.c"
int64_t esp_timer_get_time(void){return now;}
bool moonraker_live_websocket_connected(void){return ws;}
void endstop_status_controller_reset(void){memset(&endstops,0,sizeof(endstops));}
uint32_t endstop_status_controller_begin(uint32_t owner){endstops.owner_generation=owner;endstops.waiting=true;endstops.updated_us=now;return ++endstops.request_id;}
void endstop_status_controller_failed(const char*m){errors++;endstops.waiting=false;snprintf(endstops.error,sizeof(endstops.error),"%s",m);endstops.updated_us=now;}
void endstop_status_controller_snapshot(endstop_status_snapshot_t*out){*out=endstops;unsigned m=snapshot_mode;snapshot_mode=0;if(m==1)ui_endstop_status_close();}
bool moonraker_live_websocket_request_endstops(uint32_t id){(void)id;queries++;unsigned m=mode;mode=0;if(m==1)refresh(NULL);if(m==2)ui_endstop_status_close();if(m==3){ui_endstop_status_close();ui_endstop_status_show();}if(m==4){generation++;ui_endstop_status_close();ui_endstop_status_show();}if(m==5)generation++;return accept;}
static void reset(void){ui_endstop_status_close();setup();}
static void checks(void){
 reset();unsigned baseline=timers(),before=queries;accept=false;ui_endstop_status_show();assert(queries==before+1&&strstr(lv_label_get_text(s_body),"Retrying"));refresh(NULL);assert(queries==before+1);now+=2000000;accept=true;refresh(NULL);assert(queries==before+2&&endstops.waiting);
 for(unsigned m=1;m<=5;m++){reset();before=queries;unsigned e=errors;mode=m;accept=false;ui_endstop_status_show();assert(queries==before+1&&!s_query_dispatching);if(m==2)assert(!s_popup&&!s_body&&!s_timer);if(m==3||m==4){assert(s_popup&&!endstops.error[0]&&errors==e);accept=true;refresh(NULL);assert(queries==before+2);}if(m==5){refresh(NULL);assert(strstr(lv_label_get_text(s_body),"changed"));}}
 reset();ui_endstop_status_show();endstops.waiting=false;now+=2000000;before=queries;snapshot_mode=1;refresh(NULL);assert(!s_popup&&queries==before);
 reset();before=queries;strcpy(live.printer_state,"startup");ui_endstop_status_show();assert(queries==before&&strstr(lv_label_get_text(s_body),"unavailable"));strcpy(live.printer_state,"ready");ws=false;refresh(NULL);assert(queries==before);ws=true;strcpy(live.printer_state,"printing");refresh(NULL);assert(queries==before&&strstr(lv_label_get_text(s_body),"paused"));
 reset();ui_endstop_status_show();lv_obj_delete(s_body);assert(!s_body&&!s_timer);before=queries;refresh(NULL);assert(queries==before);ui_endstop_status_close();assert(timers()==baseline);
 reset();ui_endstop_status_show();generation++;before=queries;ui_endstop_status_show();assert(s_owner==generation&&queries==before+1);ui_endstop_status_close();
}
#elif defined(TEST_MOTION)
#include "ui_motion_diagnostics.c"
#include "motion_calculation.inc"
static lv_obj_t*card;
void motion_diagnostics_controller_snapshot(motion_diagnostics_snapshot_t*out){*out=motion_sample;unsigned m=snapshot_mode;snapshot_mode=0;if(m==1)close_popup();if(m==2){close_popup();show_view(VIEW_DISTANCE);}if(m==3)generation++;}
static void reset(void){close_popup();if(card)lv_obj_delete(card);setup();card=lv_obj_create(lv_screen_active());ui_motion_diagnostics_create(card);}
static void checks(void){
 reset();unsigned baseline=timers();click(s_actions[1]);assert(s->popup&&s->pitch&&s->timer);click(s->pitch);assert(s->editor);lv_textarea_set_text(s->editor_value,"2.5");editor_done_cb(NULL);assert(!s->editor&&!strcmp(lv_textarea_get_text(s->pitch),"2.5"));calculate_cb(NULL);assert(strstr(lv_label_get_text(s->result),"50.000000"));
 click(s->pitch);lv_textarea_set_text(s->editor_value,"bad");editor_done_cb(NULL);assert(s->editor&&lv_label_get_text(s->editor_feedback)[0]);lv_obj_delete(s->pitch);assert(!s->pitch&&!s->editor&&!s->editor_target);calculate_cb(NULL);close_popup();assert(timers()==baseline);
 reset();click(s_actions[1]);click(s->count);lv_obj_delete(s->editor_value);assert(!s->editor_value);editor_done_cb(NULL);close_popup();assert(timers()==baseline);
 for(unsigned view=0;view<3;view++){reset();click(s_actions[view]);assert(s->popup);if(view==0){lv_obj_delete(s->limits[0]);assert(!s->limits[0]);}if(view==2){lv_obj_delete(s->driver_selector);assert(!s->driver_selector);}refresh(NULL);close_popup();assert(timers()==baseline);}
 for(unsigned m=1;m<=3;m++){reset();snapshot_mode=m;click(s_actions[0]);if(m==1)assert(!s->popup&&!s->timer);if(m==2)assert(s->popup&&s->view==VIEW_DISTANCE);if(m==3){refresh(NULL);assert(!s->timer&&strstr(lv_label_get_text(s->status),"Printer changed"));}}
 reset();click(s_actions[1]);lv_obj_delete(s->pitch_label);assert(!s->pitch_label);click(s->pitch);assert(s->editor);close_popup();
 reset();generation++;click(s_actions[1]);assert(!s->popup);
 reset();click(s_actions[1]);click(s->pitch);lv_obj_delete(card);card=NULL;assert(!s_card&&!s->popup&&!s->editor&&!s->timer);refresh(NULL);calculate_cb(NULL);
 reset();lv_obj_t*old=card;card=lv_obj_create(lv_screen_active());ui_motion_diagnostics_create(card);click(s_actions[1]);lv_obj_t*popup=s->popup;lv_obj_delete(old);assert(s_card==card&&s->popup==popup);close_popup();lv_obj_delete(card);card=NULL;
}
#endif
int main(void){lv_init();lv_display_t*d=lv_display_create(1024,600);static uint8_t pixels[1024*40*2];lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);lv_display_set_buffers(d,pixels,NULL,sizeof(pixels),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(d,flush);
 for(unsigned theme=0;theme<5;theme++)for(unsigned density=0;density<3;density++)for(unsigned large=0;large<2;large++){ui_theme_set_active(theme);ui_theme_set_density(density);ui_theme_set_accessibility((ui_accessibility_t){.large_text=large});checks();}
 puts("PASS: diagnostics batch dispatch/retry, captured owner, reentrant callbacks, replacement sessions/pages, retired fields/editors/timers and all five themes");return 0;}
