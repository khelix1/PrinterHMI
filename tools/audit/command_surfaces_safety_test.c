#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "custom_theme.h"
#include "moonraker.h"
#include "ui_toast.h"
#include "console_controller.h"
#include "macro_parameter_utils.h"
#include "ui_page_layout_profile.h"
const ui_page_layout_profile_t *custom_theme_page_profile(void){return NULL;}
bool custom_theme_color_override(uint32_t a,uint32_t b,uint32_t c,uint32_t *d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_metric_override(int32_t a,int32_t b,int32_t c,int32_t *d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_accent_override(uint8_t a,uint32_t *b){(void)a;(void)b;return false;}
bool custom_theme_surface_opacity(uint8_t *a){(void)a;return false;}
static moonraker_state_t live;
static uint32_t generation=1;
static unsigned sends,notes,mode,log_mode;
static bool accepted=true;
static char sent[MACRO_COMMAND_MAX],notice[192];
uint32_t moonraker_config_generation(void){return generation;}
void moonraker_state_snapshot(moonraker_state_t*out){*out=live;}
void ui_toast_show(ui_status_kind_t k,const char*t,const char*d){(void)k;assert(t&&d);notes++;snprintf(notice,sizeof(notice),"%s",d);}
static void click(lv_obj_t*o){assert(o);lv_obj_send_event(o,LV_EVENT_CLICKED,NULL);}
static void setup(void){generation++;accepted=true;mode=log_mode=0;live=(moonraker_state_t){.moonraker_ok=true,.live_data_ok=true};strcpy(live.printer_state,"ready");}
static void flush(lv_display_t*d,const lv_area_t*a,uint8_t*p){(void)a;(void)p;lv_display_flush_ready(d);}
#if defined(TEST_DRYBOX)
#include "macro_parameter_utils.h"
#include "ui_drybox_page.c"
#include "ui_drybox.c"
void ui_shell_raise_topbar(void){}
void ui_shell_raise_nav(void){}
static void status(lv_event_t*event){(void)event;}
static bool send(const char*command){
 sends++;snprintf(sent,sizeof(sent),"%s",command);unsigned m=mode;mode=0;
 if(m==1)drybox_page_action(command,NULL);
 if(m==2)ui_drybox_hide();
 if(m==3){ui_drybox_hide();ui_drybox_show();drybox_page_action(command,NULL);}
 if(m==4){generation++;ui_drybox_hide();ui_drybox_show();drybox_page_action(command,NULL);}
 if(m==5)lv_obj_delete(s_page.panel);
 return accepted;
}
static void reset(void){ui_drybox_hide();setup();s_sent_pending=false;live.drybox_active_program=live.drybox_selected_program=UI_DRYBOX_PROGRAM_PLA;live.heater_on=true;ui_drybox_set_callbacks(send,status);ui_drybox_show();assert(s_page.panel);}
static void checks(void){
 reset();unsigned before=sends,nt=notes;accepted=false;click(s_program_buttons[DRYBOX_PROGRAM_PETG]);assert(sends==before+1&&s_active_program==UI_DRYBOX_PROGRAM_PLA&&s_selected_program==UI_DRYBOX_PROGRAM_PLA&&notes==nt+1&&strstr(notice,"Previous program kept"));accepted=true;click(s_program_buttons[DRYBOX_PROGRAM_PETG]);assert(sends==before+2&&s_selected_program==UI_DRYBOX_PROGRAM_PETG&&s_active_program==UI_DRYBOX_PROGRAM_PLA);click(s_program_buttons[DRYBOX_PROGRAM_PETG]);assert(sends==before+2);
 ui_drybox_hide();ui_drybox_show();click(s_program_buttons[DRYBOX_PROGRAM_PETG]);assert(sends==before+2);click(s_program_buttons[DRYBOX_PROGRAM_STOP]);assert(sends==before+3&&!strcmp(sent,"DRY_STOP")&&s_active_program==UI_DRYBOX_PROGRAM_PLA);live.drybox_active_program=UI_DRYBOX_PROGRAM_NONE;live.heater_on=false;ui_drybox_refresh();assert(s_active_program==UI_DRYBOX_PROGRAM_NONE);
 for(unsigned m=1;m<=5;m++){reset();before=sends;nt=notes;mode=m;click(s_program_buttons[DRYBOX_PROGRAM_STOP]);assert(sends==before+1&&!s_drybox_dispatching);if(m==1)assert(notes==nt+1);else assert(notes==nt);if(m==2||m==5)assert(!s_page.panel&&!s_page_owner&&!s_program_buttons[1]);if(m==3){click(s_program_buttons[DRYBOX_PROGRAM_STOP]);assert(sends==before+1);}if(m==4){click(s_program_buttons[DRYBOX_PROGRAM_STOP]);assert(sends==before+2);}}
 reset();before=sends;live.live_data_ok=false;drybox_page_action("DRY_STOP",NULL);assert(sends==before);live.live_data_ok=true;generation++;drybox_page_action("DRY_STOP",NULL);assert(sends==before);ui_drybox_show();assert(s_drybox_owner==generation);click(s_program_buttons[DRYBOX_PROGRAM_STOP]);assert(sends==before+1);
 reset();lv_obj_delete(s_page.air_label);assert(!s_page.air_label);ui_drybox_refresh();lv_obj_delete(s_program_buttons[1]);assert(!s_program_buttons[1]);ui_drybox_refresh();lv_obj_delete(s_page.panel);assert(!s_page.panel&&!s_heater_dot&&!s_action_cb);ui_drybox_refresh();ui_drybox_show();assert(s_page.panel);
 ui_drybox_hide();ui_drybox_page_t old={0},next={0};assert(ui_drybox_page_create(&old,drybox_page_action,drybox_banner_text));assert(ui_drybox_page_create(&next,drybox_page_action,drybox_banner_text));lv_obj_t*button=s_program_buttons[1];lv_obj_delete(old.panel);assert(!old.panel);ui_drybox_page_cleanup(&old);assert(s_page_owner==&next&&s_program_buttons[1]==button);ui_drybox_page_cleanup(&next);assert(!s_page_owner);
}
#elif defined(TEST_MACROS)
#include "macro_controller.h"
static uint32_t catalog_generation=1;
void macro_controller_status(macro_controller_status_t*out){memset(out,0,sizeof(*out));out->discovered=true;out->count=out->total_count=3;out->generation=catalog_generation;}
bool macro_controller_get(size_t i,char*out,size_t n){if(i>=3)return false;snprintf(out,n,"TEST_%u",(unsigned)i);return true;}
bool macro_controller_is_favorite(const char*n){(void)n;return false;}
bool macro_controller_toggle_favorite(const char*n){(void)n;catalog_generation++;return true;}
bool macro_controller_parameters(const char*n,macro_parameter_catalog_t*out){(void)n;memset(out,0,sizeof(*out));out->count=1;strcpy(out->names[0],"VALUE");return true;}
#include "ui_macros.c"
static bool send(const char*command){sends++;snprintf(sent,sizeof(sent),"%s",command);unsigned m=mode;mode=0;if(m==1)run_macro_cb(NULL);if(m==2)close_confirm();if(m==3){ui_macros_hide();ui_macros_show(send);}if(m==4)generation++;return accepted;}
void console_controller_add_command(const char*c){(void)c;unsigned m=log_mode;if(m!=5)log_mode=0;if(m==1)ui_macros_hide();if(m==2)generation++;if(m==3)s_command_callback=NULL;if(m==4)run_macro_cb(NULL);}
void console_controller_add(console_entry_type_t t,const char*f,...){(void)t;(void)f;if(log_mode==5){log_mode=0;ui_macros_hide();}}
static void reset(void){ui_macros_hide();setup();ui_macros_show(send);s_macros->accepted=false;click(s_macros->rows[0]);assert(s_confirm);lv_textarea_set_text(s_macros->parameter_fields[0],"42");review_macro_cb(NULL);assert(s_macros->reviewed);}
static void review(void){click(s_macros->rows[0]);lv_textarea_set_text(s_macros->parameter_fields[0],"42");review_macro_cb(NULL);}
static void checks(void){
 reset();unsigned before=sends;accepted=false;run_macro_cb(NULL);assert(sends==before+1&&s_confirm&&s_macros->reviewed&&!strcmp(s_macros->pending_command,"TEST_0 VALUE=42"));accepted=true;run_macro_cb(NULL);assert(sends==before+2&&!s_confirm&&!strcmp(sent,"TEST_0 VALUE=42"));review();run_macro_cb(NULL);assert(sends==before+2&&s_confirm);ui_macros_hide();ui_macros_show(send);review();run_macro_cb(NULL);assert(sends==before+2);lv_tick_inc(5000);run_macro_cb(NULL);assert(sends==before+3&&!s_confirm);
 for(unsigned m=1;m<=4;m++){reset();before=sends;mode=m;run_macro_cb(NULL);assert(sends==before+1&&!s_macro_dispatching);if(m==2||m==3){ui_macros_show(send);review();run_macro_cb(NULL);assert(sends==before+1);}}
 for(unsigned m=1;m<=5;m++){reset();before=sends;log_mode=m;accepted=m!=5;run_macro_cb(NULL);assert(sends==before+((m==1||m==2)?0:1)&&!s_macro_dispatching);if(m==3)assert(!s_confirm);if(m==5)assert(!s_root);}
 reset();close_confirm();click(s_macros->rows[0]);click(s_macros->parameter_fields[0]);assert(s_macros->editor);lv_obj_delete(s_macros->parameter_fields[0]);assert(!s_macros->parameter_fields[0]&&!s_macros->editor&&!s_macros->editor_target);review_macro_cb(NULL);assert(s_confirm&&!s_macros->reviewed);
 reset();close_confirm();click(s_macros->rows[0]);click(s_macros->parameter_fields[0]);lv_obj_delete(s_macros->editor_value);assert(!s_macros->editor_value);editor_done_cb(NULL);close_confirm();
 reset();before=sends;generation++;run_macro_cb(NULL);assert(sends==before);refresh_timer_cb(NULL);assert(lv_obj_has_state(s_macros->rows[0],LV_STATE_DISABLED));ui_macros_show(send);assert(s_macros->root_owner==generation&&!s_confirm);click(s_macros->rows[0]);assert(s_confirm);
 reset();before=sends;live.moonraker_ok=false;run_macro_cb(NULL);assert(sends==before);ui_macros_hide();assert(!s_root&&!s_refresh_timer&&!s_command_callback&&!s_macros->editor);
}
#endif
int main(void){lv_init();lv_display_t*d=lv_display_create(1024,600);static uint8_t pixels[1024*40*2];lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);lv_display_set_buffers(d,pixels,NULL,sizeof(pixels),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(d,flush);
 for(unsigned theme=0;theme<5;theme++)for(unsigned density=0;density<3;density++)for(unsigned large=0;large<2;large++){ui_theme_set_active(theme);ui_theme_set_density(density);ui_theme_set_accessibility((ui_accessibility_t){.large_text=large});checks();}
 puts("PASS: command surfaces accepted/rejected dispatch, retry, logging callback fences, duplicate guard across reopening, confirmed Drybox state, retired fields/pages and all five themes");return 0;}
