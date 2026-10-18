#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "custom_theme.h"
#include "ui_toast.c"
static unsigned log_count,changes,owner=1,opens,nav_calls,commits;
static void(*storage_hook)(void),(*logging_hook)(void),(*navigation_hook)(void);
static void run_hook(void(**slot)(void)){if(*slot){void(*hook)(void)=*slot;*slot=NULL;hook();}}
static ui_shell_page_t opened;
uint32_t moonraker_config_generation(void){return owner;}
void ui_shell_page_action(ui_shell_page_t page){opened=page;opens++;run_hook(&navigation_hook);}
void ui_shell_set_active_nav(int page){nav_calls++;assert(page==UI_SHELL_PAGE_CONSOLE||page==UI_SHELL_PAGE_PRINTER);}
static uint8_t stored,staged;
static bool has_saved,fail_write,legacy_saved;
static uint8_t legacy_value;
static char printer[80]="Printer A";
static moonraker_profile_t profiles[MOONRAKER_CONFIG_MAX_PROFILES];
static bool health_known[MOONRAKER_CONFIG_MAX_PROFILES],health_online[MOONRAKER_CONFIG_MAX_PROFILES],health_fresh[MOONRAKER_CONFIG_MAX_PROFILES];
static int active_profile;
static unsigned chooser_opens;
static void open_chooser(void){chooser_opens++;}
int moonraker_config_active_profile_index(void){return active_profile;}
const moonraker_profile_t *moonraker_config_profile(int i){return i>=0&&i<MOONRAKER_CONFIG_MAX_PROFILES?&profiles[i]:NULL;}
bool printer_profile_health_get(int i,bool*k){if(k)*k=health_known[i];return health_online[i];}
bool printer_profile_health_live_state_fresh(int i,int64_t age){assert(age==5000000LL);return health_fresh[i];}

bool custom_theme_color_override(uint32_t a,uint32_t b,uint32_t c,uint32_t *d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_metric_override(int32_t a,int32_t b,int32_t c,int32_t *d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_accent_override(uint8_t a,uint32_t *b){(void)a;(void)b;return false;}
bool custom_theme_surface_opacity(uint8_t *a){(void)a;return false;}
const char *moonraker_config_active_profile_name(void){return printer;}
esp_err_t nvs_open(const char *name,int mode,nvs_handle_t *h){assert(!strcmp(name,"notifications"));*h=mode;return ESP_OK;}
esp_err_t nvs_get_u8(nvs_handle_t h,const char *key,uint8_t *v){assert(h==NVS_READONLY);if(!strcmp(key,"level")){*v=stored;return has_saved?ESP_OK:ESP_ERR_NVS_NOT_FOUND;}assert(!strcmp(key,"confirm"));*v=legacy_value;return legacy_saved?ESP_OK:ESP_ERR_NVS_NOT_FOUND;}
esp_err_t nvs_set_u8(nvs_handle_t h,const char *key,uint8_t value){assert(h==NVS_READWRITE&&!strcmp(key,"level"));staged=value;return ESP_OK;}
esp_err_t nvs_commit(nvs_handle_t h){assert(h==NVS_READWRITE);commits++;run_hook(&storage_hook);if(fail_write)return ESP_FAIL;stored=staged;has_saved=true;return ESP_OK;}
void nvs_close(nvs_handle_t h){(void)h;}
void operator_event_log_add(operator_event_level_t level,const char *format,...){(void)level;(void)format;log_count++;run_hook(&logging_hook);}
static void changed(void){changes++;}
static void inside(lv_obj_t *c,lv_obj_t *p){lv_area_t a,b;lv_obj_get_coords(c,&a);lv_obj_get_coords(p,&b);assert(a.x1>=b.x1&&a.x2<=b.x2&&a.y1>=b.y1&&a.y2<=b.y2);}
static void bounds(lv_obj_t *root){lv_obj_update_layout(root);inside(root,lv_screen_active());for(unsigned i=0;i<lv_obj_get_child_count(root);i++)inside(lv_obj_get_child(root,i),root);lv_obj_t *body=lv_obj_get_child(root,lv_obj_get_child_count(root)==2?0:1),*footer=lv_obj_get_child(root,lv_obj_get_child_count(root)-1);lv_area_t a,b;lv_obj_get_coords(footer,&a);lv_obj_scroll_to_y(body,lv_obj_get_scroll_bottom(body),LV_ANIM_OFF);lv_obj_update_layout(root);assert(lv_obj_get_scroll_bottom(body)<=1);lv_obj_get_coords(footer,&b);assert(!memcmp(&a,&b,sizeof(a)));for(unsigned i=0;i<lv_obj_get_child_count(footer);i++){lv_obj_t *button=lv_obj_get_child(footer,i);if(lv_obj_check_type(button,&lv_button_class)){inside(button,footer);inside(lv_obj_get_child(button,0),button);assert(lv_obj_get_height(button)==48);lv_obj_add_state(button,LV_STATE_PRESSED);assert(!lv_obj_get_style_transform_width(button,0)&&!lv_obj_get_style_outline_width(button,0));lv_obj_remove_state(button,LV_STATE_PRESSED);}}}
static uint16_t raster[1024*600];
static void flush(lv_display_t *d,const lv_area_t *a,uint8_t *p){uint16_t *v=(uint16_t *)p;for(int y=a->y1;y<=a->y2;y++)for(int x=a->x1;x<=a->x2;x++)raster[y*1024+x]=*v++;lv_display_flush_ready(d);}
static void capture(const char *name){const char *folder=getenv("NOTIFICATION_SCREENSHOTS");if(!folder||lv_display_get_horizontal_resolution(NULL)!=1024||ui_theme_get_density()!=1||!ui_theme_get_accessibility().large_text)return;lv_refr_now(NULL);char path[512];snprintf(path,sizeof(path),"%s/%s-theme%u.ppm",folder,name,(unsigned)ui_theme_get_active());FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n1024 600\n255\n");for(unsigned i=0;i<1024*600;i++){uint16_t v=raster[i];unsigned char c[3]={(v>>11)*255/31,((v>>5)&63)*255/63,(v&31)*255/31};fwrite(c,1,3,f);}fclose(f);}

static void observe_wait(moonraker_state_t *state,unsigned ms){ui_toast_observe_printer(state);lv_tick_inc(ms);ui_toast_observe_printer(state);}
static void attention_checks(void) {
 for(unsigned level=0;level<3;level++) {
  ui_toast_close();assert(ui_toast_set_level((ui_notification_level_t)level));
  moonraker_state_t state={0};strcpy(state.printer_state,"--");owner++;
  observe_wait(&state,10000);assert(!s_notice_count);
  state.live_data_ok=state.moonraker_ok=true;strcpy(state.printer_state,"ready");observe_wait(&state,3000);assert(!s_notice_count);
  unsigned logged=log_count;strcpy(state.printer_state,"printing");ui_toast_observe_printer(&state);
  assert(log_count==logged+1 && s_notice_count==(level?1u:0u));ui_toast_close();
  state.live_data_ok=false;observe_wait(&state,2999);assert(!s_notice_count);
  state.live_data_ok=true;ui_toast_observe_printer(&state);assert(!s_notice_count);
  for(unsigned cycle=0;cycle<3;cycle++) {
   state.live_data_ok=false;strcpy(state.printer_state,"--");observe_wait(&state,3000);
   assert(s_notice_count==1 && s_notices[0].kind==UI_STATUS_WARNING && !strcmp(s_notices[0].title,"PRINTER OFFLINE") && !s_timer);
   bounds(s_toast);ui_toast_observe_printer(&state);assert(s_notice_count==1);
   state.live_data_ok=true;strcpy(state.printer_state,"printing");observe_wait(&state,3000);
   assert(s_notice_count==1 && s_notices[0].essential && !strcmp(s_notices[0].title,"PRINTER RECONNECTED") && !s_timer);
   lv_obj_t *body=lv_obj_get_child(s_toast,0);assert(!strcmp(lv_label_get_text(lv_obj_get_child(body,1)),"PRINTER RECONNECTED"));bounds(s_toast);
  }
  ui_toast_close();strcpy(state.printer_state,"paused");ui_toast_observe_printer(&state);
  assert(s_notice_count==1 && s_notices[0].kind==UI_STATUS_WARNING && !strcmp(s_notices[0].title,"PRINT PAUSED"));ui_toast_close();
  ui_toast_observe_printer(&state);assert(!s_notice_count);
  state.live_data_ok=false;state.moonraker_ok=true;strcpy(state.printer_state,"error");ui_toast_observe_printer(&state);
  assert(s_notice_count==1 && s_notices[0].kind==UI_STATUS_DANGER && !strcmp(s_notices[0].title,"PRINTER ERROR"));ui_toast_close();ui_toast_observe_printer(&state);assert(!s_notice_count);
  state.live_data_ok=true;strcpy(state.printer_state,"ready");ui_toast_observe_printer(&state);
  strcpy(state.printer_state,"shutdown");ui_toast_observe_printer(&state);assert(s_notice_count==1 && !strcmp(s_notices[0].title,"PRINTER SHUTDOWN"));ui_toast_close();
  /* Choosing a fresh owner must retain bootstrap fencing at every level. */
  owner++;state.live_data_ok=false;state.moonraker_ok=false;strcpy(state.printer_state,"--");observe_wait(&state,10000);assert(!s_notice_count);
 }
 assert(ui_toast_set_level(UI_NOTIFY_STANDARD));
 for(unsigned i=0;i<4;i++){char title[20];snprintf(title,sizeof(title),"INFORMATION %u",i);ui_toast_show(UI_STATUS_INFO,title,"Reported routine event");}
 assert(s_notice_count==4);unsigned logged=log_count;
 ui_toast_show_link(UI_STATUS_WARNING,"PRINTER OFFLINE","Required connection warning",UI_SHELL_PAGE_PRINTER);
 assert(s_notice_count==4 && log_count==logged+1 && s_notices[0].kind==UI_STATUS_WARNING && s_overflow==1);bounds(s_toast);
 toast_show(UI_STATUS_INFO,"RECOVERY","Important recovery context",UI_SHELL_PAGE_PRINTER,true);
 assert(ui_toast_set_level(UI_NOTIFY_QUIET));assert(s_notice_count==2 && s_notices[0].kind==UI_STATUS_WARNING && s_notices[1].essential);
 ui_toast_close();assert(ui_toast_set_level(UI_NOTIFY_STANDARD));
 ui_toast_show(UI_STATUS_INFO,"ROUTINE","Suppress on lowering level");assert(s_notice_count==1);assert(ui_toast_set_level(UI_NOTIFY_QUIET));assert(!s_notice_count&&!s_toast);
 for(unsigned i=0;i<4;i++){char title[20];snprintf(title,sizeof(title),"DANGER %u",i);ui_toast_show(UI_STATUS_DANGER,title,"Unresolved fault");}
 ui_toast_show(UI_STATUS_WARNING,"WARNING","Must not displace faults");assert(s_notice_count==4 && s_overflow==1);
 for(unsigned i=0;i<4;i++)assert(s_notices[i].kind==UI_STATUS_DANGER);
 ui_toast_close();
}

static void inactive_profile_checks(void){
 ui_toast_close();assert(ui_toast_set_level(UI_NOTIFY_QUIET));
 static unsigned run;run++;
 memset(profiles,0,sizeof(profiles));memset(health_known,0,sizeof(health_known));memset(health_online,0,sizeof(health_online));memset(health_fresh,0,sizeof(health_fresh));
 active_profile=0;
 profiles[1]=(moonraker_profile_t){.configured=true,.port=7126};
 snprintf(profiles[1].host,sizeof(profiles[1].host),"printer-b-%u.local",run);
 snprintf(profiles[1].name,sizeof(profiles[1].name),"Printer B");
 moonraker_state_t state={.moonraker_ok=true,.live_data_ok=true};
 health_known[1]=true;ui_toast_observe_profiles(&state);assert(!s_notice_count);
 health_online[1]=health_fresh[1]=true;ui_toast_observe_profiles(&state);assert(!s_notice_count);
 health_known[1]=false;health_online[1]=false;ui_toast_observe_profiles(&state);assert(!s_notice_count);
 health_known[1]=true;ui_toast_observe_profiles(&state);
 assert(s_notice_count==1&&s_notices[0].kind==UI_STATUS_WARNING&&s_notices[0].profile_index==1&&!strcmp(s_notices[0].printer,"Printer B"));bounds(s_toast);
 unsigned logged=log_count;for(unsigned i=0;i<10;i++)ui_toast_observe_profiles(&state);assert(s_notice_count==1&&log_count==logged);
 health_online[1]=true;health_fresh[1]=false;ui_toast_observe_profiles(&state);assert(!strcmp(s_notices[0].title,"PRINTER OFFLINE"));
 health_fresh[1]=true;ui_toast_observe_profiles(&state);assert(s_notice_count==1&&s_notices[0].essential&&!strcmp(s_notices[0].title,"PRINTER RECONNECTED"));
 ui_toast_set_printer_chooser_action(open_chooser);unsigned opened_before=chooser_opens;owner++;open_notice(NULL);assert(chooser_opens==opened_before+1&&active_profile==0&&!s_notice_count);
 /* Equal display names must not merge independent printers' notices. */
 profiles[2]=profiles[1];profiles[2].port=7127;
 health_known[2]=health_online[2]=health_fresh[2]=true;
 ui_toast_observe_profiles(&state);assert(!s_notice_count);
 health_online[1]=health_online[2]=false;ui_toast_observe_profiles(&state);assert(s_notice_count==2);
 health_online[2]=true;ui_toast_observe_profiles(&state);assert(s_notice_count==2);
 bool first_offline=false,second_recovered=false;
 for(unsigned i=0;i<s_notice_count;i++){
  if(s_notices[i].profile_index==1)first_offline=!strcmp(s_notices[i].title,"PRINTER OFFLINE");
  if(s_notices[i].profile_index==2)second_recovered=!strcmp(s_notices[i].title,"PRINTER RECONNECTED");
 }
 assert(first_offline&&second_recovered);ui_toast_close();profiles[2].configured=false;
 /* Edited endpoints and initial offline slots must establish a new baseline. */
 profiles[1].port++;health_online[1]=false;ui_toast_observe_profiles(&state);assert(!s_notice_count);
 health_online[1]=true;ui_toast_observe_profiles(&state);assert(!s_notice_count);
 health_online[1]=false;ui_toast_observe_profiles(&state);assert(s_notice_count==1);
 ui_toast_close();profiles[1].configured=false;ui_toast_observe_profiles(&state);assert(!s_notice_count);
}

static void preference_nested(void){preference_save(NULL);preference_toggle(NULL);}
static void preference_replacement(void){ui_toast_preferences_close();ui_toast_preferences_show(changed);}
static void increment_owner(void){owner++;}
static void log_teardown(void){ui_toast_close();}
static void replace_surface(lv_event_t *event){(void)event;ui_toast_show(UI_STATUS_WARNING,"REPLACEMENT","New notification");}
static void switch_owner_on_delete(lv_event_t *event){(void)event;owner++;}
static void nested_notice_action(lv_event_t *event){(void)event;open_notice(NULL);dismiss_notice(NULL);}
static void notification_lifecycle_checks(void)
{
    ui_toast_close();assert(ui_toast_set_level(UI_NOTIFY_DETAILED));
    ui_toast_show(UI_STATUS_OK,"TEMPORARY","");lv_timer_t *retired=s_timer;
    ui_toast_show(UI_STATUS_DANGER,"PERSISTENT","Keep this visible");assert(s_toast&&!s_timer);
    toast_timer_cb(retired);assert(s_toast&&s_notice_count==1);ui_toast_close();
    ui_toast_show(UI_STATUS_WARNING,"ORIGINAL","Review this notice");
    lv_obj_add_event_cb(s_toast,replace_surface,LV_EVENT_DELETE,NULL);
    dismiss_notice(NULL);assert(s_toast&&s_notice_count==1&&!strcmp(s_notices[0].title,"REPLACEMENT"));ui_toast_close();
    ui_toast_show_link(UI_STATUS_WARNING,"LINK","Review",UI_SHELL_PAGE_PRINTER);
    unsigned before=opens;lv_obj_add_event_cb(s_toast,switch_owner_on_delete,LV_EVENT_DELETE,NULL);open_notice(NULL);assert(opens==before&&!s_notice_count);
    ui_toast_show_link(UI_STATUS_WARNING,"LINK","Review",UI_SHELL_PAGE_CONSOLE);
    unsigned nav_before=nav_calls;navigation_hook=increment_owner;open_notice(NULL);assert(opens==before+1&&nav_calls==nav_before&&!s_notice_count);
    ui_toast_show_link(UI_STATUS_WARNING,"LINK","Review",UI_SHELL_PAGE_CONSOLE);
    before=opens;lv_obj_add_event_cb(s_toast,nested_notice_action,LV_EVENT_DELETE,NULL);open_notice(NULL);assert(opens==before+1&&!s_notice_count);
    ui_toast_show_link(UI_STATUS_WARNING,"LINK","Review",UI_SHELL_PAGE_CONSOLE);lv_obj_delete(s_notice_open);assert(!s_notice_open);before=opens;open_notice(NULL);assert(opens==before&&s_notice_count==1);ui_toast_close();
    ui_toast_show_link(UI_STATUS_WARNING,"BODY RETIRED","Review",UI_SHELL_PAGE_CONSOLE);lv_obj_delete(s_notice_body);assert(!s_notice_body);before=opens;open_notice(NULL);assert(opens==before&&s_notice_count==1);dismiss_notice(NULL);assert(!s_toast&&!s_notice_count);
    logging_hook=log_teardown;ui_toast_show(UI_STATUS_WARNING,"RETIRED","Do not resurrect");assert(!s_toast&&!s_notice_count);
    logging_hook=increment_owner;ui_toast_show(UI_STATUS_WARNING,"OLD OWNER","Do not redirect");assert(!s_toast&&!s_notice_count);
    ui_toast_preferences_show(changed);s_pending_level=UI_NOTIFY_QUIET;unsigned saved_before=commits,changed_before=changes;
    storage_hook=preference_nested;preference_save(NULL);assert(commits==saved_before+1&&changes==changed_before+1&&!s_preference_popup&&ui_toast_level()==UI_NOTIFY_QUIET);
    ui_toast_preferences_show(changed);s_pending_level=UI_NOTIFY_STANDARD;changed_before=changes;storage_hook=preference_replacement;preference_save(NULL);assert(s_preference_popup&&changes==changed_before);ui_toast_preferences_close();
    ui_toast_preferences_show(changed);s_pending_level=UI_NOTIFY_DETAILED;fail_write=true;storage_hook=preference_replacement;preference_save(NULL);assert(s_preference_popup&&changes==changed_before&&ui_toast_level()==UI_NOTIFY_STANDARD);fail_write=false;
    lv_obj_delete(s_preference_status);assert(!s_preference_status);fail_write=true;preference_save(NULL);assert(s_preference_popup);fail_write=false;
    lv_obj_delete(s_level_buttons[s_pending_level]);assert(!s_level_buttons[s_pending_level]);saved_before=commits;preference_save(NULL);assert(commits==saved_before);preference_toggle(NULL);ui_toast_preferences_close();
    assert(!s_preference_popup&&!s_preference_save);preference_save(NULL);preference_toggle(NULL);assert(ui_toast_set_level(UI_NOTIFY_QUIET));
}

/* Appearance changes must rebuild the persistent top-layer notice as well
 * as pages, without logging twice, discarding faults or retargeting actions. */
static void notification_theme_switch_checks(void)
{
    ui_toast_close();
    lv_display_set_resolution(lv_display_get_default(),1024,600);
    ui_theme_set_active(UI_THEME_OPERATOR);
    ui_theme_set_density(UI_DENSITY_COMFORTABLE);
    ui_theme_set_accessibility((ui_accessibility_t){0});
    assert(ui_toast_set_level(UI_NOTIFY_STANDARD));
    ui_toast_show_link(UI_STATUS_WARNING,"LINKED FAULT","Review this printer",UI_SHELL_PAGE_CONSOLE);
    ui_toast_show(UI_STATUS_DANGER,"URGENT FAULT","Keep this fault until dismissed");
    notice_t saved[NOTICE_CAPACITY];memcpy(saved,s_notices,sizeof(saved));
    unsigned count=s_notice_count,overflow=s_overflow,logged=log_count;
    for(unsigned i=0;i<4;i++){
        ui_theme_set_active(i%2?UI_THEME_OPERATOR:UI_THEME_STUDIO_DARK);
        ui_theme_set_density(i%2?UI_DENSITY_COMPACT:UI_DENSITY_LARGE);
        ui_theme_set_accessibility((ui_accessibility_t){.large_text=!(i%2)});
        ui_toast_refresh_appearance();
        assert(s_toast&&!s_timer&&s_notice_count==count&&s_overflow==overflow&&log_count==logged);
        assert(!memcmp(saved,s_notices,sizeof(saved)));
        bounds(s_toast);
        lv_obj_t *reference=lv_obj_create(lv_screen_active());ui_apply_popup_style(reference);
        assert(lv_color_eq(lv_obj_get_style_bg_color(s_toast,0),lv_obj_get_style_bg_color(reference,0)));
        assert(lv_obj_get_style_radius(s_toast,0)==lv_obj_get_style_radius(reference,0));
        assert(lv_obj_get_style_border_width(s_toast,0)==lv_obj_get_style_border_width(reference,0));
        assert(lv_color_eq(lv_obj_get_style_border_color(s_toast,0),ui_status_color(s_notices[0].kind)));
        const lv_font_t *fonts[]={UI_FONT_CAPTION,UI_FONT_BODY_LARGE,UI_FONT_BODY};
        lv_color_t colors[]={UI_TEXT_DIM,ui_status_color(s_notices[0].kind),UI_TEXT};
        for(unsigned j=0;j<3;j++){
            lv_obj_t *expected=lv_label_create(reference);ui_apply_custom_label_style(expected,fonts[j],colors[j]);
            lv_obj_t *label=lv_obj_get_child(s_notice_body,j);
            assert(lv_obj_get_style_text_font(label,0)==lv_obj_get_style_text_font(expected,0));
            assert(lv_color_eq(lv_obj_get_style_text_color(label,0),colors[j]));
        }
        lv_obj_delete(reference);
        capture("notification-theme-switch");
    }
    dismiss_notice(NULL);assert(s_notice_count==1&&s_notice_open);
    unsigned before=opens;open_notice(NULL);assert(opens==before+1&&opened==UI_SHELL_PAGE_CONSOLE&&!s_notice_count);
    assert(ui_toast_set_level(UI_NOTIFY_DETAILED));
    ui_toast_show(UI_STATUS_OK,"SAVED","Optional confirmation");assert(s_toast&&s_timer);
    ui_toast_refresh_appearance();assert(!s_toast&&!s_timer&&!s_notice_count);
    ui_toast_refresh_appearance();assert(!s_toast&&!s_timer&&!s_notice_count);
    assert(ui_toast_set_level(UI_NOTIFY_QUIET));
}

int main(void){lv_init();lv_display_t *d=lv_display_create(1024,600);static uint8_t pixels[1024*50*2];lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);lv_display_set_buffers(d,pixels,NULL,sizeof(pixels),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(d,flush);
 for(unsigned theme=0;theme<5;theme++)for(unsigned density=0;density<3;density++)for(unsigned large=0;large<2;large++)for(unsigned narrow=0;narrow<3;narrow++){
  lv_display_set_resolution(d,(int[]){1024,640,480}[narrow],narrow==2?400:600);ui_theme_set_active(theme);ui_theme_set_density(density);ui_theme_set_accessibility((ui_accessibility_t){.large_text=large,.reduced_motion=true});has_saved=false;legacy_saved=false;stored=0;fail_write=false;log_count=changes=0;ui_toast_init();assert(!ui_toast_confirmations_enabled());
  ui_toast_show(UI_STATUS_OK,"SAVED","No essential instructions here");assert(!s_toast&&!s_timer);
  ui_toast_show(UI_STATUS_DANGER,"STOP NOT CONFIRMED","Use the printer power switch if needed.");assert(s_toast&&!s_timer&&s_notice_count==1&&log_count==1);bounds(s_toast);capture("notification-fault");lv_tick_inc(10000);lv_timer_handler();assert(s_toast&&s_notice_count==1);
  ui_toast_show(UI_STATUS_DANGER,"STOP NOT CONFIRMED","Use the printer power switch if needed.");assert(s_notice_count==1&&log_count==1);
  assert(ui_toast_set_confirmations(true));ui_toast_show(UI_STATUS_OK,"SAVED","");assert(s_notice_count==1&&s_toast&&!s_timer);ui_toast_init();assert(ui_toast_confirmations_enabled());
  strcpy(printer,"Printer B");ui_toast_show(UI_STATUS_WARNING,"OFFLINE","Check connection");assert(s_notice_count==2&&!strcmp(s_notices[0].printer,"Printer A")&&!strcmp(s_notices[1].printer,"Printer B"));dismiss_notice(NULL);assert(s_notice_count==1&&!strcmp(s_notices[0].printer,"Printer B"));dismiss_notice(NULL);assert(!s_toast&&!s_notice_count);
  ui_toast_show(UI_STATUS_OK,"SAVED","Optional confirmation");assert(s_toast&&s_timer);lv_tick_inc(3000);lv_timer_handler();assert(!s_toast&&!s_timer);
  ui_toast_preferences_show(changed);bounds(s_preference_popup);capture("notification-preferences");preference_toggle(NULL);assert(s_pending_level==UI_NOTIFY_QUIET);fail_write=true;preference_save(NULL);assert(s_preference_popup&&ui_toast_confirmations_enabled()&&strstr(lv_label_get_text(s_preference_status),"Could not save"));fail_write=false;preference_save(NULL);assert(!s_preference_popup&&!ui_toast_confirmations_enabled()&&changes==1);ui_toast_init();assert(!ui_toast_confirmations_enabled());
  for(unsigned i=0;i<6;i++){char title[32];snprintf(title,sizeof(title),"FAULT %u",i);ui_toast_show(UI_STATUS_DANGER,title,"Important instructions remain visible until dismissed.");}assert(s_notice_count==4&&s_overflow==2);bounds(s_toast);unsigned pending=s_notice_count;assert(ui_toast_set_level(UI_NOTIFY_STANDARD));ui_toast_show(UI_STATUS_INFO,"INFO","Additional context");assert(s_notice_count==pending&&s_overflow==3);assert(!strcmp(s_notices[0].title,"FAULT 5"));
  lv_obj_delete(s_toast);assert(!s_toast&&s_notice_count==4);render_notice();assert(s_toast);ui_toast_close();assert(!s_toast&&!s_notice_count);
  ui_toast_show_link(UI_STATUS_DANGER,"COMMAND FAILED","Check the failed command.",UI_SHELL_PAGE_CONSOLE);bounds(s_toast);
  unsigned before=opens;owner++;open_notice(NULL);assert(opens==before&&s_notice_count==1);
  owner--;open_notice(NULL);assert(opens==before+1&&opened==UI_SHELL_PAGE_CONSOLE&&!s_notice_count);
  for(unsigned level=0;level<3;level++) {
   assert(ui_toast_set_level((ui_notification_level_t)level));ui_toast_init();assert(ui_toast_level()==(ui_notification_level_t)level);
   ui_toast_show(UI_STATUS_INFO,"EVENT","Connection/print event");assert((s_notice_count!=0)==(level>=1));ui_toast_close();
   ui_toast_show(UI_STATUS_OK,"SAVED","");assert((s_toast!=NULL)==(level==2));ui_toast_close();
   ui_toast_show(UI_STATUS_WARNING,"WARNING","Required at every level");assert(s_notice_count==1);ui_toast_close();
  }
  assert(!ui_toast_set_level((ui_notification_level_t)3));
  has_saved=false;legacy_saved=true;legacy_value=1;ui_toast_init();assert(ui_toast_level()==UI_NOTIFY_DETAILED);
  legacy_value=0;ui_toast_init();assert(ui_toast_level()==UI_NOTIFY_QUIET);
  has_saved=true;stored=9;legacy_value=1;ui_toast_init();assert(ui_toast_level()==UI_NOTIFY_QUIET);legacy_saved=false;
  assert(ui_toast_set_level(UI_NOTIFY_STANDARD));
  moonraker_state_t observed={.live_data_ok=true};strcpy(observed.printer_state,"ready");owner++;
  ui_toast_observe_printer(&observed);assert(!s_notice_count);
  lv_tick_inc(3000);ui_toast_observe_printer(&observed);assert(!s_notice_count);
  strcpy(observed.printer_state,"printing");ui_toast_observe_printer(&observed);assert(s_notice_count==1);
  ui_toast_observe_printer(&observed);assert(s_notice_count==1);ui_toast_close();
  observed.live_data_ok=false;ui_toast_observe_printer(&observed);assert(!s_notice_count);
  lv_tick_inc(1000);observed.live_data_ok=true;ui_toast_observe_printer(&observed);assert(!s_notice_count);
  observed.live_data_ok=false;ui_toast_observe_printer(&observed);lv_tick_inc(2999);ui_toast_observe_printer(&observed);assert(!s_notice_count);
  lv_tick_inc(1);ui_toast_observe_printer(&observed);assert(s_notice_count==1);ui_toast_close();
  observed.live_data_ok=true;ui_toast_observe_printer(&observed);assert(!s_notice_count);
  lv_tick_inc(3000);ui_toast_observe_printer(&observed);assert(s_notice_count==1);ui_toast_close();
  /* Fresh chooser owner: cached live, bootstrap gap, then first connection
   * must never produce OFFLINE or RECONNECTED, even on slow startup. */
  owner++;ui_toast_observe_printer(&observed);observed.live_data_ok=false;ui_toast_observe_printer(&observed);
  lv_tick_inc(10000);ui_toast_observe_printer(&observed);assert(!s_notice_count);
  observed.live_data_ok=true;ui_toast_observe_printer(&observed);lv_tick_inc(3000);ui_toast_observe_printer(&observed);assert(!s_notice_count);
  assert(ui_toast_set_level(UI_NOTIFY_QUIET));strcpy(observed.printer_state,"complete");ui_toast_observe_printer(&observed);assert(!s_notice_count);
  attention_checks();
  inactive_profile_checks();
  notification_lifecycle_checks();
  ui_toast_preferences_show(NULL);lv_obj_delete(s_preference_popup);assert(!s_preference_popup&&!s_preference_value);assert(!lv_obj_get_child_count(lv_layer_top()));strcpy(printer,"Printer A");
 }
 notification_theme_switch_checks();
 lv_display_delete(d);lv_deinit();puts("PASS: global quiet/confirmation policy, persistent faults, deduplication/priority/bounded queue, event history, profile identity, preference persistence/failure, layouts and lifecycle across all themes/densities/text sizes/viewports");return 0;}
