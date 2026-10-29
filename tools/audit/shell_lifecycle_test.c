/* Real shell roots, widgets, clock and E-stop with deterministic wall time. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "custom_theme.h"
#include "ui_global_estop.h"
static time_t wall_time;
static time_t test_time(time_t *out){if(out)*out=wall_time;return wall_time;}
#define time test_time
#include "ui_shell.c"
#undef time
bool custom_theme_color_override(uint32_t a,uint32_t b,uint32_t c,uint32_t*d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_metric_override(int32_t a,int32_t b,int32_t c,int32_t*d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_accent_override(uint8_t a,uint32_t*b){(void)a;(void)b;return false;}
bool custom_theme_surface_opacity(uint8_t*a){(void)a;return false;}
static unsigned invalidations,switches,routes,stops;
static int last_page;
uint32_t moonraker_config_generation(void){return 1;}
void ui_toast_show(ui_status_kind_t kind,const char *t,const char *d){(void)kind;(void)t;(void)d;}
void ui_shell_page_action(ui_shell_page_t page){routes++;last_page=page;}
static bool send(const char *command){assert(!strcmp(command,"M112"));stops++;return true;}
static void select_printer(void){switches++;}
static void flush(lv_display_t *d,const lv_area_t *area,uint8_t *p){(void)area;(void)p;lv_display_flush_ready(d);}
static void invalidated(lv_event_t *e){(void)e;invalidations++;}
static unsigned timers(void){unsigned n=0;for(lv_timer_t *t=lv_timer_get_next(NULL);t;t=lv_timer_get_next(t))n++;return n;}
static unsigned objects(lv_obj_t *root){unsigned n=1;for(unsigned i=0;i<lv_obj_get_child_count(root);i++)n+=objects(lv_obj_get_child(root,i));return n;}
static lv_obj_t *find(lv_obj_t *root,const char *text){if(lv_obj_check_type(root,&lv_label_class)&&!strcmp(lv_label_get_text(root),text))return root;for(unsigned i=0;i<lv_obj_get_child_count(root);i++){lv_obj_t *p=find(lv_obj_get_child(root,i),text);if(p)return p;}return NULL;}
static void click(lv_obj_t *obj){lv_obj_send_event(obj,LV_EVENT_CLICKED,NULL);}
static void reopen(lv_event_t *e){(void)e;ui_shell_create();ui_shell_create_nav();}
static void idle(lv_timer_t *timer){(void)timer;}
static void quiet(lv_display_t *d)
{
 lv_refr_now(d);invalidations=0;
 for(unsigned i=0;i<100;i++){ui_shell_refresh_clock();ui_shell_set_wifi_signal(true,-55);}
 assert(!invalidations);
}
int main(void)
{
 setenv("TZ","UTC0",1);tzset();struct tm clock={.tm_year=124,.tm_mon=0,.tm_mday=1,.tm_hour=13,.tm_min=5};wall_time=mktime(&clock);
 lv_init();lv_display_t *d=lv_display_create(1024,600);static uint8_t pixels[1024*32*2];
 lv_display_set_buffers(d,pixels,NULL,sizeof(pixels),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);lv_display_set_flush_cb(d,flush);lv_display_add_event_cb(d,invalidated,LV_EVENT_INVALIDATE_AREA,NULL);
 assert(ui_global_estop_init(send));
 for(unsigned theme=0;theme<5;theme++)for(unsigned density=0;density<3;density++)for(unsigned large=0;large<2;large++){
  ui_theme_set_active(theme);ui_theme_set_density(density);ui_theme_set_accessibility((ui_accessibility_t){.large_text=large});
  unsigned base_timers=timers(),base_objects=objects(lv_screen_active())+objects(lv_layer_top());
  ui_shell_create();ui_shell_create_nav();ui_shell_set_printer_switch_callback(select_printer);
  lv_obj_t *top=shell_top_bar,*nav=shell_nav_rail;lv_timer_t *clock_timer=s_clock_timer;
  ui_shell_create();ui_shell_create_nav();assert(shell_top_bar==top&&shell_nav_rail==nav&&s_clock_timer==clock_timer);
  /* A direct parent delete must retire references and its periodic callback. */
  lv_obj_delete(top);assert(!shell_top_bar&&!shell_clock_label&&!s_shell_title_label&&!s_clock_timer);
  ui_shell_refresh_clock();ui_shell_update_status_icons();ui_shell_raise_topbar();
  ui_shell_create();assert(shell_top_bar&&s_clock_timer&&shell_nav_rail==nav);
  ui_shell_set_active_printer_name("Sermoon D1");ui_shell_set_wifi_signal(true,-55);quiet(d);
  wall_time+=60;ui_shell_refresh_clock();assert(invalidations);quiet(d);
  wall_time=0;ui_shell_refresh_clock();assert(!strcmp(lv_label_get_text(shell_clock_label),"--:--"));quiet(d);
  wall_time=mktime(&clock);ui_shell_refresh_clock();assert(!strcmp(lv_label_get_text(shell_clock_label),"1:05 PM"));quiet(d);
  lv_timer_t *other=lv_timer_create(idle,60000,NULL);wall_time+=60;shell_clock_timer_cb(other);
  assert(!strcmp(lv_label_get_text(shell_clock_label),"1:05 PM"));shell_clock_timer_cb(s_clock_timer);
  assert(!strcmp(lv_label_get_text(shell_clock_label),"1:06 PM"));lv_timer_delete(other);
  ui_shell_set_wifi_signal(true,-62);assert(s_shell->wifi_signal_bars==4);
  ui_shell_set_wifi_signal(true,-64);assert(s_shell->wifi_signal_bars==3);
  ui_shell_set_wifi_signal(false,-55);assert(s_shell->wifi_signal_bars==0);
  lv_obj_t *stop_label=find(shell_top_bar,LV_SYMBOL_WARNING " E-STOP");assert(stop_label);
  unsigned stopped=stops;click(lv_obj_get_parent(stop_label));assert(stops==stopped+1);
  lv_obj_t *close=find(lv_layer_top(),LV_SYMBOL_CLOSE " Close");assert(close);click(lv_obj_get_parent(close));
  unsigned before=switches;click(s_shell_printer_button);assert(switches==before+1);
  for(unsigned i=0;i<8;i++){before=routes;click(shell_nav_buttons[i]);assert(routes==before+1&&last_page==(int)i);}
  lv_obj_t *fake=lv_button_create(lv_screen_active());lv_obj_add_event_cb(fake,shell_nav_btn_event_cb,LV_EVENT_CLICKED,(void*)(uintptr_t)UI_SHELL_PAGE_PRINTER);
  lv_obj_add_event_cb(fake,shell_printer_switch_event_cb,LV_EVENT_CLICKED,NULL);
  before=routes;unsigned old_switches=switches;click(fake);assert(routes==before&&switches==old_switches);lv_obj_delete(fake);
  lv_obj_delete(shell_topbar_wifi_bars[0]);assert(!shell_topbar_wifi_bars[0]);ui_shell_set_wifi_signal(true,-55);
  lv_obj_delete(shell_clock_label);assert(!shell_clock_label&&!s_clock_timer);ui_shell_refresh_clock();
  lv_obj_delete(s_shell_title_label);assert(!s_shell_title_label);ui_shell_set_active_printer_name("Other printer");
  lv_obj_delete(s_shell_printer_button);assert(!s_shell_printer_button&&!s_shell_title_label);
  lv_obj_delete(shell_nav_buttons[2]);assert(!shell_nav_buttons[2]);ui_shell_set_active_nav(2);
  lv_obj_delete(shell_nav_rail);assert(!shell_nav_rail);ui_shell_raise_nav();ui_shell_create_nav();assert(shell_nav_rail);
  ui_shell_destroy();assert(!shell_top_bar&&!shell_nav_rail&&!s_clock_timer&&timers()==base_timers);
  for(unsigned mode=0;mode<2;mode++){
   ui_shell_create();ui_shell_create_nav();lv_obj_add_event_cb(shell_top_bar,reopen,LV_EVENT_DELETE,NULL);
   if(mode)ui_shell_destroy();else lv_obj_delete(shell_top_bar);
   assert(shell_top_bar&&shell_nav_rail&&s_clock_timer);ui_shell_destroy();assert(timers()==base_timers);
  }
  assert(objects(lv_screen_active())+objects(lv_layer_top())==base_objects);
 }
 /* Destroy-before-create and update-after-retirement are safe no-ops. */
 heap_caps_free(s_shell);s_shell=NULL;
 ui_shell_destroy();ui_shell_raise_topbar();ui_shell_raise_nav();ui_shell_refresh_clock();ui_shell_set_active_nav(0);ui_shell_set_active_printer_name("Early");
 ui_shell_create_nav();assert(s_shell&&shell_nav_rail);ui_shell_destroy();
 puts("PASS: real shell direct/normal/field teardown, replacement callbacks, timer/object balance, redraw silence, deterministic clock, Wi-Fi hysteresis, eight owned routes and early/late calls across all themes/densities/text sizes");
}
