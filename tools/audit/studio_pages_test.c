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
static moonraker_state_t state;
void moonraker_state_snapshot(moonraker_state_t *s){*s=state;}
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

#include <stdlib.h>
#include "ui_page_layout_profile.h"
const ui_page_layout_profile_t *custom_theme_page_profile(void){static ui_page_layout_profile_t profile;if(!custom_tokens)return NULL;profile=*ui_page_layout_profile_for_theme(ui_theme_get_active());profile.files.subtitle="Custom styled job library";return &profile;}
void ui_shell_raise_topbar(void){}
void ui_shell_raise_nav(void){}

static void inside(lv_obj_t *child,lv_obj_t *parent){lv_area_t a,b;lv_obj_get_coords(child,&a);lv_obj_get_coords(parent,&b);if(!(a.x1>=b.x1 && a.x2<=b.x2 && a.y1>=b.y1 && a.y2<=b.y2)){fprintf(stderr,"outside: child %d,%d..%d,%d parent %d,%d..%d,%d\n",a.x1,a.y1,a.x2,a.y2,b.x1,b.y1,b.x2,b.y2);abort();}}

#include "ui_studio_layout.h"
static unsigned sends;
static char command[128];
static bool send_command(const char *c){sends++;snprintf(command,sizeof(command),"%s",c);return true;}
static void clicked(lv_event_t *e){(void)e;}
static void action(const char *c,lv_event_t *e){(void)e;send_command(c);}
moonraker_filament_status_t moonraker_filament_state_status(const moonraker_filament_state_t *s,size_t *p,size_t *e){(void)s;if(p)*p=1;if(e)*e=1;return MOONRAKER_FILAMENT_READY;}
static void children_inside(lv_obj_t *parent){lv_obj_update_layout(parent);for(uint32_t i=0;i<lv_obj_get_child_count(parent);i++){lv_obj_t *child=lv_obj_get_child(parent,i);if(lv_obj_has_flag(child,LV_OBJ_FLAG_HIDDEN))continue;inside(child,parent);children_inside(child);}}
static lv_obj_t *page_root(void){lv_obj_t *r=lv_obj_create(lv_screen_active());lv_obj_set_size(r,854,528);ui_apply_root_style(r);return r;}
static uint16_t raster[1024*600];
static void flush(lv_display_t *d,const lv_area_t *a,uint8_t *p){size_t w=(size_t)(a->x2-a->x1+1);for(int y=a->y1;y<=a->y2;y++)memcpy(raster+y*1024+a->x1,p+(size_t)(y-a->y1)*w*2,w*2);lv_display_flush_ready(d);}
static void snapshot(const char *name){if(ui_theme_get_density()!=UI_DENSITY_COMFORTABLE || ui_theme_get_accessibility().large_text)return;const char *dir=getenv("STUDIO_SCREENSHOTS");if(!dir)return;lv_obj_invalidate(lv_screen_active());lv_refr_now(NULL);char path[512];snprintf(path,sizeof(path),"%s/%s.ppm",dir,name);FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n1024 600\n255\n");for(size_t i=0;i<1024*600;i++){uint16_t v=raster[i];unsigned char c[3]={(v>>11)*255/31,((v>>5)&63)*255/63,(v&31)*255/31};fwrite(c,1,3,f);}fclose(f);}
#if defined(TEST_PRINTER)
#include "ui_printer_live_status.c"
#include "ui_printer.c"
static unsigned preview_opens;
void ui_preview_lightbox_show_file_object(lv_obj_t *o,const char *f){assert(o && !strcmp(f,"job.gcode"));preview_opens++;}
int moonraker_config_active_profile_index(void){return 0;}
const lv_image_dsc_t *printer_preview_cache_image(int profile,const char **file,uint32_t *revision){
 static uint16_t pixels[20*10];
 static lv_image_dsc_t image={.header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_RGB565,.w=20,.h=10,.stride=40},.data_size=sizeof(pixels),.data=(const uint8_t *)pixels};
 assert(profile==0);*file="job.gcode";*revision=1;return &image;
}
static void send_motion(const char *c);
static unsigned toolhead_opens;
static void open_toolhead(lv_event_t *e){(void)e;toolhead_opens++;static lv_obj_t *a,*b,*c;static double step=10;ui_printer_motion_show(&a,&b,&c,&step,send_motion);}
#include "ui_printer_layout.h"
#include "ui_printer_info_cards.h"
#include "ui_printer_actions.h"
static void send_motion(const char *c){(void)send_command(c);}
static void check_toolhead_offset(void){
 for(int theme=0;theme<5;theme++) {
  ui_theme_set_active((ui_theme_id_t)theme);lv_obj_t *a=NULL,*b=NULL,*c=NULL;double step=10;
  ui_printer_motion_show(&a,&b,&c,&step,send_motion);
  lv_obj_t *popup=lv_obj_get_child(lv_layer_top(),-1);lv_obj_update_layout(popup);
  lv_obj_t *caption=NULL,*value=NULL,*adjust=NULL;
  for(uint32_t i=0;i<lv_obj_get_child_count(popup);i++){
   lv_obj_t *o=lv_obj_get_child(popup,i);if(lv_obj_check_type(o,&lv_button_class)&&lv_obj_get_child_count(o)&&!strcmp(lv_label_get_text(lv_obj_get_child(o,0)),"-0.05"))adjust=o;if(!lv_obj_check_type(o,&lv_label_class))continue;
   const char *text=lv_label_get_text(o);
   if(!strcmp(text,"RUNTIME Z OFFSET"))caption=o;
   if(lv_obj_get_y(o)==326){value=o;lv_label_set_text(value,"-10.000 mm");}
  }
  assert(caption&&value&&adjust);lv_area_t ca,va,ba;lv_obj_get_coords(caption,&ca);lv_obj_get_coords(value,&va);lv_obj_get_coords(adjust,&ba);assert(ca.y2<va.y1);assert(va.y2<ba.y1);
  lv_obj_t *close=lv_obj_get_child(lv_obj_get_child(popup,-1),0);lv_obj_send_event(close,LV_EVENT_CLICKED,NULL);
  assert(lv_obj_get_child_count(lv_layer_top())==0);
 }
 ui_theme_set_active(UI_THEME_STUDIO_DARK);
}
static void run(void){
 check_toolhead_offset();
 /* Reproduce the panel's Operator -> Studio navigation sequence. */
 for (int cycle=0; cycle<3; cycle++) for (int theme=UI_THEME_CLASSIC; theme<UI_THEME_STUDIO_DARK; theme++) {
  ui_theme_set_active((ui_theme_id_t)theme);
  lv_obj_t *old=page_root();lv_obj_t *old_file,*old_speed,*old_flow,*old_layer,*old_filament;
  ui_printer_live_status_create(old,&old_file,&old_speed,&old_flow,&old_layer,&old_filament,clicked,100,100,send_command);
  assert(s_speed_factor_slider && s_flow_factor_slider && s_speed_factor_value && s_flow_factor_value);
  lv_obj_delete(old);
  assert(!s_tuning_owner && !s_speed_factor_slider && !s_flow_factor_slider && !s_speed_factor_value && !s_flow_factor_value);
  ui_printer_live_status_refresh(NULL,NULL,NULL,NULL,NULL,NULL,NULL,"standby","",0,0,100,100,0,0,0,0,0,true,NULL);
  ui_theme_set_active(UI_THEME_STUDIO_DARK);
  lv_obj_t *next=page_root();
  ui_printer_live_status_create(next,&old_file,&old_speed,&old_flow,&old_layer,&old_filament,clicked,100,100,send_command);
  assert(!s_speed_factor_slider && !s_flow_factor_slider);
  ui_printer_live_status_refresh(next,NULL,old_file,old_speed,old_flow,old_layer,old_filament,"standby","",0,0,100,100,0,0,0,0,0,true,NULL);
  studio_tuning_open(NULL);assert(s_speed_factor_slider && s_flow_factor_slider);
  lv_obj_delete(next);
  assert(!s_tuning_owner && !s_studio_position && !s_studio_tuning_popup && !s_speed_factor_slider && !s_flow_factor_slider);

 }
 ui_theme_set_active(UI_THEME_STUDIO_DARK);
 lv_obj_t *r=page_root();ui_printer_layout_t l;assert(ui_printer_layout_create(r,&l));
 lv_obj_t *file,*speed,*flow,*layer,*filament;
 ui_printer_live_status_create(l.active_panel,&file,&speed,&flow,&layer,&filament,clicked,1,1,send_command);
 ui_printer_info_cards_t cards;ui_printer_info_cards_create(l.status_panel,&cards,clicked,clicked,clicked);
 ui_printer_actions_t actions;ui_printer_actions_create(l.action_panel,&actions,clicked,open_toolhead);
 ui_printer_preview_create(l.active_panel);
 lv_obj_update_layout(l.active_panel);
 assert(lv_obj_get_width(s_preview_box)==620 && lv_obj_get_height(s_preview_box)==240);
 ui_printer_info_cards_refresh_live(r,&cards,.64,205,210,60,60,80,3600,true,NULL);
 ui_printer_live_status_refresh(r,NULL,file,speed,flow,layer,filament,"standby","job.gcode",120,8.2,1,1,215,336,0,0,.64,true,NULL);
 children_inside(r);assert(!lv_obj_has_flag(r,LV_OBJ_FLAG_SCROLLABLE));snapshot("printer-native");
 assert(lv_obj_get_y(s_studio_position)>lv_obj_get_height(s_preview_box));
 assert(!strcmp(lv_label_get_text(layer),"215 / 336"));
 assert(!strcmp(lv_label_get_text(lv_obj_get_child(actions.motion,0)),"Toolhead Control"));
 unsigned before=toolhead_opens;lv_obj_send_event(actions.motion,LV_EVENT_CLICKED,NULL);assert(toolhead_opens==before+1);
 lv_obj_t *popup=lv_obj_get_child(lv_layer_top(),-1);children_inside(popup);
 lv_obj_send_event(lv_obj_get_child(lv_obj_get_child(popup,-1),0),LV_EVENT_CLICKED,NULL);
 assert(lv_obj_get_child_count(lv_layer_top())==0);
 ui_printer_preview_show("printing","job.gcode","");
 assert(s_preview_image && lv_obj_has_flag(s_preview_label,LV_OBJ_FLAG_HIDDEN));
 unsigned previews=preview_opens;lv_obj_send_event(s_preview_box,LV_EVENT_CLICKED,NULL);assert(preview_opens==previews+1);
 ui_printer_preview_destroy_refs();
 moonraker_capabilities_t caps={.discovered=true,.has_heated_bed=true,.has_part_fan=false};ui_printer_info_cards_refresh_live(r,&cards,.64,205,210,60,60,0,3600,true,&caps);assert(!lv_obj_has_state(l.status_panel,LV_STATE_DISABLED));assert(!strcmp(lv_label_get_text(cards.part_fan),"N/A"));
 studio_tuning_open(NULL);assert(s_studio_tuning_popup);studio_tuning_close(NULL);assert(!s_speed_factor_slider && !s_flow_factor_slider);studio_tuning_open(NULL);assert(s_studio_tuning_popup);children_inside(s_studio_tuning_popup);lv_slider_set_value(s_speed_factor_slider,120,LV_ANIM_OFF);lv_obj_send_event(s_speed_factor_slider,LV_EVENT_RELEASED,NULL);assert(!strcmp(command,"M220 S120"));lv_obj_delete(r);assert(!s_studio_position && !s_studio_tuning_popup && !s_speed_factor_slider);
}
#elif defined(TEST_DRYBOX)
#include "ui_drybox_page.c"
static const char *banner(void){return "DRYBOX READY";}
static void run(void){ui_drybox_page_t p={0};assert(ui_drybox_page_create(&p,action,banner));ui_drybox_page_state_t values={.banner_text="DRYBOX ACTIVE",.air_temp=48.2,.center_temp=46.8,.humidity=18,.heater_target=50,.heater_on=true,.fan_speed=75};ui_drybox_page_refresh(&p,&values);children_inside(p.panel);assert(!lv_obj_has_flag(p.panel,LV_OBJ_FLAG_SCROLLABLE));assert(lv_arc_get_value(s_studio_humidity_arc)==18);snapshot("drybox-native");values.humidity=100;ui_drybox_page_refresh(&p,&values);children_inside(p.panel);values.banner_text="DRYBOX OFFLINE";ui_drybox_page_refresh(&p,&values);assert(lv_arc_get_value(s_studio_humidity_arc)==0);for(int i=1;i<DRYBOX_PROGRAM_COUNT;i++)assert(lv_obj_has_state(s_program_buttons[i],LV_STATE_DISABLED));ui_drybox_page_cleanup(&p);assert(!s_studio_humidity_arc);}
#elif defined(TEST_CONSOLE)
#include "ui_console.c"
const char *moonraker_config_active_profile_name(void){return "Workshop printer";}
static void run(void){s_query[0]=0;s_follow=true;ui_console_show(send_command);lv_obj_update_layout(s_root);for(unsigned i=0;i<lv_obj_get_child_count(s_root);i++)inside(lv_obj_get_child(s_root,i),s_root);assert(lv_obj_get_child_count(s_output)==64 && lv_obj_has_flag(s_output,LV_OBJ_FLAG_SCROLLABLE));assert(strstr(lv_label_get_text(s_rows->rows[0]),"Message 63"));snapshot("console-native");strcpy(s_query,"Message 63");rebuild_output();assert(strstr(lv_label_get_text(s_rows->rows[0]),"Message 63"));ui_console_hide();assert(!s_root && !s_output);}
#elif defined(TEST_CALIBRATION)
#include "ui_calibration_layout.c"
static void run(void){lv_obj_t *r=page_root();ui_calibration_card_refs_t refs[4];lv_obj_t *cards[4];for(int i=0;i<4;i++)cards[i]=ui_calibration_layout_card(r,"WORKFLOW",i%2?430:20,i>1?322:126,&refs[i]);children_inside(r);assert(lv_obj_get_x(cards[1])+lv_obj_get_width(cards[1])==976);assert(lv_obj_get_y(cards[3])+lv_obj_get_height(cards[3])==424);snapshot("calibration-layout-native");lv_obj_delete(r);}
#elif defined(TEST_BED_MESH)
#include "ui_bed_mesh.c"
bool bed_mesh_controller_snapshot(bed_mesh_snapshot_t *out,float *v,size_t n,bed_mesh_profile_name_t *p,size_t c){(void)v;(void)n;(void)p;(void)c;memset(out,0,sizeof(*out));return false;}
void ui_bed_mesh_profiles_init(lv_obj_t *owner,ui_bed_mesh_profiles_command_cb_t cb){(void)owner;(void)cb;}
void ui_bed_mesh_profiles_update(const bed_mesh_snapshot_t *mesh){(void)mesh;}
void ui_bed_mesh_profiles_show_cb(lv_event_t *e){(void)e;}
void ui_bed_mesh_profiles_close(void){}
static void run(void){ui_bed_mesh_show(send_command);lv_obj_update_layout(s.popup);assert(s.canvas && lv_obj_get_width(s.popup)==976 && lv_obj_get_height(s.popup)==424);children_inside(s.popup);assert(lv_obj_get_width(s.canvas)==776 && lv_obj_get_height(s.canvas)==250);snapshot("bed-mesh-layout-native");ui_bed_mesh_close();assert(!s.popup && !s.buf);}
#else
#include "ui_settings_components.c"
static void run(void){lv_obj_t *r=page_root();studio_text(r,"Settings",0,0,500,&ui_studio_font_48,UI_TEXT);lv_obj_t *body=studio_plane(r,0,66,976,358);lv_obj_add_flag(body,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_scroll_dir(body,LV_DIR_VER);lv_obj_t *section=ui_settings_section_create(body,"Display",0,332);ui_settings_section_add_percent_slider_row(section,"Brightness","Adjust the display backlight",69,10,100,52,clicked);ui_settings_section_add_row(section,"Theme","Choose the interface", "STUDIO Dark",120,clicked);ui_settings_section_add_row(section,"Text size","Choose the reading size","Standard",188,clicked);ui_settings_section_add_action_row(section,"Calibration","Adjust panel touch alignment","Calibrate",256,clicked,false);children_inside(section);snapshot("settings-components-native");lv_obj_delete(r);}
#endif
int main(void){lv_init();lv_display_t *d=lv_display_create(1024,600);static uint8_t buffer[1024*50*2];lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);lv_display_set_buffers(d,buffer,NULL,sizeof(buffer),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(d,flush);ui_theme_set_active(UI_THEME_STUDIO_DARK);ui_theme_set_density(UI_DENSITY_COMFORTABLE);ui_theme_set_accessibility((ui_accessibility_t){0});ui_apply_root_style(lv_screen_active());state.moonraker_ok=state.live_data_ok=true;strcpy(state.printer_state,"standby");strcpy(state.homed_axes,"xyz");state.toolhead_position_valid=true;state.toolhead_x=120;state.toolhead_y=120;state.toolhead_z=1.2;for(unsigned density=0;density<3;density++)for(unsigned large=0;large<2;large++){ui_theme_set_density((ui_density_id_t)density);ui_theme_set_accessibility((ui_accessibility_t){.large_text=large});run();}lv_display_delete(d);lv_deinit();puts("PASS: STUDIO native page bounds, control behavior and teardown");}
