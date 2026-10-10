/* Real Console layout/dialogs and themed button press/focus styling. Transport is a fixture. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "custom_theme.h"
#include "ui_widgets.h"
#include "ui_studio_layout.h"
#include "ui_theme_preview.h"
#include "ui_console.c"
static moonraker_state_t state;
static uint32_t generation=1,sequence=64;
static size_t entries=64;
static unsigned sends,invalidations;
static bool accept=true,long_messages;
static char sent[CONSOLE_COMMAND_MAX+1];
bool custom_theme_color_override(uint32_t a,uint32_t b,uint32_t c,uint32_t *d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_metric_override(int32_t a,int32_t b,int32_t c,int32_t *d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_accent_override(uint8_t a,uint32_t *b){(void)a;(void)b;return false;}
bool custom_theme_surface_opacity(uint8_t *a){(void)a;return false;}
uint32_t moonraker_config_generation(void){return generation;}
const char *moonraker_config_active_profile_name(void){return "Workshop printer";}
void moonraker_state_snapshot(moonraker_state_t *o){*o=state;}
void ui_toast_show(ui_status_kind_t k,const char *t,const char *d){(void)k;(void)t;(void)d;}
size_t console_controller_count(void){return entries;}
uint32_t console_controller_latest_sequence(void){return sequence;}
bool console_controller_get(size_t i,console_entry_t *e){if(i>=entries)return false;memset(e,0,sizeof(*e));e->sequence=sequence-(uint32_t)i;e->type=i%2?CONSOLE_ENTRY_ERROR:CONSOLE_ENTRY_RESPONSE;snprintf(e->message,sizeof(e->message),"Message %u%.170s",e->sequence,long_messages?" | LONG TOKEN ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 followed by a complete multiline response that must wrap and remain readable at large text sizes.":"");return true;}
void console_controller_clear(void){entries=0;sequence++;}
size_t console_controller_history_count(void){return 2;}
bool console_controller_history_get(size_t i,char *o,size_t n){if(i>=2)return false;snprintf(o,n,"%s",i?"M105":"STATUS");return true;}
void console_controller_add_command(const char *c){(void)c;sequence++;}
void console_controller_add(console_entry_type_t t,const char *f,...){(void)t;(void)f;sequence++;}
static bool send(const char *command){sends++;snprintf(sent,sizeof(sent),"%s",command);return accept;}
static void click(lv_obj_t *b){lv_obj_send_event(b,LV_EVENT_CLICKED,NULL);}
static void inside(lv_obj_t *c,lv_obj_t *p){lv_area_t a,b;lv_obj_get_coords(c,&a);lv_obj_get_coords(p,&b);if(a.x1<b.x1||a.x2>b.x2||a.y1<b.y1||a.y2>b.y2){fprintf(stderr,"outside %d,%d..%d,%d vs %d,%d..%d,%d\n",a.x1,a.y1,a.x2,a.y2,b.x1,b.y1,b.x2,b.y2);abort();}}
static void same(lv_obj_t *label,const char *expected){assert(!strcmp(lv_textarea_get_text(label),expected));}
static void frame(lv_obj_t *b){lv_obj_update_layout(b);lv_area_t base,current;lv_obj_get_coords(b,&base);int width=lv_obj_get_style_border_width(b,LV_PART_MAIN),radius=lv_obj_get_style_radius(b,LV_PART_MAIN);
 lv_state_t states[]={LV_STATE_PRESSED,LV_STATE_FOCUSED|LV_STATE_PRESSED,LV_STATE_CHECKED|LV_STATE_PRESSED,LV_STATE_FOCUS_KEY|LV_STATE_FOCUSED,LV_STATE_DISABLED,LV_STATE_DISABLED|LV_STATE_PRESSED|LV_STATE_FOCUS_KEY};
 for(unsigned i=0;i<6;i++){lv_obj_add_state(b,states[i]);lv_obj_update_layout(b);lv_obj_get_coords(b,&current);assert(!memcmp(&base,&current,sizeof(base)));assert(!lv_obj_get_style_transform_width(b,0)&&!lv_obj_get_style_transform_height(b,0)&&!lv_obj_get_style_translate_y(b,0));assert(lv_obj_get_style_border_width(b,0)==width&&lv_obj_get_style_radius(b,0)==radius);if((states[i]&LV_STATE_FOCUS_KEY)&&!(states[i]&LV_STATE_DISABLED)){if(lv_obj_get_style_outline_width(b,0)!=2){fprintf(stderr,"focus fail theme%u kind? children%u outline%d\n",ui_theme_get_active(),(unsigned)lv_obj_get_child_count(b),lv_obj_get_style_outline_width(b,0));abort();}}else assert(!lv_obj_get_style_outline_width(b,0));lv_obj_remove_state(b,states[i]);}
}
static void buttons(lv_obj_t *parent){for(unsigned i=0;i<lv_obj_get_child_count(parent);i++){lv_obj_t *child=lv_obj_get_child(parent,i);if(lv_obj_check_type(child,&lv_button_class)){inside(child,parent);frame(child);for(unsigned j=0;j<lv_obj_get_child_count(child);j++)if(lv_obj_check_type(lv_obj_get_child(child,j),&lv_label_class)){inside(lv_obj_get_child(child,j),child);assert(lv_obj_get_height(lv_obj_get_child(child,j))<=lv_obj_get_style_text_font(lv_obj_get_child(child,j),0)->line_height);}}}}
static void page_bounds(void){lv_obj_update_layout(s_root);inside(s_root,lv_screen_active());for(unsigned i=0;i<lv_obj_get_child_count(s_root);i++)inside(lv_obj_get_child(s_root,i),s_root);buttons(s_filters);buttons(s_footer);assert(lv_obj_get_height(s_output)>32);}
static void popup_bounds(lv_obj_t *popup){lv_obj_update_layout(popup);inside(popup,lv_screen_active());for(unsigned i=0;i<lv_obj_get_child_count(popup);i++){lv_obj_t *c=lv_obj_get_child(popup,i);inside(c,popup);buttons(c);}assert(lv_obj_get_height(lv_obj_get_child(popup,1))==56);assert(lv_obj_get_height(lv_obj_get_child(popup,lv_obj_get_child_count(popup)-1))>=90);lv_obj_t *keys=lv_obj_get_child(popup,lv_obj_get_child_count(popup)-1);int stroke=lv_obj_get_style_border_width(keys,LV_PART_ITEMS),radius=lv_obj_get_style_radius(keys,LV_PART_ITEMS);lv_obj_add_state(keys,LV_STATE_PRESSED);assert(stroke==lv_obj_get_style_border_width(keys,LV_PART_ITEMS)&&radius==lv_obj_get_style_radius(keys,LV_PART_ITEMS));assert(!lv_obj_get_style_transform_width(keys,LV_PART_ITEMS)&&!lv_obj_get_style_transform_height(keys,LV_PART_ITEMS));lv_obj_remove_state(keys,LV_STATE_PRESSED);}
static uint16_t raster[1024*600];
static void flush(lv_display_t *d,const lv_area_t *a,uint8_t *p){uint16_t *v=(uint16_t *)p;for(int y=a->y1;y<=a->y2;y++)for(int x=a->x1;x<=a->x2;x++)raster[y*1024+x]=*v++;lv_display_flush_ready(d);}
static void invalidated(lv_event_t *e){(void)e;invalidations++;}
static void capture(const char *name){const char *folder=getenv("CONSOLE_SCREENSHOTS");if(!folder||lv_display_get_horizontal_resolution(NULL)!=1024||ui_theme_get_density()!=1||!ui_theme_get_accessibility().large_text)return;lv_refr_now(NULL);char path[512];snprintf(path,sizeof(path),"%s/%s-theme%u.ppm",folder,name,(unsigned)ui_theme_get_active());FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n1024 600\n255\n");for(unsigned i=0;i<1024*600;i++){uint16_t v=raster[i];unsigned char c[3]={(v>>11)*255/31,((v>>5)&63)*255/63,(v&31)*255/31};fwrite(c,1,3,f);}fclose(f);}
static void press_catalog(void){
 for(unsigned kind=0;kind<=UI_BUTTON_OUTLINED;kind++){lv_obj_t *b=ui_button_create(lv_screen_active(),kind,"ACTION");lv_obj_set_size(b,140,48);frame(b);ui_button_apply_kind(b,UI_BUTTON_DANGER);frame(b);lv_obj_delete(b);}
 for(unsigned kind=0;kind<=UI_STATUS_ACTIVE;kind++){lv_obj_t *b=lv_button_create(lv_screen_active());ui_apply_button_status_style(b,kind);lv_obj_set_size(b,140,48);frame(b);lv_obj_delete(b);}
 lv_obj_t *b=ui_create_button(lv_screen_active(),"CUSTOM",UI_CONTROL);lv_obj_set_size(b,140,48);frame(b);lv_obj_delete(b);
 b=ui_create_operator_nav_button(lv_screen_active(),8,8,156,52,LV_SYMBOL_HOME,"Dashboard");frame(b);lv_obj_delete(b);
 b=studio_action(lv_screen_active(),"Studio",8,8,156,52,NULL,NULL);frame(b);lv_obj_delete(b);
 b=ui_theme_preview_create(lv_screen_active(),ui_theme_get_active(),false,0,0,300,240,NULL,NULL);frame(b);lv_obj_delete(b);
 b=ui_theme_preview_create(lv_screen_active(),ui_theme_get_active(),true,0,0,300,240,NULL,NULL);frame(b);lv_obj_delete(b);
 b=lv_obj_create(lv_screen_active());ui_apply_surface_role(b,UI_SURFACE_LIST_ROW);int stroke=lv_obj_get_style_border_width(b,0);lv_obj_add_state(b,LV_STATE_PRESSED);assert(lv_obj_get_style_border_width(b,0)==stroke&&!lv_obj_get_style_transform_width(b,0)&&!lv_obj_get_style_transform_height(b,0));lv_obj_delete(b);
}
int main(void){lv_init();lv_display_t *d=lv_display_create(1024,600);static uint8_t pixels[1024*50*2];lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);lv_display_set_buffers(d,pixels,NULL,sizeof(pixels),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(d,flush);lv_display_add_event_cb(d,invalidated,LV_EVENT_INVALIDATE_AREA,NULL);
 for(unsigned high_contrast=0;high_contrast<2;high_contrast++)for(unsigned theme=0;theme<5;theme++)for(unsigned density=0;density<3;density++)for(unsigned large=0;large<2;large++)for(unsigned narrow=0;narrow<3;narrow++){
  lv_display_set_resolution(d,(int[]){1024,640,480}[narrow],narrow==2?400:600);ui_theme_set_active(theme);ui_theme_set_density(density);ui_theme_set_accessibility((ui_accessibility_t){.large_text=large,.reduced_motion=high_contrast,.high_contrast=high_contrast});state=(moonraker_state_t){.moonraker_ok=true,.live_data_ok=true};generation++;entries=64;sequence=64;sends=0;accept=true;long_messages=true;s_query[0]=0;s_filter=CONSOLE_FILTER_ALL;s_hide_temperatures=false;s_follow=true;
  ui_console_show(send);page_bounds();assert(s_rows->rows[63]);assert(lv_label_get_long_mode(s_rows->rows[0])==LV_LABEL_LONG_WRAP);assert(lv_obj_get_height(s_rows->rows[0])>UI_FONT_CAPTION->line_height);capture("console");
  lv_obj_t *first=s_rows->rows[0];s_follow=false;lv_obj_scroll_to_y(s_output,200,LV_ANIM_OFF);lv_obj_update_layout(s_root);int scroll=lv_obj_get_scroll_y(s_output);rebuild_output();assert(scroll==lv_obj_get_scroll_y(s_output)&&s_rows->rows[0]==first);
  uint32_t anchor=0;int anchor_y=0;lv_area_t vp;lv_obj_get_coords(s_output,&vp);
  for(unsigned i=0;i<64;i++){lv_area_t a;lv_obj_get_coords(s_rows->rows[i],&a);if(a.y2>=vp.y1){anchor=s_row_sequence[i];anchor_y=a.y1;break;}}
  sequence++;rebuild_output();bool retained=false;for(unsigned i=0;i<64;i++)if(s_row_sequence[i]==anchor){lv_area_t a;lv_obj_get_coords(s_rows->rows[i],&a);assert(a.y1==anchor_y);retained=true;break;}assert(retained);sequence=64;rebuild_output();
  lv_obj_send_event(s_output,LV_EVENT_SCROLL_BEGIN,NULL);assert(!s_follow);follow_cb(NULL);assert(s_follow);s_follow=false;
  s_filter=CONSOLE_FILTER_ERRORS;rebuild_output();assert(!strcmp(lv_label_get_text(s_filter_count),"32 / 64"));s_filter=CONSOLE_FILTER_ALL;strcpy(s_query,"Message 64");rebuild_output();assert(!strcmp(lv_label_get_text(s_filter_count),"1 / 64"));s_query[0]=0;rebuild_output();
  lv_refr_now(d);invalidations=0;for(unsigned i=0;i<100;i++)refresh_timer_cb(NULL);assert(!invalidations);
  click(s_command_button);assert(s_command_popup);popup_bounds(s_command_popup);capture("console-command");history_prev_cb(NULL);same(s_command_input,"STATUS");history_prev_cb(NULL);same(s_command_input,"M105");history_next_cb(NULL);same(s_command_input,"STATUS");history_next_cb(NULL);same(s_command_input,"");
  lv_textarea_set_text(s_command_input,"  STATUS  ");generation++;send_command_cb(NULL);assert(!sends&&s_command_popup);assert(strstr(lv_label_get_text(s_command_feedback),"Printer changed"));close_command_popup();open_command_cb(NULL);lv_textarea_set_text(s_command_input,"STATUS");state.moonraker_ok=false;send_command_cb(NULL);assert(!sends);state.moonraker_ok=true;accept=false;send_command_cb(NULL);assert(sends==1&&s_command_popup);same(s_command_input,"STATUS");assert(strstr(lv_label_get_text(s_command_feedback),"Send failed"));accept=true;send_command_cb(NULL);assert(sends==2&&!s_command_popup&&!strcmp(sent,"STATUS"));
  search_cb(NULL);popup_bounds(s_search_popup);lv_textarea_set_text(s_search_input,"Message 64");search_done_cb(NULL);assert(!strcmp(s_query,"Message 64")&&!s_search_popup);search_cb(NULL);lv_obj_delete(s_search_popup);assert(!s_search_popup&&!s_search_input);
  open_command_cb(NULL);lv_obj_delete(s_command_popup);assert(!s_command_popup&&!s_command_input&&!s_command_send);state.moonraker_ok=false;update_connection();assert(lv_obj_has_state(s_command_button,LV_STATE_DISABLED));open_command_cb(NULL);assert(!s_command_popup);state.moonraker_ok=true;update_connection();
  state.live_data_ok=false;strcpy(state.printer_state,"error");open_command_cb(NULL);assert(s_command_popup);lv_textarea_set_text(s_command_input,"RESTART");send_command_cb(NULL);assert(!s_command_popup&&!strcmp(sent,"RESTART"));reset_filters_cb(NULL);clear_cb(NULL);assert(!strcmp(lv_label_get_text(s_filter_count),"0 / 0"));ui_console_hide();assert(!s_root&&!s_refresh_timer&&!lv_obj_get_child_count(lv_layer_top()));press_catalog();
 }
 lv_display_delete(d);lv_deinit();puts("PASS: Console layouts/history/filters/editor, owner/offline send guards, rejection retention, no-op refresh, lifecycle and pressed/focused/checked/disabled button frames across themes/densities/text sizes/viewports");return 0;}
