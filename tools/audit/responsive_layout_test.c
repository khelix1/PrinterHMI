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
static bool custom_tokens;
bool custom_theme_metric_override(int32_t a,int32_t b,int32_t c,int32_t *d){(void)a;(void)b;if(custom_tokens){*d=c+2;return true;}(void)d;return false;}
bool custom_theme_accent_override(uint8_t a,uint32_t *b){(void)a;(void)b;return false;}
bool custom_theme_surface_opacity(uint8_t *a){(void)a;return false;}
void moonraker_state_snapshot(moonraker_state_t *s){memset(s,0,sizeof(*s));s->moonraker_ok=s->live_data_ok=true;}
static uint32_t config_generation=1;
uint32_t moonraker_config_generation(void){return config_generation;}
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

#include <stdlib.h>
#include "ui_page_layout_profile.h"
const ui_page_layout_profile_t *custom_theme_page_profile(void){static ui_page_layout_profile_t profile;if(!custom_tokens)return NULL;profile=*ui_page_layout_profile_for_theme(ui_theme_get_active());profile.files.subtitle="Custom styled job library";return &profile;}
void ui_shell_raise_topbar(void){}
void ui_shell_raise_nav(void){}
static void snapshot(lv_obj_t *root);
static void inside(lv_obj_t *child,lv_obj_t *parent){lv_area_t a,b;lv_obj_get_coords(child,&a);lv_obj_get_coords(parent,&b);if(!(a.x1>=b.x1 && a.x2<=b.x2 && a.y1>=b.y1 && a.y2<=b.y2)){fprintf(stderr,"outside: child %d,%d..%d,%d parent %d,%d..%d,%d\n",a.x1,a.y1,a.x2,a.y2,b.x1,b.y1,b.x2,b.y2);abort();}}
static void cards(lv_obj_t *list){
 lv_obj_update_layout(list);
 for(uint32_t i=0;i<lv_obj_get_child_count(list);i++){lv_obj_t *b=lv_obj_get_child(list,i);if(!lv_obj_check_type(b,&lv_button_class) || lv_obj_has_flag(b,LV_OBJ_FLAG_HIDDEN))continue;
  assert(lv_obj_get_width(b)>0 && lv_obj_get_height(b)>=44);
  for(uint32_t c=0;c<lv_obj_get_child_count(b);c++)inside(lv_obj_get_child(b,c),b);
  lv_area_t aa;lv_obj_get_coords(b,&aa);lv_area_t ll;lv_obj_get_coords(list,&ll);assert(aa.x1>=ll.x1 && aa.x2<=ll.x2);
  for(uint32_t j=0;j<i;j++){lv_obj_t *other=lv_obj_get_child(list,j);if(!lv_obj_check_type(other,&lv_button_class)||lv_obj_has_flag(other,LV_OBJ_FLAG_HIDDEN))continue;lv_area_t bb;lv_obj_get_coords(other,&bb);assert(aa.x2<bb.x1 || bb.x2<aa.x1 || aa.y2<bb.y1 || bb.y2<aa.y1);}
 }
}
#if defined(TEST_TOOLS)
#include "ui_tools.c"
static void run(int width){ui_tools_show();lv_obj_set_width(s_root,width);lv_obj_update_layout(s_root);if(!ui_theme_is_studio())cards(s_tiles);else {inside(s_tiles,s_root);cards(s_tiles);}assert(lv_obj_get_child_count(s_tiles)==4);snapshot(s_root);ui_tools_hide();assert(!s_root && !s_tiles);}
#elif defined(TEST_FILES)
#include "ui_page_state.c"
#include "ui_files.c"
char *thumbnail_session_selected_file(void){return "fixture.gcode";}
int moonraker_config_active_profile_index(void){return 0;}
const char *moonraker_config_active_profile_name(void){return "Workshop printer";}
void ui_preview_lightbox_show_file_object(lv_obj_t *i,const char *f){(void)i;(void)f;}
static unsigned starts,cancels,refreshes;
static void refresh_files(void){refreshes++;}
static bool print_accept=true;
static void start_job(void){if(print_accept){starts++;print_confirm_cancel(NULL);}else ui_files_print_feedback("Moonraker rejected the start request. Check connection and retry.");}
static void cancel_job(void){cancels++;ui_files_close_detail_popup();}
static void run(int width){
 ui_files_show();lv_obj_set_width(s_printer_file_popup,width);ui_files_set_search_text("very long query");ui_files_set_sort_text("NEWEST");lv_obj_update_layout(s_printer_file_popup);
 if(!ui_theme_is_studio()){lv_obj_t *toolbar=lv_obj_get_child(s_printer_file_popup,1);cards(toolbar);assert(lv_obj_get_child_count(toolbar)==4);inside(toolbar,s_printer_file_popup);}inside(s_breadcrumb_label,s_printer_file_popup);
 lv_obj_t *viewport=lv_obj_get_parent(s_printer_file_list);inside(viewport,s_printer_file_popup);assert(lv_obj_get_height(viewport)>100);inside(s_printer_file_list,viewport);
 for(int i=0;i<5;i++)ui_files_add_file_entry("folder/Very_long_operator_job_name_for_responsive_row_checks.gcode",1048576,0,i*118);
 ui_files_add_folder_button("Long folder title for row checks","folder",590);lv_obj_update_layout(s_printer_file_popup);cards(s_printer_file_list);
 ui_files_set_status("Moonraker offline. Check the active printer.");lv_obj_update_layout(s_printer_file_popup);inside(s_files_state->root,viewport);inside(s_files_state->title,s_files_state->root);inside(s_files_state->detail,s_files_state->root);
 lv_area_t title,description;lv_obj_get_coords(s_files_state->title,&title);lv_obj_get_coords(s_files_state->detail,&description);assert(title.y2<description.y1);
 ui_thumbnail_t *view=NULL;lv_obj_t *box=NULL;
 ui_files_show_detail_popup("Long_operator_job_name_for_the_inspector_and_ready_to_print_dialog.gcode","Loading metadata",&box,&view,cancel_job,start_job);
 assert(view && box && ui_files_detail_is_open() && lv_obj_has_state(s_detail_start_button,LV_STATE_DISABLED));
 unsigned prior=starts;detail_start_event_cb(NULL);assert(starts==prior);
 ui_files_update_detail_metadata("Estimated time: 02:40\nMaterial: PLA\nLayers: 336\nFilament: 19.8 m\nSize: 4.2 MB\nThumbnail: metadata found\nthumbs/very_long_preview_filename_for_the_selected_object.png\n\nConfirm to start this print.",true);
 assert(!lv_obj_has_state(s_detail_start_button,LV_STATE_DISABLED));detail_start_event_cb(NULL);assert(starts==prior && s_print_confirm_popup);
 assert(!ui_files_can_refresh_rows());
 lv_obj_t *confirm=s_print_confirm_popup;detail_start_event_cb(NULL);assert(confirm==s_print_confirm_popup);
 lv_obj_set_width(confirm,width<640?width:640);
 lv_obj_update_layout(confirm);inside(confirm,lv_screen_active());
 for(uint32_t i=0;i<lv_obj_get_child_count(confirm);i++)inside(lv_obj_get_child(confirm,i),confirm);
 cards(lv_obj_get_child(confirm,-1));snapshot(confirm);
 print_confirm_cancel(NULL);assert(!s_print_confirm_popup && ui_files_detail_is_open() && starts==prior);
 detail_start_event_cb(NULL);print_accept=false;print_confirm_accept(NULL);assert(s_print_confirm_popup&&starts==prior);
 print_accept=true;print_confirm_accept(NULL);assert(!s_print_confirm_popup && starts==prior+1);
 print_confirm_accept(NULL);assert(starts==prior+1);
 if(ui_theme_is_studio()) {
  assert(ui_files_can_refresh_rows());
  lv_obj_t *owner=s_printer_file_popup,*detail=s_file_detail_popup;
  unsigned before_refresh=refreshes;ui_files_set_callbacks(refresh_files,NULL,NULL);ui_files_refresh();
  assert(refreshes==before_refresh+1 && owner==s_printer_file_popup && detail==s_file_detail_popup);
 }
 if(!ui_theme_is_studio())lv_obj_set_width(s_file_detail_popup,width<760?width:760);
 lv_obj_update_layout(s_file_detail_popup);
 for(uint32_t i=0;i<lv_obj_get_child_count(s_file_detail_popup);i++)inside(lv_obj_get_child(s_file_detail_popup,i),s_file_detail_popup);
 lv_obj_t *metadata=lv_obj_get_parent(s_detail_info_label);
 assert(lv_obj_has_flag(metadata,LV_OBJ_FLAG_SCROLLABLE));
 lv_area_t action_before,action_after;lv_obj_get_coords(s_detail_start_button,&action_before);
 int bottom=lv_obj_get_scroll_bottom(metadata);
 if(bottom>0)lv_obj_scroll_to_y(metadata,lv_obj_get_scroll_y(metadata)+bottom,LV_ANIM_OFF);
 lv_obj_update_layout(s_file_detail_popup);
 assert(lv_obj_get_scroll_bottom(metadata)<=1);
 lv_area_t text_area,view_area;lv_obj_get_coords(s_detail_info_label,&text_area);lv_obj_get_coords(metadata,&view_area);assert(text_area.y2<=view_area.y2);
 lv_obj_get_coords(s_detail_start_button,&action_after);assert(!memcmp(&action_before,&action_after,sizeof(action_before)));
 assert(strstr(lv_label_get_text(s_detail_info_label),"Confirm to start this print."));
 if(ui_theme_is_studio()) {
  assert(lv_obj_get_height(s_detail_info_label)>lv_obj_get_height(metadata));
  assert(lv_obj_get_width(box)==352 && lv_obj_get_height(box)==228);
  studio_metadata_open(NULL);assert(!ui_files_can_refresh_rows());assert(s_studio_metadata_popup && s_studio_metadata_label);
  lv_obj_update_layout(s_studio_metadata_popup);
  for(uint32_t i=0;i<lv_obj_get_child_count(s_studio_metadata_popup);i++)inside(lv_obj_get_child(s_studio_metadata_popup,i),s_studio_metadata_popup);
  lv_obj_t *footer=lv_obj_get_child(s_studio_metadata_popup,-1);
  lv_obj_t *close=lv_obj_get_child(footer,0);
  assert(lv_obj_get_width(close)==128 && lv_obj_get_height(close)==48);inside(close,footer);
  lv_area_t ca,fa;lv_obj_get_coords(close,&ca);lv_obj_get_coords(footer,&fa);assert(ca.x2==fa.x2);
  lv_obj_t *close_label=lv_obj_get_child(close,0);
  assert(!strcmp(lv_label_get_text(close_label),"Close"));
  assert(lv_obj_get_width(close_label)>0 && lv_obj_get_width(close_label)<lv_obj_get_width(close));
  inside(close_label,close);
  ui_files_update_detail_metadata("Updated metadata",true);assert(!strcmp(lv_label_get_text(s_studio_metadata_label),"Updated metadata"));
  studio_metadata_close(NULL);assert(!s_studio_metadata_popup && !s_studio_metadata_label);
 } else {
  lv_obj_t *footer=lv_obj_get_parent(s_detail_start_button);cards(footer);inside(footer,s_file_detail_popup);
  assert(lv_label_get_long_mode(lv_obj_get_child(s_file_detail_popup,1))==LV_LABEL_LONG_SCROLL_CIRCULAR);
 }
 ui_files_update_detail_metadata("Metadata unavailable",false);assert(lv_obj_has_state(s_detail_start_button,LV_STATE_DISABLED));
 detail_start_event_cb(NULL);assert(starts==prior+1);
 snapshot(s_file_detail_popup);
 prior=cancels;detail_cancel_event_cb(NULL);assert(cancels==prior+1 && !s_file_detail_popup && !s_detail_start_button && !s_print_confirm_popup);
 ui_files_show_detail_popup("Another_file.gcode","Ready",NULL,NULL,cancel_job,start_job);ui_files_update_detail_metadata("Ready",true);
 detail_start_event_cb(NULL);assert(s_print_confirm_popup);prior=starts;config_generation++;
 print_confirm_accept(NULL);assert(starts==prior && s_print_confirm_popup && s_file_detail_popup);
 ui_files_close_detail_popup();
 ui_files_show_detail_popup("Final_file.gcode","Ready",NULL,NULL,cancel_job,start_job);ui_files_update_detail_metadata("Ready",true);
 detail_start_event_cb(NULL);assert(s_print_confirm_popup);ui_files_close_detail_popup();assert(!s_print_confirm_popup && !s_detail_filename);

 snapshot(s_printer_file_popup);ui_files_hide();assert(!s_file_detail_popup && !s_studio_metadata_popup);assert(!s_printer_file_popup);
}
#elif defined(TEST_NETWORK)
#include "ui_network.c"
void ui_network_create(void){}
void ui_network_destroy(void){ui_network_destroy_objects(NULL,NULL,NULL);}
static unsigned scans,profiles;
static char picked[40];
static void scan_cb(lv_event_t *e){(void)e;scans++;}
static void profiles_cb(lv_event_t *e){(void)e;profiles++;}
static void selected_cb(lv_event_t *e){snprintf(picked,sizeof(picked),"%s",(char *)lv_event_get_user_data(e));}
static void run(int width){
 if(width!=854 && width!=976)return; /* Legacy themes retain their fixed page geometry. */
 ui_network_create_objects("Network connected",7125,NULL,scan_cb,profiles_cb,NULL);
 ui_network_refresh_objects("Printer connected","A_very_long_network_name_12345678","192.168.100.123",true,"very-long-printer-hostname.example.local",200,"Ready to scan");
 ui_network_set_port(7130);assert(!strcmp(lv_label_get_text(s_network.moonraker_port),"7130"));
 lv_obj_update_layout(s_network.root);
 if(ui_theme_is_studio()) {
  assert(lv_obj_get_x(s_network.root)==24 && lv_obj_get_y(s_network.root)==80);
  assert(lv_obj_get_width(s_network.root)==976 && lv_obj_get_height(s_network.root)==424);
  for(uint32_t i=0;i<lv_obj_get_child_count(s_network.root);i++)inside(lv_obj_get_child(s_network.root,i),s_network.root);
  lv_obj_t *panels[]={s_network.wifi_card,s_network.moonraker_card,s_network.actions_card,s_network.banner};
  for(unsigned p=0;p<4;p++)for(uint32_t i=0;i<lv_obj_get_child_count(panels[p]);i++)inside(lv_obj_get_child(panels[p],i),panels[p]);
  cards(s_network.actions_card);
 }
 unsigned prior=scans;lv_obj_send_event(lv_obj_get_child(s_network.actions_card,ui_theme_is_studio()?0:1),LV_EVENT_CLICKED,NULL);assert(scans==prior+1);
 prior=profiles;lv_obj_send_event(lv_obj_get_child(s_network.actions_card,ui_theme_is_studio()?1:2),LV_EVENT_CLICKED,NULL);assert(profiles==prior+1);
 wifi_ap_record_t aps[6]={0};for(unsigned i=0;i<6;i++){snprintf((char *)aps[i].ssid,sizeof(aps[i].ssid),"A_long_access_point_name_%u",i);aps[i].rssi=-57-(int)i;}
 ui_network_render_scan_results(aps,6,8,selected_cb);lv_obj_update_layout(s_network.root);
 assert(lv_obj_get_child_count(s_network.networks_list)==6);
 if(ui_theme_is_studio())cards(s_network.networks_list);
 lv_obj_send_event(lv_obj_get_child(s_network.networks_list,0),LV_EVENT_CLICKED,NULL);assert(!strcmp(picked,(char *)aps[0].ssid));
 snapshot(s_network.root);
 ui_network_render_scan_results(NULL,0,0,selected_cb);assert(lv_obj_get_child_count(s_network.networks_list)==0);
 ui_network_hide();assert(!s_network.root && !s_network.networks_list);
}

#else
static uint32_t gen=1;static bool fav[64];static char selected[64];
static unsigned macro_sends;
static char sent_macro[MACRO_COMMAND_MAX];
static bool send_macro(const char *command){macro_sends++;snprintf(sent_macro,sizeof(sent_macro),"%s",command);return true;}
void macro_controller_status(macro_controller_status_t *s){memset(s,0,sizeof(*s));s->discovered=true;s->count=s->total_count=64;s->generation=gen;}
bool macro_controller_get(size_t i,char *o,size_t n){if(i>=64)return false;snprintf(o,n,"MACRO_%02u_WITH_A_VERY_LONG_OPERATOR_ACTION_NAME",(unsigned)i);return true;}
bool macro_controller_is_favorite(const char *n){unsigned i=0;sscanf(n,"MACRO_%u",&i);return i<64 && fav[i];}
bool macro_controller_toggle_favorite(const char *n){unsigned i=0;sscanf(n,"MACRO_%u",&i);if(i>=64)return false;fav[i]=!fav[i];gen++;return true;}
bool macro_controller_parameters(const char *n,macro_parameter_catalog_t *p){snprintf(selected,sizeof(selected),"%s",n);memset(p,0,sizeof(*p));p->count=8;for(int i=0;i<8;i++)snprintf(p->names[i],sizeof(p->names[i]),"PARAM_%d",i);return true;}

#include "ui_macros.c"
static void macro_popup_bounds(lv_obj_t *popup, int width)
{
 lv_obj_set_width(popup,width<800?width:800);lv_obj_update_layout(popup);
 inside(popup,lv_screen_active());
 for(uint32_t i=0;i<lv_obj_get_child_count(popup);i++)inside(lv_obj_get_child(popup,i),popup);
 cards(lv_obj_get_child(popup,-1));
 lv_obj_t *body=lv_obj_get_child(popup,1),*footer=lv_obj_get_child(popup,-1);
 lv_area_t before,after;lv_obj_get_coords(footer,&before);
 lv_obj_scroll_to_y(body,lv_obj_get_scroll_bottom(body),LV_ANIM_OFF);
 lv_obj_update_layout(popup);lv_obj_get_coords(footer,&after);
 assert(!memcmp(&before,&after,sizeof(before)));
 assert(lv_obj_get_scroll_bottom(body)<=1);
 lv_obj_scroll_to_y(body,0,LV_ANIM_OFF);
}
static void run(int width){
 ui_macros_show(send_macro);lv_obj_set_width(s_root,width);lv_obj_update_layout(s_root);cards(s_list);inside(s_list,s_root);assert(lv_obj_get_height(s_list)>100 && s_macros->rows[63]);if(ui_theme_is_studio()){assert(lv_obj_get_height(s_list)>=340);lv_obj_t *header=lv_obj_get_child(s_root,0);for(uint32_t i=0;i<lv_obj_get_child_count(header);i++){lv_obj_t *child=lv_obj_get_child(header,i);if(!lv_obj_has_flag(child,LV_OBJ_FLAG_HIDDEN))inside(child,header);}cards(header);}
 lv_obj_t *first=s_macros->rows[0];fav[63]=true;gen++;rebuild_macro_list();lv_obj_update_layout(s_root);cards(s_list);assert(first==s_macros->rows[0]);
 strcpy(s_macros->query,"nothing");rebuild_macro_list();lv_obj_update_layout(s_root);assert(s_macros->empty && !lv_obj_has_flag(s_macros->empty,LV_OBJ_FLAG_HIDDEN));inside(s_macros->empty,s_list);
 clear_search_cb(NULL);lv_obj_update_layout(s_root);
 lv_obj_t *header=lv_obj_get_child(s_root,0);
 lv_obj_t *actions=ui_theme_is_studio()?header:lv_obj_get_child(header,3);
 cards(actions);
 for(uint32_t i=0;i<lv_obj_get_child_count(actions);i++){
  lv_obj_t *button=lv_obj_get_child(actions,i);if(!lv_obj_check_type(button,&lv_button_class))continue;
  lv_obj_t *label=lv_obj_get_child(button,0);
  assert(lv_obj_get_height(label)<=lv_obj_get_style_text_font(label,0)->line_height);
 }
 search_cb(NULL);macro_popup_bounds(s_macros->editor,width);snapshot(s_macros->editor);
 lv_textarea_set_text(s_macros->editor_value,"MACRO_00");editor_done_cb(NULL);
 assert(!s_macros->editor && !strcmp(s_macros->query,"MACRO_00"));
 assert(!lv_obj_has_flag(s_macros->rows[0],LV_OBJ_FLAG_HIDDEN));
 clear_search_cb(NULL);lv_obj_update_layout(s_root);
 lv_obj_send_event(s_macros->rows[0],LV_EVENT_CLICKED,NULL);assert(s_confirm);
 macro_popup_bounds(s_confirm,width);
 for(size_t i=0;i<MACRO_PARAMETER_MAX+MACRO_PARAMETER_EXTRA;i++)assert(lv_obj_get_height(s_macros->parameter_fields[i])>=48);
 assert(lv_obj_get_height(lv_obj_get_child(s_confirm,0))==UI_FONT_TITLE->line_height);
 snapshot(s_confirm);
 lv_obj_send_event(s_macros->parameter_fields[0],LV_EVENT_CLICKED,NULL);
 assert(s_macros->editor);macro_popup_bounds(s_macros->editor,width);
 assert(lv_obj_get_height(s_macros->editor_value)>=48);
 lv_textarea_set_text(s_macros->editor_value,"205");editor_done_cb(NULL);
 assert(!strcmp(lv_textarea_get_text(s_macros->parameter_fields[0]),"205"));
 review_macro_cb(NULL);assert(s_confirm && strstr(s_macros->pending_command,"PARAM_0=205"));
 macro_popup_bounds(s_confirm,width);
 unsigned before=macro_sends;config_generation++;run_macro_cb(NULL);
 assert(macro_sends==before && s_confirm);close_confirm();
 lv_obj_send_event(s_macros->rows[0],LV_EVENT_CLICKED,NULL);assert(s_confirm);
 lv_textarea_set_text(s_macros->parameter_fields[0],"205");review_macro_cb(NULL);
 run_macro_cb(NULL);assert(macro_sends==before+1 && !s_confirm && strstr(sent_macro,"PARAM_0=205"));
 snapshot(s_root);ui_macros_hide();assert(!s_macros->editor && !s_confirm);
}
#endif
static uint16_t raster[1024*600];
static unsigned theme_case,density_case,large_case,width_case;
static void flush(lv_display_t *d,const lv_area_t *a,uint8_t *p){size_t width=(size_t)(a->x2-a->x1+1);for(int y=a->y1;y<=a->y2;y++)memcpy(raster+y*1024+a->x1,p+(size_t)(y-a->y1)*width*2,width*2);lv_display_flush_ready(d);}
static void snapshot(lv_obj_t *root){
 const char *folder=getenv("RESPONSIVE_SCREENSHOTS");if(!folder || custom_tokens || density_case!=1 || !large_case || width_case)return;
 lv_obj_invalidate(lv_screen_active());lv_refr_now(NULL);lv_area_t a;lv_obj_get_coords(root,&a);char path[512];
#if defined(TEST_TOOLS)
 const char *page="tools";
#elif defined(TEST_FILES)
 const char *page=root==s_print_confirm_popup?"files-confirm":root==s_file_detail_popup?"files-ready":"files";
#elif defined(TEST_NETWORK)
 const char *page="network";
#else
 const char *page=root==s_macros->editor?"macros-search":root==s_confirm?"macros-parameters":"macros";
#endif
 snprintf(path,sizeof(path),"%s/%s-theme%u.ppm",folder,page,theme_case);FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n%d %d\n255\n",a.x2-a.x1+1,a.y2-a.y1+1);for(int y=a.y1;y<=a.y2;y++)for(int x=a.x1;x<=a.x2;x++){uint16_t v=raster[y*1024+x];unsigned char c[3]={(unsigned char)(((v>>11)&31)*255/31),(unsigned char)(((v>>5)&63)*255/63),(unsigned char)((v&31)*255/31)};fwrite(c,1,3,f);}fclose(f);
}
int main(void){lv_init();lv_display_t *d=lv_display_create(1024,600);static uint8_t buffer[1024*50*2];lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);lv_display_set_buffers(d,buffer,NULL,sizeof(buffer),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(d,flush);
 for(unsigned custom=0;custom<2;custom++)for(unsigned theme=0;theme<5;theme++)for(unsigned density=0;density<3;density++)for(unsigned large=0;large<2;large++)for(unsigned w=0;w<3;w++){
  if(theme==UI_THEME_STUDIO_DARK&&(w||custom))continue;
  custom_tokens=custom;theme_case=theme;density_case=density;large_case=large;width_case=w;ui_theme_set_active((ui_theme_id_t)theme);ui_theme_set_density((ui_density_id_t)density);ui_theme_set_accessibility((ui_accessibility_t){.large_text=large});run((int[]){theme==UI_THEME_STUDIO_DARK?976:854,640,480}[w]);
 }
 lv_display_delete(d);lv_deinit();puts("PASS: responsive page bounds, wrapping, non-overlap and lifecycle across all five themes, three densities, both text sizes, three viewport widths and custom metric/profile overrides");}
