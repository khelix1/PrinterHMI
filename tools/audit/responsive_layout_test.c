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
static void run(int width){ui_tools_show();lv_obj_set_width(s_root,width);lv_obj_update_layout(s_root);cards(s_tiles);assert(lv_obj_get_child_count(s_tiles)==4);snapshot(s_root);ui_tools_hide();assert(!s_root && !s_tiles);}
#elif defined(TEST_FILES)
#include "ui_page_state.c"
#include "ui_files.c"
void ui_preview_lightbox_show_file_object(lv_obj_t *i,const char *f){(void)i;(void)f;}
static void run(int width){
 ui_files_show();lv_obj_set_width(s_printer_file_popup,width);ui_files_set_search_text("very long query");ui_files_set_sort_text("NEWEST");lv_obj_update_layout(s_printer_file_popup);
 lv_obj_t *toolbar=lv_obj_get_child(s_printer_file_popup,1);cards(toolbar);assert(lv_obj_get_child_count(toolbar)==4);inside(toolbar,s_printer_file_popup);inside(s_breadcrumb_label,s_printer_file_popup);
 lv_obj_t *viewport=lv_obj_get_parent(s_printer_file_list);inside(viewport,s_printer_file_popup);assert(lv_obj_get_height(viewport)>100);inside(s_printer_file_list,viewport);
 for(int i=0;i<5;i++)ui_files_add_file_entry("folder/Very_long_operator_job_name_for_responsive_row_checks.gcode",1048576,0,i*118);
 ui_files_add_folder_button("Long folder title for row checks","folder",590);lv_obj_update_layout(s_printer_file_popup);cards(s_printer_file_list);
 ui_files_set_status("Moonraker offline. Check the active printer.");lv_obj_update_layout(s_printer_file_popup);inside(s_files_state->root,viewport);inside(s_files_state->title,s_files_state->root);inside(s_files_state->detail,s_files_state->root);
 lv_area_t title,description;lv_obj_get_coords(s_files_state->title,&title);lv_obj_get_coords(s_files_state->detail,&description);assert(title.y2<description.y1);
 snapshot(s_printer_file_popup);ui_files_hide();assert(!s_printer_file_popup);
}
#else
static uint32_t gen=1;static bool fav[64];static char selected[64];
void macro_controller_status(macro_controller_status_t *s){memset(s,0,sizeof(*s));s->discovered=true;s->count=s->total_count=64;s->generation=gen;}
bool macro_controller_get(size_t i,char *o,size_t n){if(i>=64)return false;snprintf(o,n,"MACRO_%02u_WITH_A_VERY_LONG_OPERATOR_ACTION_NAME",(unsigned)i);return true;}
bool macro_controller_is_favorite(const char *n){unsigned i=0;sscanf(n,"MACRO_%u",&i);return i<64 && fav[i];}
bool macro_controller_toggle_favorite(const char *n){unsigned i=0;sscanf(n,"MACRO_%u",&i);if(i>=64)return false;fav[i]=!fav[i];gen++;return true;}
bool macro_controller_parameters(const char *n,macro_parameter_catalog_t *p){snprintf(selected,sizeof(selected),"%s",n);memset(p,0,sizeof(*p));p->count=8;for(int i=0;i<8;i++)snprintf(p->names[i],sizeof(p->names[i]),"PARAM_%d",i);return true;}

#include "ui_macros.c"
static void run(int width){
 ui_macros_show(NULL);lv_obj_set_width(s_root,width);lv_obj_update_layout(s_root);cards(s_list);inside(s_list,s_root);assert(lv_obj_get_height(s_list)>100 && s_macros->rows[63]);
 lv_obj_t *first=s_macros->rows[0];fav[63]=true;gen++;rebuild_macro_list();lv_obj_update_layout(s_root);cards(s_list);assert(first==s_macros->rows[0]);
 strcpy(s_macros->query,"nothing");rebuild_macro_list();lv_obj_update_layout(s_root);assert(s_macros->empty && !lv_obj_has_flag(s_macros->empty,LV_OBJ_FLAG_HIDDEN));inside(s_macros->empty,s_list);
 s_macros->query[0]=0;rebuild_macro_list();lv_obj_update_layout(s_root);snapshot(s_root);ui_macros_hide();
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
 const char *page="files";
#else
 const char *page="macros";
#endif
 snprintf(path,sizeof(path),"%s/%s-theme%u.ppm",folder,page,theme_case);FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n%d %d\n255\n",a.x2-a.x1+1,a.y2-a.y1+1);for(int y=a.y1;y<=a.y2;y++)for(int x=a.x1;x<=a.x2;x++){uint16_t v=raster[y*1024+x];unsigned char c[3]={(unsigned char)(((v>>11)&31)*255/31),(unsigned char)(((v>>5)&63)*255/63),(unsigned char)((v&31)*255/31)};fwrite(c,1,3,f);}fclose(f);
}
int main(void){lv_init();lv_display_t *d=lv_display_create(1024,600);static uint8_t buffer[1024*50*2];lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);lv_display_set_buffers(d,buffer,NULL,sizeof(buffer),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(d,flush);
 for(unsigned custom=0;custom<2;custom++)for(unsigned theme=0;theme<5;theme++)for(unsigned density=0;density<3;density++)for(unsigned large=0;large<2;large++)for(unsigned w=0;w<3;w++){
  custom_tokens=custom;theme_case=theme;density_case=density;large_case=large;width_case=w;ui_theme_set_active((ui_theme_id_t)theme);ui_theme_set_density((ui_density_id_t)density);ui_theme_set_accessibility((ui_accessibility_t){.large_text=large});run((int[]){theme==UI_THEME_STUDIO_DARK?976:854,640,480}[w]);
 }
 lv_display_delete(d);lv_deinit();puts("PASS: responsive page bounds, wrapping, non-overlap and lifecycle across all five themes, three densities, both text sizes, three viewport widths and custom metric/profile overrides");}
