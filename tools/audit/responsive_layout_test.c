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
#include "keyboard_layout_checks.h"
bool custom_theme_color_override(uint32_t a,uint32_t b,uint32_t c,uint32_t *d){(void)a;(void)b;(void)c;(void)d;return false;}
static bool custom_tokens;
bool custom_theme_metric_override(int32_t a,int32_t b,int32_t c,int32_t *d){(void)a;(void)b;if(custom_tokens){*d=c+2;return true;}(void)d;return false;}
bool custom_theme_accent_override(uint8_t a,uint32_t *b){(void)a;(void)b;return false;}
bool custom_theme_surface_opacity(uint8_t *a){(void)a;return false;}
static bool connected=true,klipper_ready=true;static const char *test_printer_state="ready";
void moonraker_state_snapshot(moonraker_state_t *s){memset(s,0,sizeof(*s));s->moonraker_ok=connected;s->live_data_ok=klipper_ready;snprintf(s->printer_state,sizeof(s->printer_state),"%s",test_printer_state);}
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
static void tools_symbol_check(void){
 const uint32_t codepoints[]={0xF021,0xF03E,0xF0E7,0xF04B};
 for(unsigned i=0;i<4;i++){
  lv_obj_t *entry=lv_obj_get_child(s_tiles,i),*icon=lv_obj_get_child(entry,0);
  lv_font_glyph_dsc_t glyph;
  assert(lv_font_get_glyph_dsc(lv_obj_get_style_text_font(icon,0),&glyph,codepoints[i],0));
  assert(!glyph.is_placeholder && glyph.box_w>0 && glyph.box_h>0);
  assert(glyph.resolved_font && lv_font_get_line_height(glyph.resolved_font)>=28);
 }
}
static void tools_divider_check(void){
 if(!ui_theme_is_studio()){assert(!s_center_rule);return;}
 lv_area_t left,right,rule,viewport;
 lv_obj_get_coords(lv_obj_get_child(s_tiles,0),&left);lv_obj_get_coords(lv_obj_get_child(s_tiles,1),&right);
 bool columns=left.y1==right.y1 && left.x2<right.x1;
 assert(s_center_rule && lv_obj_has_flag(s_center_rule,LV_OBJ_FLAG_HIDDEN)!=columns);
 if(columns){
  lv_obj_get_coords(s_center_rule,&rule);lv_obj_get_content_coords(s_tiles,&viewport);
  assert(rule.x1>left.x2 && rule.x2<right.x1 && rule.x1==rule.x2);
  assert(rule.y1>=viewport.y1 && rule.y2<=viewport.y2);
  assert(!lv_obj_has_flag(s_center_rule,LV_OBJ_FLAG_CLICKABLE));
  assert(lv_color_eq(lv_obj_get_style_bg_color(s_center_rule,0),UI_BORDER));
  assert(lv_obj_get_style_bg_opa(s_center_rule,0)==LV_OPA_COVER);
  for(unsigned i=0;i<4;i++){
   lv_obj_t *entry=lv_obj_get_child(s_tiles,i);
   assert(lv_color_eq(lv_obj_get_style_border_color(entry,0),lv_obj_get_style_bg_color(s_center_rule,0)));
   assert(lv_obj_get_style_border_opa(entry,0)==LV_OPA_COVER);
  }
 }
}
static void run(int width){ui_tools_show();connected=false;tools_connection_refresh(NULL);assert(strstr(lv_label_get_text(s_body_labels[0]),"Offline"));lv_obj_update_layout(s_root);cards(s_tiles);connected=true;tools_connection_refresh(NULL);lv_obj_set_width(s_root,width);lv_obj_update_layout(s_root);if(!ui_theme_is_studio())cards(s_tiles);else {inside(s_tiles,s_root);cards(s_tiles);}assert(lv_obj_get_child_count(s_tiles)==4);tools_symbol_check();tools_divider_check();if(!ui_theme_is_studio()){inside(s_banner,s_root);for(unsigned i=0;i<lv_obj_get_child_count(s_banner);i++)inside(lv_obj_get_child(s_banner,i),s_banner);}snapshot(s_root);ui_tools_hide();assert(!s_root && !s_tiles && !s_center_rule);}
#elif defined(TEST_FILES)
#include "ui_page_state.c"
#include "ui_files.c"
static void files_review_layout(lv_obj_t *popup)
{
 lv_obj_update_layout(popup);
 lv_obj_t *title=lv_obj_get_child(popup,0),*body=lv_obj_get_child(popup,1),*footer=lv_obj_get_child(popup,-1);
 inside(title,popup);inside(body,popup);inside(footer,popup);
 assert(lv_obj_get_style_border_side(title,0)==LV_BORDER_SIDE_BOTTOM);
 assert(lv_obj_get_style_border_side(footer,0)==LV_BORDER_SIDE_TOP);
 assert(lv_obj_get_scrollbar_mode(body)==LV_SCROLLBAR_MODE_OFF);
 lv_area_t a,b,c;lv_obj_get_coords(title,&a);lv_obj_get_coords(body,&b);lv_obj_get_coords(footer,&c);assert(a.y2<b.y1&&b.y2<c.y1);
 if(popup==s_print_confirm_popup){
  lv_obj_t *printer=lv_obj_get_child(body,0),*file=lv_obj_get_child(body,1),*hint=lv_obj_get_child(body,2);
  assert(strstr(lv_label_get_text(printer),moonraker_config_active_profile_name()));
  assert(!strcmp(lv_label_get_text(file),s_detail_filename));
  assert(lv_obj_get_style_text_font(file,0)->line_height==UI_FONT_BODY_LARGE->line_height);
  for(unsigned i=0;i<3;i++){lv_obj_t *label=lv_obj_get_child(body,i);assert(lv_label_get_long_mode(label)==LV_LABEL_LONG_WRAP&&lv_obj_get_style_text_line_space(label,0)==4);}
  lv_obj_get_coords(footer,&a);lv_obj_scroll_to_y(body,LV_COORD_MAX,LV_ANIM_OFF);lv_obj_update_layout(popup);lv_obj_get_coords(footer,&b);assert(!memcmp(&a,&b,sizeof(a)));inside(hint,body);
  lv_obj_scroll_to_y(body,0,LV_ANIM_OFF);
 }else assert(lv_obj_get_style_text_line_space(s_studio_metadata_label,0)==4);
}
static void files_search_layout_check(void)
{
 lv_obj_t *popup=s_search_popup;assert(popup);lv_obj_update_layout(popup);
 inside(popup,lv_screen_active());
 lv_obj_t *title=lv_obj_get_child(popup,0),*body=lv_obj_get_child(popup,1),*keyboard=s_search_keyboard,*footer=lv_obj_get_child(popup,-1);
 inside(title,popup);inside(body,popup);inside(keyboard,popup);inside(footer,popup);
 assert(lv_obj_get_style_border_side(title,0)==LV_BORDER_SIDE_BOTTOM);
 assert(lv_obj_get_style_border_side(footer,0)==LV_BORDER_SIDE_TOP);
 inside(s_search_textarea,body);assert(lv_obj_get_height(s_search_textarea)>=56);
 keyboard_layout_check(popup,keyboard,s_search_textarea);
 lv_area_t a,b,c;lv_obj_get_coords(title,&a);lv_obj_get_coords(body,&b);lv_obj_get_coords(keyboard,&c);assert(a.y2<b.y1&&b.y2<c.y1);
 lv_obj_get_coords(footer,&a);assert(c.y2<a.y1);
 for(unsigned i=0;i<lv_obj_get_child_count(footer);i++){
  lv_obj_t *button=lv_obj_get_child(footer,i);inside(button,footer);inside(lv_obj_get_child(button,0),button);assert(lv_obj_get_height(button)>=48);
 }
}
static unsigned live_objects(lv_obj_t *p){unsigned n=1;for(uint32_t i=0;i<lv_obj_get_child_count(p);i++)n+=live_objects(lv_obj_get_child(p,i));return n;}
static unsigned live_timers(void){unsigned n=0;lv_timer_t*t=NULL;while((t=lv_timer_get_next(t)))n++;return n;}
char *thumbnail_session_selected_file(void){return "fixture.gcode";}
int moonraker_config_active_profile_index(void){return 0;}
const char *moonraker_config_active_profile_name(void){return "Workshop printer";}
static unsigned lightboxes,selections;
static bool files_inline_snapshot,files_long_review_capture;
static const char *files_state_capture;
void ui_preview_lightbox_show_file_object(lv_obj_t *i,const char *f){assert(i&&f[0]);lightboxes++;}
static void select_file(const char*p){assert(p&&p[0]);selections++;}
static unsigned starts,cancels,refreshes,up_calls;
static void up_fixture(void){up_calls++;}
static void refresh_files(void){refreshes++;}
static bool print_accept=true;
static unsigned start_attempts;
static void (*start_hook)(void);
static void start_job(void){start_attempts++;if(start_hook)start_hook();if(print_accept){starts++;print_confirm_cancel(NULL);}else ui_files_print_feedback("Moonraker rejected the start request. Check connection and retry.");}
static void cancel_job(void){cancels++;ui_files_close_detail_popup();}
static lv_point_t files_touch;
static bool files_touch_down;
static void files_read_touch(lv_indev_t *input,lv_indev_data_t *data){(void)input;data->point=files_touch;data->state=files_touch_down?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED;}
static void drag_metadata(lv_obj_t *body){
 lv_obj_update_layout(body);lv_obj_scroll_to_y(body,0,LV_ANIM_OFF);lv_obj_update_layout(body);lv_area_t area;lv_obj_get_coords(body,&area);int before=lv_obj_get_scroll_y(body);assert(lv_obj_get_scroll_bottom(body)>0);
 lv_indev_t*input=lv_indev_create();lv_indev_set_type(input,LV_INDEV_TYPE_POINTER);lv_indev_set_read_cb(input,files_read_touch);lv_indev_set_scroll_limit(input,2);lv_indev_set_scroll_throw(input,0);
 int travel=(area.y2-area.y1)/2;if(travel<8)travel=8;files_touch=(lv_point_t){(area.x1+area.x2)/2,area.y2-4};files_touch_down=true;lv_indev_read(input);
 for(int step=1;step<=6;step++){files_touch.y=area.y2-4-travel*step/6;lv_indev_read(input);}
 files_touch_down=false;lv_indev_read(input);lv_obj_update_layout(body);assert(lv_obj_get_scroll_y(body)>before);
 /* Retire a synthetic pointer's throw animation before freeing it. The panel
  * pointer is permanent; the fixture creates/deletes one per drag case. */
 lv_anim_delete(input,NULL);lv_indev_delete(input);
}
static void reenter_start(void){print_confirm_accept(NULL);detail_start_event_cb(NULL);}
static void retire_print(void){lv_obj_delete(s_file_detail_popup);}
static void retire_files_page(void){lv_obj_delete(s_printer_file_popup);}
static unsigned browser_searches,browser_sorts,browser_folders,browser_previews;
static void browser_click(lv_obj_t *object){lv_obj_send_event(object,LV_EVENT_CLICKED,NULL);}
static void nested_sort(void){browser_sorts++;browser_click(s_sort_button);ui_files_refresh();}
static void browser_search(const char *query){assert(!strcmp(query,"safe query"));browser_searches++;ui_files_hide();ui_files_show();assert(!strcmp(query,"safe query"));}
static void browser_folder(const char *path){browser_folders++;ui_files_clear_rows();assert(!strcmp(path,"safe/folder"));}
static void browser_preview(const char *path){browser_previews++;ui_files_clear_rows();assert(!strcmp(path,"safe.gcode"));}
static void browser_preview_reopen(const char *path){browser_previews++;ui_files_hide();ui_files_show();assert(!strcmp(path,"safe.gcode"));}
static void browser_select(const char *path){ui_files_clear_rows();assert(!strcmp(path,"safe.gcode"));selections++;}
static void replace_page_on_search_delete(lv_event_t *event){(void)event;ui_files_hide();ui_files_show();}
static void browser_safety_checks(void)
{
 ui_files_set_callbacks(NULL,browser_select,NULL);ui_files_set_browser_callbacks(browser_search,nested_sort,browser_folder,up_fixture);
 ui_files_show();unsigned sorted=browser_sorts,searches=browser_searches;
 lv_obj_t *fake=lv_button_create(s_printer_file_popup);
 lv_obj_add_event_cb(fake,search_button_cb,LV_EVENT_CLICKED,NULL);lv_obj_add_event_cb(fake,sort_button_cb,LV_EVENT_CLICKED,NULL);
 lv_obj_add_event_cb(fake,files_refresh_event_cb,LV_EVENT_CLICKED,NULL);browser_click(fake);
 assert(!s_search_popup&&browser_sorts==sorted&&!s_files_refresh_timer);lv_obj_delete(fake);
 browser_click(s_sort_button);assert(browser_sorts==sorted+1);
 lv_obj_add_state(s_sort_button,LV_STATE_DISABLED);browser_click(s_sort_button);assert(browser_sorts==sorted+1);
 lv_obj_remove_state(s_sort_button,LV_STATE_DISABLED);
 browser_click(s_search_button);lv_obj_t *keyboard=s_search_keyboard;lv_obj_delete(s_search_textarea);
 assert(!s_search_textarea&&!lv_keyboard_get_textarea(keyboard));browser_click(s_search_apply);assert(browser_searches==searches);close_search_popup();
 browser_click(s_search_button);lv_obj_delete(s_search_apply);assert(!s_search_apply);
 lv_obj_send_event(s_search_keyboard,LV_EVENT_READY,NULL);assert(browser_searches==searches);close_search_popup();
 browser_click(s_search_button);lv_textarea_set_text(s_search_textarea,"safe query");
 browser_click(s_search_apply);assert(browser_searches==searches+1&&s_printer_file_popup&&!s_search_popup);
 browser_click(s_search_button);lv_textarea_set_text(s_search_textarea,"safe query");
 lv_obj_add_event_cb(s_search_popup,replace_page_on_search_delete,LV_EVENT_DELETE,NULL);
 browser_click(s_search_apply);assert(browser_searches==searches+1&&s_printer_file_popup&&!s_search_popup);
 ui_files_add_folder_button("Folder","safe/folder",0);unsigned folders=browser_folders;
 browser_click(lv_obj_get_child(s_printer_file_list,0));assert(browser_folders==folders+1&&!lv_obj_get_child_count(s_printer_file_list));
 ui_files_add_file_entry("safe.gcode",1,0,0);unsigned selected=selections;
 browser_click(s_file_rows->button);assert(selections==selected+1&&!s_file_rows);
 ui_files_add_file_entry("safe.gcode",1,0,500);ui_files_add_file_entry("safe.gcode",1,0,620);
 lv_obj_update_layout(s_printer_file_popup);lv_obj_scroll_to_y(s_printer_file_list,500,LV_ANIM_OFF);
 ui_files_set_callbacks(NULL,browser_select,browser_preview);unsigned previews=browser_previews;
 request_visible_file_previews();assert(browser_previews==previews+1&&!s_file_rows);
 ui_files_set_callbacks(NULL,browser_select,NULL);
 ui_files_add_file_entry("safe.gcode",1,0,0);lv_obj_update_layout(s_printer_file_popup);
 ui_files_set_callbacks(NULL,browser_select,browser_preview_reopen);previews=browser_previews;
 request_visible_file_previews();assert(browser_previews==previews+1&&s_printer_file_popup&&!s_file_rows);
 ui_files_set_callbacks(NULL,browser_select,NULL);ui_files_add_file_entry("safe.gcode",1,0,0);
 file_row_context_t *row=s_file_rows;lv_obj_delete(row->preview_frame);
 assert(!row->preview_frame&&!row->file_icon&&!row->preview_image);
 lv_image_dsc_t unused={0};ui_files_set_file_thumbnail("safe.gcode",&unused);ui_files_clear_rows();
 ui_files_add_folder_button("Folder","safe/folder",0);lv_obj_t *folder=lv_obj_get_child(s_printer_file_list,0);
 lv_obj_add_flag(lv_obj_get_child(folder,0),LV_OBJ_FLAG_EVENT_BUBBLE);lv_obj_delete(folder);
 ui_files_add_file_entry("safe.gcode",1,0,0);lv_obj_delete(s_printer_file_list);assert(!s_printer_file_list&&!s_file_rows);
 request_visible_file_previews();
 lv_obj_delete(s_breadcrumb_label);lv_obj_delete(s_sort_label);lv_obj_delete(s_search_label);
 ui_files_set_breadcrumb("test");ui_files_set_sort_text("Newest");ui_files_set_search_text("safe query");
 lv_obj_delete(s_search_button);lv_obj_delete(s_sort_button);lv_obj_delete(s_refresh_button);
 assert(!s_search_button&&!s_sort_button&&!s_refresh_button);
 lv_obj_delete(s_files_state->root);assert(!s_files_state);
 ui_files_set_status("Loading files...");ui_files_set_status("Moonraker offline");ui_files_set_status("");
 ui_files_hide();assert(!s_file_rows&&!s_search_popup);
 ui_files_set_browser_callbacks(NULL,NULL,NULL,up_fixture);
}
static unsigned files_root_draws, files_viewport_draws, files_list_draws;
static void files_count_draw(lv_event_t *event)
{
 unsigned *count=lv_event_get_user_data(event); ++*count;
}
static void files_scroll_damage_check(void)
{
 char *display=file_detail_display_text("File:\njob.gcode\n\nETA: 1:20\nThumbnail: metadata found\n.thumbs/job.png\nMaterial: PETG\nLayers: 300\n\nConfirm to start this print.");
 assert(display && !strcmp(display,"ETA: 1:20\nMaterial: PETG\nLayers: 300"));free(display);
 ui_files_show();ui_files_set_callbacks(NULL,select_file,NULL);
 for(unsigned i=0;i<20;++i){char path[80];snprintf(path,sizeof(path),"folder/scroll_job_%02u.gcode",i);ui_files_add_file_entry(path,1048576,0,(int)i*118);}
 lv_obj_update_layout(s_printer_file_popup);
 /* All sizes and unknown metadata remain concise on the native list. */
 const double sizes[]={256,16384,1048576,3221225472.0,0};
 const char *captions[]={"256 B","16.0 KiB","1.0 MiB","3.0 GiB","G-code file"};
 for(unsigned i=0;i<5;i++){
  ui_files_add_file_entry("format.gcode",sizes[i],0,2400+(int)i*118);
  lv_obj_update_layout(s_printer_file_popup);
  assert(!strcmp(lv_label_get_text(lv_obj_get_child(s_file_rows->button,2)),captions[i]));
 }
 lv_obj_t *viewport=lv_obj_get_parent(s_printer_file_list);
 lv_area_t breadcrumb_before,breadcrumb_after;
 lv_obj_get_coords(s_breadcrumb_label,&breadcrumb_before);
 lv_obj_add_event_cb(s_printer_file_popup,files_count_draw,LV_EVENT_DRAW_MAIN,&files_root_draws);
 lv_obj_add_event_cb(viewport,files_count_draw,LV_EVENT_DRAW_MAIN,&files_viewport_draws);
 lv_obj_add_event_cb(s_printer_file_list,files_count_draw,LV_EVENT_DRAW_MAIN,&files_list_draws);
 lv_refr_now(NULL);files_root_draws=files_viewport_draws=files_list_draws=0;
 for(unsigned i=1;i<=6;++i){lv_obj_scroll_to_y(s_printer_file_list,(int)i*18,LV_ANIM_OFF);lv_refr_now(NULL);}
 assert(files_list_draws>0 && lv_obj_get_scroll_y(s_printer_file_list)>0);
 lv_obj_get_coords(s_breadcrumb_label,&breadcrumb_after);assert(!memcmp(&breadcrumb_before,&breadcrumb_after,sizeof(breadcrumb_before)));
 if(getenv("FILES_SCROLL_DIAGNOSTIC"))printf("Files theme=%u root_draws=%u viewport_draws=%u list_draws=%u per six scroll steps\n",(unsigned)ui_theme_get_active(),files_root_draws,files_viewport_draws,files_list_draws);
 if(!getenv("FILES_SCROLL_BASELINE")){assert(!files_root_draws && !files_viewport_draws);assert(lv_obj_get_style_bg_opa(s_printer_file_list,0)==LV_OPA_COVER);}
 /* Physical drag must move the list without selecting a file. */
 lv_obj_scroll_to_y(s_printer_file_list,0,LV_ANIM_OFF);lv_obj_update_layout(s_printer_file_popup);
 unsigned selected=selections;drag_metadata(s_printer_file_list);assert(selected==selections);
 ui_files_hide();assert(!s_file_rows && !s_printer_file_list && !s_files_refresh_timer);
}
static void run(int width){
 ui_files_show();ui_files_set_browser_callbacks(NULL,NULL,NULL,up_fixture);ui_files_set_breadcrumb(NULL);ui_files_set_search_text("");assert(s_up_button&&lv_obj_has_state(s_up_button,LV_STATE_DISABLED));unsigned before_up=up_calls;lv_obj_send_event(s_up_button,LV_EVENT_CLICKED,NULL);assert(up_calls==before_up);
 ui_files_set_breadcrumb("parts/subfolder");assert(!lv_obj_has_state(s_up_button,LV_STATE_DISABLED));lv_obj_send_event(s_up_button,LV_EVENT_CLICKED,NULL);assert(up_calls==before_up+1);ui_files_set_breadcrumb(NULL);ui_files_set_search_text("needle");assert(!lv_obj_has_state(s_up_button,LV_STATE_DISABLED));
 assert(lv_obj_has_state(s_search_button,LV_STATE_CHECKED));
 assert(!strcmp(lv_label_get_text(s_search_label),"Search"));
 assert(!strcmp(lv_label_get_text(s_breadcrumb_label),"Search / needle"));
 lv_obj_update_layout(s_printer_file_popup);
 browser_click(s_search_button);assert(s_search_popup && lv_obj_has_state(s_search_button,LV_STATE_CHECKED));
 lv_textarea_set_text(s_search_textarea,"unapplied query");browser_click(s_search_cancel);
 assert(!s_search_popup && lv_obj_has_state(s_search_button,LV_STATE_CHECKED));
 assert(!strcmp(s_search_text,"needle") && !strcmp(lv_label_get_text(s_breadcrumb_label),"Search / needle"));
 ui_files_set_breadcrumb("parts/subfolder");assert(!strcmp(lv_label_get_text(s_breadcrumb_label),"Search / needle"));
 ui_files_set_search_text("");assert(!lv_obj_has_state(s_search_button,LV_STATE_CHECKED));
 assert(!strcmp(lv_label_get_text(s_breadcrumb_label),"G-code files / parts/subfolder"));
 ui_files_set_breadcrumb(NULL);assert(!strcmp(lv_label_get_text(s_breadcrumb_label),"G-code files"));
 assert(lv_obj_has_state(s_up_button,LV_STATE_DISABLED));
 ui_files_set_sort_text("NAME");assert(!strcmp(lv_label_get_text(s_sort_label),"Name"));
 ui_files_set_sort_text("SIZE");assert(!strcmp(lv_label_get_text(s_sort_label),"Size"));
 ui_files_set_sort_text("NEWEST");assert(!strcmp(lv_label_get_text(s_sort_label),"Newest"));
 ui_files_set_sort_text("Custom order");assert(!strcmp(lv_label_get_text(s_sort_label),"Custom order"));
 lv_obj_set_width(s_printer_file_popup,width);ui_files_set_search_text("very long query");ui_files_set_sort_text("NEWEST");lv_obj_update_layout(s_printer_file_popup);
 if(!ui_theme_is_studio()){lv_obj_t *toolbar=lv_obj_get_child(s_printer_file_popup,1);cards(toolbar);assert(lv_obj_get_child_count(toolbar)==4);inside(toolbar,s_printer_file_popup);}inside(s_breadcrumb_label,s_printer_file_popup);
 lv_obj_t *viewport=lv_obj_get_parent(s_printer_file_list);inside(viewport,s_printer_file_popup);assert(lv_obj_get_height(viewport)>100);inside(s_printer_file_list,viewport);
 const double file_sizes[]={256,16384,1048576,3221225472.0,0};
 for(int i=0;i<5;i++)ui_files_add_file_entry("folder/Very_long_operator_job_name_for_responsive_row_checks.gcode",file_sizes[i],i%2?0:1700000000,i*118);
 lv_obj_update_layout(s_printer_file_popup);
 for(file_row_context_t *entry=s_file_rows;entry;entry=entry->next){
  lv_obj_t *name=lv_obj_get_child(entry->button,1),*metadata=lv_obj_get_child(entry->button,2);
  lv_area_t n,m;lv_obj_get_coords(name,&n);lv_obj_get_coords(metadata,&m);assert(n.y2<m.y1);
  const char *text=lv_label_get_text(metadata);assert(!strstr(text,"HOLD FOR DETAILS")&&!strstr(text,"DATE --"));
  assert(lv_font_get_line_height(lv_obj_get_style_text_font(metadata,0))>=lv_font_get_line_height(UI_FONT_CAPTION));
 }
 ui_files_set_callbacks(NULL,select_file,NULL);file_row_context_t*row=s_file_rows;assert(row);unsigned selected_before=selections,preview_before=lightboxes;lv_obj_add_flag(row->preview_frame,LV_OBJ_FLAG_EVENT_BUBBLE);lv_obj_send_event(row->preview_frame,LV_EVENT_CLICKED,NULL);assert(selections==selected_before+1&&lightboxes==preview_before);row->preview_image=lv_image_create(row->preview_frame);lv_obj_send_event(row->preview_frame,LV_EVENT_LONG_PRESSED,NULL);assert(selections==selected_before+1);lv_obj_send_event(row->preview_frame,LV_EVENT_CLICKED,NULL);assert(selections==selected_before+1&&lightboxes==preview_before+1);
 ui_files_add_folder_button("Long folder title for row checks","folder",590);lv_obj_update_layout(s_printer_file_popup);cards(s_printer_file_list);
 ui_files_set_status("Moonraker offline. Check the active printer.");lv_obj_update_layout(s_printer_file_popup);inside(s_files_state->root,viewport);inside(s_files_state->title,s_files_state->root);inside(s_files_state->detail,s_files_state->root);
 lv_area_t title,description;lv_obj_get_coords(s_files_state->title,&title);lv_obj_get_coords(s_files_state->detail,&description);assert(title.y2<description.y1);
 const char *messages[]={"Loading files...","No files match the current search","No files found","Moonraker offline","Request failed"};
 const char *titles[]={"Loading files","No search results","No files found","Files offline","Files unavailable"};
 const char *states[]={"files-loading","files-search-empty","files-empty","files-offline","files-error"};
 for(unsigned state=0;state<5;state++){ui_files_set_status(messages[state]);lv_obj_update_layout(s_printer_file_popup);assert(!strcmp(lv_label_get_text(s_files_state->title),titles[state]));inside(s_files_state->title,s_files_state->root);inside(s_files_state->detail,s_files_state->root);files_state_capture=states[state];snapshot(s_printer_file_popup);}
 files_state_capture=NULL;assert(lv_obj_get_scrollbar_mode(s_printer_file_list)==LV_SCROLLBAR_MODE_OFF);
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
 files_review_layout(confirm);cards(lv_obj_get_child(confirm,-1));snapshot(confirm);
 print_confirm_cancel(NULL);assert(!s_print_confirm_popup && ui_files_detail_is_open() && starts==prior);
 detail_start_event_cb(NULL);print_accept=false;print_confirm_accept(NULL);assert(s_print_confirm_popup&&starts==prior);
 print_accept=true;unsigned attempts=start_attempts;start_hook=reenter_start;print_confirm_accept(NULL);start_hook=NULL;assert(start_attempts==attempts+1&&!s_print_confirm_popup && starts==prior+1);
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
 if(bottom>0){lv_area_t held;lv_obj_get_coords(s_detail_start_button,&held);drag_metadata(metadata);lv_obj_get_coords(s_detail_start_button,&action_after);assert(!memcmp(&held,&action_after,sizeof(held)));}
 if(bottom>0)lv_obj_scroll_to_y(metadata,lv_obj_get_scroll_y(metadata)+bottom,LV_ANIM_OFF);
 lv_obj_update_layout(s_file_detail_popup);
 assert(lv_obj_get_scroll_bottom(metadata)<=1);assert(lv_obj_get_scrollbar_mode(metadata)==LV_SCROLLBAR_MODE_OFF);
 lv_area_t text_area,view_area;lv_obj_get_coords(s_detail_info_label,&text_area);lv_obj_get_coords(metadata,&view_area);assert(text_area.y2<=view_area.y2);
 lv_obj_get_coords(s_detail_start_button,&action_after);assert(!memcmp(&action_before,&action_after,sizeof(action_before)));
 assert(!strstr(lv_label_get_text(s_detail_info_label),"Confirm to start this print."));
 assert(!strstr(lv_label_get_text(s_detail_info_label),"Thumbnail:")&&!strstr(lv_label_get_text(s_detail_info_label),"thumbs/"));
 if(ui_theme_is_studio()) {
  assert(lv_obj_get_width(box)==352 && lv_obj_get_height(box)>=128);
  ui_files_update_detail_metadata("ETA: 2:40\nFilament: 20 m\nLayer Height: 0.20 mm\nObject Height: 64.0 mm\nSize: 4.20 MB",true);
  lv_obj_update_layout(s_file_detail_popup);
  assert(lv_obj_get_scroll_bottom(metadata)<=0 && lv_obj_has_flag(s_detail_more_button,LV_OBJ_FLAG_HIDDEN));
  assert(lv_obj_get_height(box)>176);
  lv_area_t preview_area,filename_area,metadata_area;
  lv_obj_get_coords(box,&preview_area);lv_obj_get_coords(s_detail_filename_label,&filename_area);lv_obj_get_coords(metadata,&metadata_area);
  assert(preview_area.y2<filename_area.y1 && filename_area.y2<metadata_area.y1);
  assert(metadata_area.y2+16<action_before.y1+1);
  assert(lv_obj_get_width(s_detail_start_button)==168 && lv_obj_get_width(s_detail_cancel_button)==168);
  studio_metadata_open(NULL);assert(!s_studio_metadata_popup);
  files_inline_snapshot=true;snapshot(s_file_detail_popup);files_inline_snapshot=false;
  int32_t standard_preview_height=lv_obj_get_height(box);
  if(getenv("RESPONSIVE_SCREENSHOTS"))printf("Studio density=%u large=%u preview_height=%ld metadata_height=%ld\n",(unsigned)ui_theme_get_density(),(unsigned)ui_theme_get_accessibility().large_text,(long)standard_preview_height,(long)lv_obj_get_height(metadata));
  static uint16_t preview_pixels[64*128];
  const lv_image_dsc_t preview_image={.header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_RGB565,.w=64,.h=128,.stride=128},.data_size=sizeof(preview_pixels),.data=(const uint8_t*)preview_pixels};
  lv_obj_t *resized_image=lv_image_create(box);lv_image_set_src(resized_image,&preview_image);
  lv_obj_send_event(box,LV_EVENT_SIZE_CHANGED,NULL);
  assert(lv_image_get_transformed_height(resized_image)<=lv_obj_get_height(box));
  char long_details[3000];size_t used=0;for(unsigned line=0;line<40;line++)used+=(size_t)snprintf(long_details+used,sizeof(long_details)-used,"Field %02u: complete metadata values for this print file\n",line);
  ui_files_update_detail_metadata(long_details,true);
  lv_obj_update_layout(s_file_detail_popup);
  assert(!lv_obj_has_flag(s_detail_more_button,LV_OBJ_FLAG_HIDDEN) && lv_obj_get_width(s_detail_start_button)==128);
  assert(lv_obj_get_height(box)==128 && lv_image_get_transformed_height(resized_image)<=128);
  studio_metadata_open(NULL);assert(!ui_files_can_refresh_rows());assert(s_studio_metadata_popup && s_studio_metadata_label);
  files_review_layout(s_studio_metadata_popup);snapshot(s_studio_metadata_popup);
  for(uint32_t i=0;i<lv_obj_get_child_count(s_studio_metadata_popup);i++)inside(lv_obj_get_child(s_studio_metadata_popup,i),s_studio_metadata_popup);
  lv_obj_t*expanded_body=lv_obj_get_child(s_studio_metadata_popup,1);assert(lv_obj_has_flag(expanded_body,LV_OBJ_FLAG_CLICKABLE));
  assert(lv_obj_get_scroll_bottom(expanded_body)>0);lv_obj_t*fixed_footer=lv_obj_get_child(s_studio_metadata_popup,-1);lv_area_t fixed_before,fixed_after;lv_obj_get_coords(fixed_footer,&fixed_before);drag_metadata(expanded_body);lv_obj_get_coords(fixed_footer,&fixed_after);assert(!memcmp(&fixed_before,&fixed_after,sizeof(fixed_before)));
  lv_obj_scroll_to_y(expanded_body,lv_obj_get_scroll_y(expanded_body)+lv_obj_get_scroll_bottom(expanded_body),LV_ANIM_OFF);lv_obj_update_layout(s_studio_metadata_popup);lv_area_t full_text,scroll_window;lv_obj_get_coords(s_studio_metadata_label,&full_text);lv_obj_get_coords(expanded_body,&scroll_window);assert(full_text.y2<=scroll_window.y2&&strstr(lv_label_get_text(s_studio_metadata_label),"Field 39:"));
  lv_obj_t *footer=lv_obj_get_child(s_studio_metadata_popup,-1);
  lv_obj_t *close=lv_obj_get_child(footer,0);
  assert(lv_obj_get_width(close)==128 && lv_obj_get_height(close)==48);inside(close,footer);
  lv_area_t ca,fa;lv_obj_get_coords(close,&ca);lv_obj_get_coords(footer,&fa);assert(ca.x2==fa.x2);
  lv_obj_t *close_label=lv_obj_get_child(close,0);
  assert(!strcmp(lv_label_get_text(close_label),"Close"));
  assert(lv_obj_get_width(close_label)>0 && lv_obj_get_width(close_label)<lv_obj_get_width(close));
  inside(close_label,close);
  ui_files_update_detail_metadata("Updated metadata",true);assert(!strcmp(lv_label_get_text(s_studio_metadata_label),"Updated metadata"));
  lv_obj_update_layout(s_file_detail_popup);
  assert(lv_obj_get_height(box)>standard_preview_height);
  assert(lv_image_get_transformed_height(resized_image)<=lv_obj_get_height(box));
  lv_obj_delete(resized_image);
  lv_obj_delete(s_studio_metadata_popup);assert(!s_studio_metadata_popup&&!s_studio_metadata_label);ui_files_update_detail_metadata("Updated after deletion",true);assert(lv_obj_has_flag(s_detail_more_button,LV_OBJ_FLAG_HIDDEN));studio_metadata_open(NULL);assert(!s_studio_metadata_popup);studio_metadata_close(NULL);assert(!s_studio_metadata_popup && !s_studio_metadata_label);
 } else {
  lv_obj_t *footer=lv_obj_get_parent(s_detail_start_button);cards(footer);inside(footer,s_file_detail_popup);assert(lv_obj_get_style_border_side(footer,0)==LV_BORDER_SIDE_TOP);
  assert(lv_label_get_long_mode(lv_obj_get_child(s_file_detail_popup,1))==LV_LABEL_LONG_SCROLL_CIRCULAR);
 }
 connected=false;detail_connection_refresh(NULL);assert(lv_obj_has_state(s_detail_start_button,LV_STATE_DISABLED)&&lv_obj_has_flag(s_detail_connection_label,LV_OBJ_FLAG_HIDDEN)&&!lv_label_get_text(s_detail_connection_label)[0]);
 assert(lv_obj_has_flag(s_detail_connection_label,LV_OBJ_FLAG_HIDDEN));
 connected=true;klipper_ready=false;detail_connection_refresh(NULL);assert(lv_obj_has_state(s_detail_start_button,LV_STATE_DISABLED));
 klipper_ready=true;test_printer_state="printing";detail_connection_refresh(NULL);assert(lv_obj_has_state(s_detail_start_button,LV_STATE_DISABLED));
 test_printer_state="ready";detail_connection_refresh(NULL);assert(!lv_obj_has_state(s_detail_start_button,LV_STATE_DISABLED));
 ui_files_update_detail_metadata("Metadata unavailable",false);assert(lv_obj_has_state(s_detail_start_button,LV_STATE_DISABLED));
 detail_start_event_cb(NULL);assert(starts==prior+1);
 snapshot(s_file_detail_popup);
 prior=cancels;detail_cancel_event_cb(NULL);assert(cancels==prior+1 && !s_file_detail_popup && !s_detail_start_button && !s_print_confirm_popup);
 ui_files_show_detail_popup("Another_file.gcode","Ready",NULL,NULL,cancel_job,start_job);ui_files_update_detail_metadata("Ready",true);
 detail_start_event_cb(NULL);assert(s_print_confirm_popup);prior=starts;
 connected=false;detail_connection_refresh(NULL);assert(lv_obj_has_state(s_print_accept_button,LV_STATE_DISABLED)&&strstr(lv_label_get_text(s_print_connection_label),"Offline"));print_confirm_accept(NULL);assert(starts==prior);
 connected=true;detail_connection_refresh(NULL);assert(!lv_obj_has_state(s_print_accept_button,LV_STATE_DISABLED));config_generation++;
 print_confirm_accept(NULL);assert(starts==prior && s_print_confirm_popup && s_file_detail_popup);
 ui_files_update_detail_metadata("Stale data",true);assert(strcmp(lv_label_get_text(s_detail_info_label),"Stale data"));
 ui_files_close_detail_popup();
 ui_files_show_detail_popup("Final_file.gcode","Ready",NULL,NULL,cancel_job,start_job);ui_files_update_detail_metadata("Ready",true);
 detail_start_event_cb(NULL);assert(s_print_confirm_popup);ui_files_close_detail_popup();assert(!s_print_confirm_popup && !s_detail_filename);
 ui_files_show_detail_popup("External_delete.gcode","Ready",NULL,NULL,cancel_job,start_job);ui_files_update_detail_metadata("Ready",true);detail_start_event_cb(NULL);assert(s_print_confirm_popup);lv_obj_delete(s_file_detail_popup);assert(!s_print_confirm_popup && !s_detail_connection_timer && !s_detail_filename);

 for(unsigned i=0;i<5;i++){
  ui_files_show_detail_popup("retire-during-print.gcode","Ready",NULL,NULL,cancel_job,start_job);ui_files_update_detail_metadata("Ready",true);detail_start_event_cb(NULL);attempts=start_attempts;start_hook=retire_print;print_confirm_accept(NULL);start_hook=NULL;assert(start_attempts==attempts+1&&!s_file_detail_popup&&!s_print_confirm_popup&&!s_detail_start_cb&&!s_detail_connection_timer);detail_connection_refresh(NULL);print_confirm_accept(NULL);detail_start_event_cb(NULL);
 }
 lv_obj_t *event_source=s_search_button;lv_obj_send_event(event_source,LV_EVENT_CLICKED,NULL);assert(!s_search_popup); /* Old printer page cannot launch an editor. */
 ui_files_hide();ui_files_show();event_source=s_search_button;lv_obj_send_event(event_source,LV_EVENT_CLICKED,NULL);
 assert(s_search_popup);files_search_layout_check();snapshot(s_search_popup);
 lv_textarea_set_text(s_search_textarea,"test query");lv_obj_t*search_owner=s_search_popup;lv_obj_send_event(event_source,LV_EVENT_CLICKED,NULL);assert(s_search_popup==search_owner&&!strcmp(lv_textarea_get_text(s_search_textarea),"test query"));lv_obj_send_event(lv_obj_get_child(s_search_popup,2),LV_EVENT_CANCEL,NULL);lv_timer_handler();assert(!s_search_popup);
 lv_obj_send_event(event_source,LV_EVENT_CLICKED,NULL);lv_obj_send_event(lv_obj_get_child(s_search_popup,2),LV_EVENT_READY,NULL);lv_timer_handler();assert(!s_search_popup);
 char very_long_name[187];memset(very_long_name,'W',180);memcpy(very_long_name+180,".gcode",7);
 ui_files_show_detail_popup(very_long_name,"Ready",NULL,NULL,cancel_job,start_job);ui_files_update_detail_metadata("Ready",true);
 attempts=start_attempts;detail_start_event_cb(NULL);assert(s_print_confirm_popup);
 lv_obj_set_width(s_print_confirm_popup,width<640?width:640);files_review_layout(s_print_confirm_popup);
 files_long_review_capture=true;snapshot(s_print_confirm_popup);files_long_review_capture=false;
 lv_obj_t *review_footer=lv_obj_get_child(s_print_confirm_popup,-1);lv_obj_send_event(lv_obj_get_child(review_footer,0),LV_EVENT_CLICKED,NULL);
 assert(!s_print_confirm_popup&&s_file_detail_popup&&start_attempts==attempts);ui_files_close_detail_popup();
 snapshot(s_printer_file_popup);ui_files_hide();assert(!s_file_detail_popup && !s_studio_metadata_popup);assert(!s_printer_file_popup);
 unsigned objects_before=live_objects(lv_screen_active())+live_objects(lv_layer_top()),timers_before=live_timers();
 for(unsigned repeat=0;repeat<4;repeat++){ui_files_show();ui_files_add_file_entry("cycle.gcode",100,0,0);ui_files_hide();assert(!s_file_rows && !s_files_refresh_timer && !s_files_refresh_pending);assert(live_objects(lv_screen_active())+live_objects(lv_layer_top())==objects_before && live_timers()==timers_before);}
 for(unsigned repeat=0;repeat<5;repeat++){
  ui_files_show();ui_files_add_file_entry("external-root.gcode",100,0,0);ui_files_show_detail_popup("external-root.gcode","Ready",NULL,NULL,cancel_job,start_job);ui_files_update_detail_metadata("Ready",true);detail_start_event_cb(NULL);if(ui_theme_is_studio())studio_metadata_open(NULL);
  attempts=start_attempts;start_hook=retire_files_page;print_confirm_accept(NULL);start_hook=NULL;assert(start_attempts==attempts+1&&!s_printer_file_popup&&!s_file_detail_popup&&!s_print_confirm_popup&&!s_studio_metadata_popup&&!s_studio_metadata_label&&!s_files_refresh_timer&&!s_detail_connection_timer&&!s_file_rows);
  unsigned before_refresh=refreshes;ui_files_refresh();files_refresh_deferred_cb(NULL);apply_search_and_close(false);search_button_cb(NULL);search_apply_button_cb(NULL);search_cancel_button_cb(NULL);sort_button_cb(NULL);up_button_cb(NULL);print_confirm_accept(NULL);detail_connection_refresh(NULL);ui_files_update_detail_metadata("late result",true);assert(refreshes==before_refresh);
  assert(live_objects(lv_screen_active())+live_objects(lv_layer_top())==objects_before&&live_timers()==timers_before);
 }
 browser_safety_checks();
}
#elif defined(TEST_NETWORK)
#include "ui_network.c"
void ui_network_create(void){}
void ui_network_destroy(void){ui_network_destroy_objects(NULL,NULL,NULL);}
static unsigned scans,profiles;
static bool network_empty_capture;
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
 } else {
  for(unsigned i=0;i<lv_obj_get_child_count(s_network.banner);i++){
   lv_obj_t *child=lv_obj_get_child(s_network.banner,i);
   if(!lv_obj_has_flag(child,LV_OBJ_FLAG_HIDDEN))inside(child,s_network.banner);
  }
  lv_obj_t *state=ui_status_banner_state_label(s_network.banner);
  const lv_font_t *font=lv_obj_get_style_text_font(state,0);lv_point_t text_size;
  lv_text_get_size(&text_size,lv_label_get_text(state),font,0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
  assert(lv_obj_get_height(state)==font->line_height && text_size.x<=lv_obj_get_width(state));
  lv_obj_t *message=lv_obj_get_child(s_network.banner,2);
  assert(lv_obj_get_width(message)>400 && !strcmp(lv_label_get_text(message),"Printer connected"));
 }
 unsigned prior=scans;lv_obj_send_event(lv_obj_get_child(s_network.actions_card,ui_theme_is_studio()?0:1),LV_EVENT_CLICKED,NULL);assert(scans==prior+1);
 prior=profiles;lv_obj_send_event(lv_obj_get_child(s_network.actions_card,ui_theme_is_studio()?1:2),LV_EVENT_CLICKED,NULL);assert(profiles==prior+1);
 wifi_ap_record_t aps[6]={0};for(unsigned i=0;i<6;i++){snprintf((char *)aps[i].ssid,sizeof(aps[i].ssid),"A_long_access_point_name_%u",i);aps[i].rssi=-57-(int)i;}
 ui_network_render_scan_results(aps,6,8,selected_cb);lv_obj_update_layout(s_network.root);
 assert(lv_obj_get_child_count(s_network.networks_list)==6);
 cards(s_network.networks_list);
 lv_obj_send_event(lv_obj_get_child(s_network.networks_list,0),LV_EVENT_CLICKED,NULL);assert(!strcmp(picked,(char *)aps[0].ssid));
 snapshot(s_network.root);
 ui_network_render_scan_results(NULL,0,0,selected_cb);assert(lv_obj_get_child_count(s_network.networks_list)==1);
 assert(lv_obj_check_type(lv_obj_get_child(s_network.networks_list,0),&lv_label_class));assert(lv_obj_get_scrollbar_mode(s_network.networks_list)==LV_SCROLLBAR_MODE_OFF);
 ui_network_refresh_objects(NULL,NULL,NULL,false,NULL,0,"No networks found");assert(!strcmp(lv_label_get_text(s_network.wifi_state),"Offline")&&!strcmp(lv_label_get_text(s_network.moonraker_state),"Disconnected"));
 network_empty_capture=true;snapshot(s_network.root);network_empty_capture=false;
 ui_network_hide();assert(!s_network.root && !s_network.networks_list);
}

#else
static uint32_t gen=1;static bool fav[64];static char selected[64];
static unsigned macro_sends;
static bool macro_accept=true;
static void (*macro_send_hook)(void);
static char sent_macro[MACRO_COMMAND_MAX];
static bool send_macro(const char *command){macro_sends++;snprintf(sent_macro,sizeof(sent_macro),"%s",command);if(macro_send_hook)macro_send_hook();return macro_accept;}
void macro_controller_status(macro_controller_status_t *s){memset(s,0,sizeof(*s));s->discovered=true;s->count=s->total_count=64;s->generation=gen;}
bool macro_controller_get(size_t i,char *o,size_t n){if(i>=64)return false;snprintf(o,n,"MACRO_%02u_WITH_A_VERY_LONG_OPERATOR_ACTION_NAME",(unsigned)i);return true;}
bool macro_controller_is_favorite(const char *n){unsigned i=0;sscanf(n,"MACRO_%u",&i);return i<64 && fav[i];}
bool macro_controller_toggle_favorite(const char *n){unsigned i=0;sscanf(n,"MACRO_%u",&i);if(i>=64)return false;fav[i]=!fav[i];gen++;return true;}
bool macro_controller_parameters(const char *n,macro_parameter_catalog_t *p){snprintf(selected,sizeof(selected),"%s",n);memset(p,0,sizeof(*p));p->count=8;for(int i=0;i<8;i++)snprintf(p->names[i],sizeof(p->names[i]),"PARAM_%d",i);return true;}

#include "ui_macros.c"
static unsigned macro_timer_count(void){unsigned count=0;for(lv_timer_t*t=lv_timer_get_next(NULL);t;t=lv_timer_get_next(t))count++;return count;}
static void macro_reenter(void){run_macro_cb(NULL);review_macro_cb(NULL);search_cb(NULL);ui_macros_show(send_macro);}
static void macro_retire(void){lv_obj_delete(s_confirm);}
static void macro_retire_root(void){lv_obj_delete(s_root);}
static void macro_switch(void){config_generation++;}
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
static lv_point_t macro_touch;
static bool macro_touch_down;
static void macro_read_touch(lv_indev_t *input,lv_indev_data_t *data){
 (void)input;data->point=macro_touch;data->state=macro_touch_down?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED;
}
static void macro_drag_list(void){
 assert(lv_obj_get_scrollbar_mode(s_list)==LV_SCROLLBAR_MODE_OFF);
 assert(lv_obj_get_scroll_bottom(s_list)>0);
 lv_area_t area;lv_obj_get_content_coords(s_list,&area);
 lv_indev_t *input=lv_indev_create();lv_indev_set_type(input,LV_INDEV_TYPE_POINTER);lv_indev_set_read_cb(input,macro_read_touch);
 lv_indev_set_scroll_limit(input,2);lv_indev_set_scroll_throw(input,0);
 unsigned before=macro_sends;
 macro_touch=(lv_point_t){(area.x1+area.x2)/2,area.y2-8};macro_touch_down=true;lv_indev_read(input);
 int travel=(area.y2-area.y1)/2;
 for(int i=1;i<=6;i++){macro_touch.y=area.y2-8-travel*i/6;lv_indev_read(input);}
 macro_touch_down=false;lv_indev_read(input);lv_obj_update_layout(s_root);
 assert(lv_obj_get_scroll_y(s_list)>0 && !s_confirm && macro_sends==before);
 lv_anim_delete(input,NULL);lv_indev_delete(input);
 lv_obj_scroll_to_y(s_list,0,LV_ANIM_OFF);lv_obj_update_layout(s_root);
}
static void run(int width){
 unsigned baseline_timers=macro_timer_count();macro_accept=true;macro_send_hook=NULL;ui_macros_show(send_macro);lv_obj_set_width(s_root,width);lv_obj_update_layout(s_root);cards(s_list);inside(s_list,s_root);assert(lv_obj_get_height(s_list)>100 && s_macros->rows[63]);if(ui_theme_is_studio()){assert(lv_obj_get_height(s_list)>=340);lv_obj_t *header=lv_obj_get_child(s_root,0);for(uint32_t i=0;i<lv_obj_get_child_count(header);i++){lv_obj_t *child=lv_obj_get_child(header,i);if(!lv_obj_has_flag(child,LV_OBJ_FLAG_HIDDEN))inside(child,header);}cards(header);}
 if(ui_theme_is_studio())for(unsigned i=0;i<64;i++){
  lv_obj_t *entry=s_macros->rows[i];assert(entry);
  assert(lv_obj_get_style_radius(entry,0)==0&&lv_obj_get_style_shadow_width(entry,0)==0);
  assert(lv_obj_get_style_border_side(entry,0)==LV_BORDER_SIDE_BOTTOM);
  assert(lv_color_eq(lv_obj_get_style_border_color(entry,0),UI_BORDER));
 }
 for(unsigned i=0;i<64;i++){
  lv_obj_t *entry=s_macros->rows[i],*icon=lv_obj_get_child(entry,0),*name=lv_obj_get_child(entry,1);
  assert(lv_obj_get_style_text_align(name,0)==LV_TEXT_ALIGN_LEFT);
  assert(lv_label_get_long_mode(name)==LV_LABEL_LONG_WRAP);
  lv_area_t a,b;lv_obj_get_coords(icon,&a);lv_obj_get_coords(name,&b);assert(a.x2<b.x1);
  inside(name,entry);
 }
 assert(lv_obj_has_state(s_macros->clear_action,LV_STATE_DISABLED));macro_drag_list();
 lv_obj_t *first=s_macros->rows[0];fav[63]=true;gen++;rebuild_macro_list();lv_obj_update_layout(s_root);cards(s_list);assert(first==s_macros->rows[0]);
 strcpy(s_macros->query,"nothing");rebuild_macro_list();lv_obj_update_layout(s_root);assert(s_macros->empty && !lv_obj_has_flag(s_macros->empty,LV_OBJ_FLAG_HIDDEN));inside(s_macros->empty,s_list);
 clear_search_cb(NULL);lv_obj_update_layout(s_root);
 assert(!lv_obj_has_state(lv_obj_get_parent(s_macros->search_label),LV_STATE_CHECKED));
 lv_obj_t *actions=lv_obj_get_parent(s_macros->clear_action);
 cards(actions);
 for(uint32_t i=0;i<lv_obj_get_child_count(actions);i++){
  lv_obj_t *button=lv_obj_get_child(actions,i);if(!lv_obj_check_type(button,&lv_button_class))continue;
  lv_obj_t *label=lv_obj_get_child(button,0);
  assert(lv_obj_get_height(label)<=lv_obj_get_style_text_font(label,0)->line_height);
 }
 search_cb(NULL);lv_obj_t*editor_owner=s_macros->editor;lv_textarea_set_text(s_macros->editor_value,"draft");search_cb(NULL);assert(s_macros->editor==editor_owner&&!strcmp(lv_textarea_get_text(s_macros->editor_value),"draft"));macro_popup_bounds(s_macros->editor,width);keyboard_layout_check(s_macros->editor,lv_obj_get_child(s_macros->editor,2),s_macros->editor_value);snapshot(s_macros->editor);
 lv_textarea_set_text(s_macros->editor_value,"discard me");lv_obj_send_event(lv_obj_get_child(s_macros->editor,2),LV_EVENT_CANCEL,NULL);assert(!s_macros->editor);search_cb(NULL);lv_textarea_set_text(s_macros->editor_value,"MACRO_00");lv_obj_send_event(lv_obj_get_child(s_macros->editor,2),LV_EVENT_READY,NULL);
 assert(!s_macros->editor && !strcmp(s_macros->query,"MACRO_00"));
 lv_obj_t *search_action=lv_obj_get_parent(s_macros->search_label);
 assert(lv_obj_has_state(search_action,LV_STATE_CHECKED) && !strcmp(lv_label_get_text(s_macros->search_label),"Search"));
 assert(!lv_obj_has_state(s_macros->clear_action,LV_STATE_DISABLED));
 search_cb(NULL);lv_textarea_set_text(s_macros->editor_value,"discarded");editor_cancel_cb(NULL);
 assert(!strcmp(s_macros->query,"MACRO_00") && lv_obj_has_state(search_action,LV_STATE_CHECKED));
 clear_search_cb(NULL);assert(!lv_obj_has_state(search_action,LV_STATE_CHECKED));
 assert(lv_obj_has_state(s_macros->clear_action,LV_STATE_DISABLED));
 assert(!lv_obj_has_flag(s_macros->rows[0],LV_OBJ_FLAG_HIDDEN));
 clear_search_cb(NULL);lv_obj_update_layout(s_root);
 lv_obj_send_event(s_macros->rows[0],LV_EVENT_CLICKED,NULL);assert(s_confirm);
 macro_popup_bounds(s_confirm,width);
 for(size_t i=0;i<MACRO_PARAMETER_MAX+MACRO_PARAMETER_EXTRA;i++)assert(lv_obj_get_height(s_macros->parameter_fields[i])>=48);
 assert(lv_obj_get_height(lv_obj_get_child(s_confirm,0))==UI_FONT_TITLE->line_height+12);
 snapshot(s_confirm);
 lv_obj_send_event(s_macros->parameter_fields[0],LV_EVENT_CLICKED,NULL);
 assert(s_macros->editor);macro_popup_bounds(s_macros->editor,width);
 assert(lv_obj_get_height(s_macros->editor_value)>=48);
 lv_textarea_set_text(s_macros->editor_value,"205");editor_done_cb(NULL);
 assert(!strcmp(lv_textarea_get_text(s_macros->parameter_fields[0]),"205"));
 review_macro_cb(NULL);assert(s_confirm && strstr(s_macros->pending_command,"PARAM_0=205"));
 macro_popup_bounds(s_confirm,width);snapshot(s_confirm);
 unsigned before=macro_sends;config_generation++;run_macro_cb(NULL);
 assert(macro_sends==before && s_confirm);close_confirm();
 /* Simulate the page rebuild after changing the selected printer. */
 ui_macros_hide();ui_macros_show(send_macro);
 lv_obj_send_event(s_macros->rows[0],LV_EVENT_CLICKED,NULL);assert(s_confirm);
 lv_textarea_set_text(s_macros->parameter_fields[0],"205");review_macro_cb(NULL);
 connected=false;refresh_timer_cb(NULL);assert(lv_obj_has_state(s_macros->run_action,LV_STATE_DISABLED));macro_popup_bounds(s_confirm,width);run_macro_cb(NULL);assert(macro_sends==before);
 connected=true;refresh_timer_cb(NULL);assert(!lv_obj_has_state(s_macros->run_action,LV_STATE_DISABLED));
 macro_accept=false;run_macro_cb(NULL);assert(macro_sends==before+1&&s_confirm&&s_macros->reviewed&&strstr(s_macros->pending_command,"PARAM_0=205"));
 macro_accept=true;macro_send_hook=macro_reenter;run_macro_cb(NULL);macro_send_hook=NULL;assert(macro_sends==before+2 && !s_confirm && strstr(sent_macro,"PARAM_0=205"));
 snapshot(s_root);
 for(unsigned i=0;i<10;i++){
  search_cb(NULL);lv_obj_delete(s_macros->editor);assert(!s_macros->editor&&!s_macros->editor_value&&!s_macros->editor_target);editor_done_cb(NULL);
  lv_obj_send_event(s_macros->rows[0],LV_EVENT_CLICKED,NULL);assert(s_confirm);lv_obj_send_event(s_macros->parameter_fields[0],LV_EVENT_CLICKED,NULL);assert(s_macros->editor);lv_obj_delete(s_confirm);assert(!s_confirm&&!s_macros->editor&&!s_macros->parameter_fields[0]&&!s_pending_macro[0]);review_macro_cb(NULL);run_macro_cb(NULL);editor_done_cb(NULL);
 }
 lv_obj_send_event(s_macros->rows[0],LV_EVENT_CLICKED,NULL);lv_obj_send_event(s_macros->parameter_fields[0],LV_EVENT_CLICKED,NULL);lv_textarea_set_text(s_macros->editor_value,"draft-value");config_generation++;editor_done_cb(NULL);assert(s_macros->editor&&!lv_textarea_get_text(s_macros->parameter_fields[0])[0]);review_macro_cb(NULL);assert(!s_macros->reviewed);close_confirm();
 ui_macros_hide();ui_macros_show(send_macro);
 lv_obj_send_event(s_macros->rows[0],LV_EVENT_CLICKED,NULL);review_macro_cb(NULL);lv_obj_t*reviewed=s_confirm;char saved[MACRO_COMMAND_MAX];snprintf(saved,sizeof(saved),"%s",s_macros->pending_command);review_macro_cb(NULL);assert(s_confirm==reviewed&&!strcmp(saved,s_macros->pending_command));
 before=macro_sends;macro_send_hook=macro_retire;run_macro_cb(NULL);macro_send_hook=NULL;assert(macro_sends==before+1&&!s_confirm&&!s_macros->run_action);
 lv_tick_inc(5001); /* A separate dispatch case, outside accepted-command suppression. */
 lv_obj_send_event(s_macros->rows[0],LV_EVENT_CLICKED,NULL);review_macro_cb(NULL);before=macro_sends;macro_send_hook=macro_switch;run_macro_cb(NULL);macro_send_hook=NULL;assert(macro_sends==before+1&&s_confirm);run_macro_cb(NULL);assert(macro_sends==before+1);close_confirm();
 ui_macros_hide();ui_macros_show(send_macro);
 lv_obj_send_event(s_macros->rows[0],LV_EVENT_CLICKED,NULL);review_macro_cb(NULL);before=macro_sends;macro_send_hook=macro_retire_root;run_macro_cb(NULL);macro_send_hook=NULL;assert(macro_sends==before+1&&!s_root&&!s_list&&!s_status&&!s_confirm&&!s_refresh_timer&&macro_timer_count()==baseline_timers);
 editor_done_cb(NULL);review_macro_cb(NULL);run_macro_cb(NULL);refresh_timer_cb(NULL);clear_search_cb(NULL);field_cb(NULL);
 for(unsigned i=0;i<5;i++){
  ui_macros_show(send_macro);search_cb(NULL);lv_obj_delete(s_root);assert(!s_root&&!s_macros->editor&&!s_refresh_timer&&macro_timer_count()==baseline_timers);ui_macros_hide();
 }
 ui_macros_show(send_macro);search_cb(NULL);lv_textarea_set_text(s_macros->editor_value,"keep-unsaved");char query[48];snprintf(query,sizeof(query),"%s",s_macros->query);config_generation++;editor_done_cb(NULL);assert(s_macros->editor&&!strcmp(s_macros->query,query));ui_macros_hide();assert(!s_macros->editor&&!s_confirm&&macro_timer_count()==baseline_timers);
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
 const char *page=files_long_review_capture?"files-confirm-long":files_state_capture?files_state_capture:files_inline_snapshot?"files-inline":root==s_search_popup?"files-search":root==s_print_confirm_popup?"files-confirm":root==s_studio_metadata_popup?"files-metadata":root==s_file_detail_popup?"files-ready":"files";
#elif defined(TEST_NETWORK)
 const char *page=network_empty_capture?"network-empty":"network";
#else
 const char *page=root==s_macros->editor?"macros-search":root==s_confirm?(s_macros->reviewed?"macros-review":"macros-parameters"):"macros";
#endif
 snprintf(path,sizeof(path),"%s/%s-theme%u.ppm",folder,page,theme_case);FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n%d %d\n255\n",a.x2-a.x1+1,a.y2-a.y1+1);for(int y=a.y1;y<=a.y2;y++)for(int x=a.x1;x<=a.x2;x++){uint16_t v=raster[y*1024+x];unsigned char c[3]={(unsigned char)(((v>>11)&31)*255/31),(unsigned char)(((v>>5)&63)*255/63),(unsigned char)((v&31)*255/31)};fwrite(c,1,3,f);}fclose(f);
}
int main(void){lv_init();lv_display_t *d=lv_display_create(1024,600);static uint8_t buffer[1024*50*2];lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);lv_display_set_buffers(d,buffer,NULL,sizeof(buffer),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(d,flush);
#if defined(TEST_FILES)
 for(unsigned theme=0;theme<5;theme++)for(unsigned density=0;density<3;density++)for(unsigned large=0;large<2;large++){
  ui_theme_set_active((ui_theme_id_t)theme);ui_theme_set_density((ui_density_id_t)density);ui_theme_set_accessibility((ui_accessibility_t){.large_text=large});
  if(getenv("FILES_SCROLL_DIAGNOSTIC") && (density!=1 || large))continue;
  files_scroll_damage_check();
 }
 if(getenv("FILES_SCROLL_DIAGNOSTIC")){lv_display_delete(d);lv_deinit();return 0;}
#endif
 for(unsigned custom=0;custom<2;custom++)for(unsigned theme=0;theme<5;theme++)for(unsigned density=0;density<3;density++)for(unsigned large=0;large<2;large++)for(unsigned w=0;w<3;w++){
  if(theme==UI_THEME_STUDIO_DARK&&(w||custom))continue;
  custom_tokens=custom;theme_case=theme;density_case=density;large_case=large;width_case=w;ui_theme_set_active((ui_theme_id_t)theme);ui_theme_set_density((ui_density_id_t)density);ui_theme_set_accessibility((ui_accessibility_t){.large_text=large});run((int[]){theme==UI_THEME_STUDIO_DARK?976:854,640,480}[w]);
 }
#if defined(TEST_FILES)
 for(unsigned viewport=0;viewport<2;viewport++)for(unsigned theme=0;theme<5;theme++)for(unsigned density=0;density<3;density++)for(unsigned large=0;large<2;large++){
  lv_display_set_resolution(d,viewport?480:640,400);ui_theme_set_active(theme);ui_theme_set_density(density);ui_theme_set_accessibility((ui_accessibility_t){.large_text=large});
  ui_files_show();lv_obj_send_event(s_search_button,LV_EVENT_CLICKED,NULL);files_search_layout_check();
  lv_textarea_set_text(s_search_textarea,"Retained only in draft");lv_obj_send_event(s_search_cancel,LV_EVENT_CLICKED,NULL);assert(!s_search_popup);ui_files_hide();
 }
#endif
 lv_display_delete(d);lv_deinit();puts("PASS: responsive page bounds, wrapping, non-overlap and lifecycle across all five themes, three densities, both text sizes, three viewport widths and custom metric/profile overrides");}
