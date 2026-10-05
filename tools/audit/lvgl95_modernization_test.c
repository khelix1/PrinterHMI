/* Real LVGL layouts and page lifecycle; fixtures replace printer transport. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "custom_theme.h"
#include "moonraker.h"
#include "moonraker_config_controller.h"
#include "ui_toast.h"
#include "console_controller.h"
#include "macro_controller.h"
#include "device_catalog_controller.h"
#include "ui_devices_live_values.h"
#include "ui_popup.h"
bool custom_theme_color_override(uint32_t a,uint32_t b,uint32_t c,uint32_t *d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_metric_override(int32_t a,int32_t b,int32_t c,int32_t *d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_accent_override(uint8_t a,uint32_t *b){(void)a;(void)b;return false;}
bool custom_theme_surface_opacity(uint8_t *a){(void)a;return false;}
void moonraker_state_snapshot(moonraker_state_t *s){memset(s,0,sizeof(*s));s->moonraker_ok=s->live_data_ok=true;}
uint32_t moonraker_config_generation(void){return 1;}
void ui_toast_show(ui_status_kind_t k,const char *t,const char *d){(void)k;(void)t;(void)d;}
static size_t entries=64;
size_t console_controller_count(void){return entries;}
uint32_t console_controller_latest_sequence(void){return (uint32_t)entries;}
bool console_controller_get(size_t i,console_entry_t *e){if(i>=entries)return false;memset(e,0,sizeof(*e));e->type=i%2?CONSOLE_ENTRY_ERROR:CONSOLE_ENTRY_RESPONSE;e->sequence=(uint32_t)i;snprintf(e->message,sizeof(e->message),"Message %u",(unsigned)i);return true;}
void console_controller_clear(void){entries=0;}
size_t console_controller_history_count(void){return 0;}
bool console_controller_history_get(size_t i,char *o,size_t n){(void)i;(void)o;(void)n;return false;}
void console_controller_add_command(const char *c){(void)c;}
void console_controller_add(console_entry_type_t t,const char *f,...){(void)t;(void)f;}
#if defined(TEST_CONSOLE)
#include "ui_console.c"
static void run(void){
 ui_console_show(NULL);lv_obj_update_layout(s_root);
 lv_obj_t *first=s_rows->rows[0],*last=s_rows->rows[63];assert(first && last);
 s_follow=false;lv_obj_scroll_to_y(s_output,140,LV_ANIM_OFF);int scroll=lv_obj_get_scroll_y(s_output);
 rebuild_output();assert(lv_obj_get_scroll_y(s_output)==scroll);
 assert(first==s_rows->rows[0] && last==s_rows->rows[63]);
 s_filter=CONSOLE_FILTER_ERRORS;rebuild_output();
 for(int i=0;i<64;i++){assert(lv_obj_has_flag(s_rows->rows[i],LV_OBJ_FLAG_HIDDEN)==(i>=32));if(i<32)assert(lv_obj_get_y(s_rows->rows[i])==10+i*38);}
 s_filter=CONSOLE_FILTER_ALL;entries=0;rebuild_output();assert(s_rows->empty && !lv_obj_has_flag(s_rows->empty,LV_OBJ_FLAG_HIDDEN));
 lv_obj_t *empty=s_rows->empty;entries=64;rebuild_output();assert(s_rows->rows[0]==first && lv_obj_has_flag(empty,LV_OBJ_FLAG_HIDDEN));
 assert(lv_obj_get_child_count(s_output)==65);
 ui_console_hide();assert(!s_rows->rows[0] && !s_rows->empty);ui_console_show(NULL);assert(s_rows->rows[63]);ui_console_hide();
 puts("PASS: Console row reuse, filtering, bounded objects, Follow Off and reopen");
}
#elif defined(TEST_MACROS)
static uint32_t gen=1;static bool fav[64];static char selected[64];
void macro_controller_status(macro_controller_status_t *s){memset(s,0,sizeof(*s));s->discovered=true;s->count=s->total_count=64;s->generation=gen;}
bool macro_controller_get(size_t i,char *o,size_t n){if(i>=64)return false;snprintf(o,n,"MACRO_%02u",(unsigned)i);return true;}
bool macro_controller_is_favorite(const char *n){unsigned i=0;sscanf(n,"MACRO_%u",&i);return i<64 && fav[i];}
bool macro_controller_toggle_favorite(const char *n){unsigned i=0;sscanf(n,"MACRO_%u",&i);if(i>=64)return false;fav[i]=!fav[i];gen++;return true;}
bool macro_controller_parameters(const char *n,macro_parameter_catalog_t *p){snprintf(selected,sizeof(selected),"%s",n);memset(p,0,sizeof(*p));p->count=8;for(int i=0;i<8;i++)snprintf(p->names[i],sizeof(p->names[i]),"PARAM_%d",i);return true;}
#include "ui_macros.c"
static void run(void){
 ui_macros_show(NULL);lv_obj_t *first=s_macros->rows[0];assert(first && s_macros->rows[63]);
 fav[63]=true;gen++;rebuild_macro_list();assert(s_macros->rows[0]==first && (uintptr_t)lv_obj_get_user_data(first)==64);
 lv_obj_send_event(first,LV_EVENT_CLICKED,NULL);assert(!strcmp(selected,"MACRO_63"));assert(s_confirm);
 lv_obj_update_layout(s_confirm);for(int i=0;i<10;i++)assert(s_macros->parameter_fields[i]);
 for(int i=0;i<10;i+=2){lv_obj_t *a=lv_obj_get_parent(s_macros->parameter_fields[i]),*b=lv_obj_get_parent(s_macros->parameter_fields[i+1]);lv_area_t aa,bb;lv_obj_get_coords(a,&aa);lv_obj_get_coords(b,&bb);assert(aa.x2<bb.x1);assert(aa.y1==bb.y1);if(i>=2){lv_area_t prev;lv_obj_get_coords(lv_obj_get_parent(s_macros->parameter_fields[i-2]),&prev);assert(prev.y2<aa.y1);}}
 close_confirm();gen++;selected[0]=0;lv_obj_send_event(first,LV_EVENT_CLICKED,NULL);assert(!s_confirm && !selected[0]);
 strcpy(s_macros->query,"MACRO_12");rebuild_macro_list();assert(first==s_macros->rows[0] && (uintptr_t)lv_obj_get_user_data(first)==13);assert(lv_obj_has_flag(s_macros->rows[1],LV_OBJ_FLAG_HIDDEN));
 lv_obj_send_event(first,LV_EVENT_LONG_PRESSED,NULL);assert(fav[12]);
 strcpy(s_macros->query,"nothing");rebuild_macro_list();assert(s_macros->empty && !lv_obj_has_flag(s_macros->empty,LV_OBJ_FLAG_HIDDEN));assert(lv_obj_get_child_count(s_list)==65);
 ui_macros_hide();assert(!s_macros->rows[0]);s_macros->query[0]=0;ui_macros_show(NULL);assert(s_macros->rows[63]);ui_macros_hide();
 puts("PASS: Macro row reuse, favorite/search rebinding, stale callbacks, ten-field Grid and reopen");
}
#elif defined(TEST_DEVICES)
static size_t bindings,indices[12];static bool discovered=true;
void device_catalog_controller_status(device_catalog_status_t *s){memset(s,0,sizeof(*s));s->discovered=discovered;s->stored_count=s->total_object_count=25;s->kind_count[DEVICE_KIND_THERMAL]=13;s->kind_count[DEVICE_KIND_AIR]=12;s->generation=1;}
bool device_catalog_controller_get(size_t i,device_descriptor_t *d){if(i>=25)return false;memset(d,0,sizeof(*d));snprintf(d->object_name,sizeof(d->object_name),"device_%02u",(unsigned)i);snprintf(d->display_name,sizeof(d->display_name),"Device %02u",(unsigned)i);d->kind=i%2?DEVICE_KIND_AIR:DEVICE_KIND_THERMAL;return true;}
const char *device_catalog_kind_label(device_kind_t k){(void)k;return "Device";}
void ui_devices_live_values_init(lv_obj_t *o){(void)o;}
void ui_devices_live_values_clear(void){bindings=0;}
void ui_devices_live_values_register(size_t v,lv_obj_t *l,size_t c){assert(v<12 && l);indices[v]=c;bindings++;}
void ui_devices_live_values_update(void){}
void ui_devices_live_values_close(void){bindings=0;}
#include "ui_devices_catalog_view.c"
static void run(void){
 lv_obj_t *owner=lv_obj_create(lv_screen_active());lv_obj_t *banner=lv_label_create(owner);ui_devices_catalog_view_create(owner,banner);
 lv_obj_t *first=s_devices->rows[0].card;assert(first && bindings==12);s_devices->page_index=1;render_catalog();assert(first==s_devices->rows[0].card && bindings==12 && indices[0]==12);
 s_devices->page_index=2;render_catalog();assert(bindings==1 && indices[0]==24);assert(lv_obj_has_flag(s_devices->rows[1].card,LV_OBJ_FLAG_HIDDEN));
 s_devices->filter=DEVICE_FILTER_AIR;s_devices->page_index=0;render_catalog();assert(bindings==12 && indices[0]==1 && indices[11]==23);
 discovered=false;render_catalog();assert(!bindings && s_devices->empty);discovered=true;s_devices->filter=DEVICE_FILTER_OTHER;render_catalog();assert(!bindings);assert(lv_obj_get_child_count(s_devices->list)==13);
 s_devices->filter=DEVICE_FILTER_ALL;render_catalog();assert(first==s_devices->rows[0].card);
 ui_devices_catalog_view_close();assert(!s_devices->rows[0].card);lv_obj_delete(owner);
 puts("PASS: Device card reuse, page/filter binding indices, bounded objects and teardown");
}
#else
static int clicks;
static void close_cb(lv_event_t *e){clicks++;lv_obj_t *p=ui_popup_find_owner(lv_event_get_target(e));assert(p);lv_obj_delete(p);}
static void run(void){
 for(int theme=0;theme<3;theme++)for(int large=0;large<2;large++)for(int actions=1;actions<=3;actions++){
  ui_theme_set_active((ui_theme_id_t)theme);ui_theme_set_accessibility((ui_accessibility_t){.large_text=large});
  int before=lv_obj_get_child_count(lv_layer_top());lv_obj_t *p=ui_popup_create(lv_screen_active(),480,300,UI_POPUP_STANDARD);lv_obj_t *b[3]={0};
  for(int i=actions-1;i>=0;i--)b[i]=ui_popup_add_footer_action(p,UI_POPUP_ACTION_CLOSE,"LONG ACTION LABEL",170,(ui_popup_footer_slot_t)i,close_cb,NULL,NULL);
  for(int resize=0;resize<3;resize++){
   lv_obj_set_width(p,resize==0?480:resize==1?620:800);lv_obj_update_layout(p);lv_area_t pp;lv_obj_get_coords(p,&pp);
   for(int i=0;i<actions;i++){lv_area_t a;lv_obj_get_coords(b[i],&a);assert(a.x1>=pp.x1 && a.x2<=pp.x2 && a.y1>=pp.y1 && a.y2<=pp.y2);assert(lv_obj_get_height(b[i])==48);assert(ui_popup_find_owner(lv_obj_get_child(b[i],0))==p);if(i){lv_area_t prev;lv_obj_get_coords(b[i-1],&prev);assert(prev.x2<a.x1);}}
  }
  lv_obj_send_event(b[0],LV_EVENT_CLICKED,NULL);assert((int)lv_obj_get_child_count(lv_layer_top())==before);
 }
 assert(clicks==18);puts("PASS: popup Grid bounds, resizing, all themes/text sizes, owner callbacks and modal cleanup");
}
#endif
int main(void){lv_init();lv_display_create(1024,600);run();lv_deinit();return 0;}
