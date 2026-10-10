#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "custom_theme.h"
#include "ui_dashboard_page.h"
#include "ui_shell.h"
#include "ui_global_estop.h"
#include "ui_theme_studio.h"
/* Inspect actual component handles; service/transport calls are stubbed below. */
#include "ui_status_banner.c"
#include "ui_active_print.c"
#include "ui_machine_status.c"
#include "ui_command_bar.c"
bool custom_theme_color_override(uint32_t a,uint32_t b,uint32_t c,uint32_t*d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_metric_override(int32_t a,int32_t b,int32_t c,int32_t*d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_accent_override(uint8_t a,uint32_t*b){(void)a;(void)b;return false;}
bool custom_theme_surface_opacity(uint8_t*a){(void)a;return false;}
const ui_dashboard_layout_profile_t *custom_theme_dashboard_profile(void){return NULL;}
const ui_page_layout_profile_t *custom_theme_page_profile(void){return NULL;}
static char last_action[32];static int lightboxes,selected_page;
void ui_command_bar_action(const char *s){snprintf(last_action,sizeof(last_action),"%s",s);}
void ui_shell_page_action(ui_shell_page_t page){selected_page=page;}
void ui_preview_lightbox_show_file_object(lv_obj_t *o,const char *f){(void)o;(void)f;lightboxes++;}
static bool sent(const char *gcode){snprintf(last_action,sizeof(last_action),"%s",gcode);return true;}
moonraker_filament_status_t moonraker_filament_state_status(const moonraker_filament_state_t*s,size_t*p,size_t*e){(void)s;*p=1;*e=1;return MOONRAKER_FILAMENT_READY;}
static lv_point_t touch_point;
static bool pressed;
static void read_pointer(lv_indev_t *i,lv_indev_data_t *d){(void)i;d->point=touch_point;d->state=pressed?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED;}
static void tap(lv_indev_t *i,int x,int y){touch_point=(lv_point_t){x,y};pressed=true;lv_indev_read(i);pressed=false;lv_indev_read(i);}
static uint16_t raster[1024*600];
static void flush(lv_display_t*d,const lv_area_t*a,uint8_t*p){uint16_t*v=(uint16_t*)p;for(int y=a->y1;y<=a->y2;y++)for(int x=a->x1;x<=a->x2;x++)raster[y*1024+x]=*v++;lv_display_flush_ready(d);}
static void screenshot(const char*name){const char*folder=getenv("STUDIO_SCREENSHOTS");if(!folder)return;lv_refr_now(NULL);char path[512];snprintf(path,sizeof(path),"%s/%s.ppm",folder,name);FILE*f=fopen(path,"wb");assert(f);fprintf(f,"P6\n1024 600\n255\n");for(int i=0;i<1024*600;i++){uint16_t v=raster[i];unsigned char c[3]={(v>>11)*255/31,((v>>5)&63)*255/63,(v&31)*255/31};fwrite(c,1,3,f);}fclose(f);}
static void inside(lv_obj_t*c,lv_obj_t*p){lv_area_t a,b;lv_obj_get_coords(c,&a);lv_obj_get_coords(p,&b);if(a.x1<b.x1||a.x2>b.x2||a.y1<b.y1||a.y2>b.y2){fprintf(stderr,"Outside: %s (%d %d %d %d) vs (%d %d %d %d)\n",lv_obj_check_type(c,&lv_label_class)?lv_label_get_text(c):"object",a.x1,a.y1,a.x2,a.y2,b.x1,b.y1,b.x2,b.y2);abort();}}
static void labels(lv_obj_t*o){for(unsigned i=0;i<lv_obj_get_child_count(o);i++){lv_obj_t*c=lv_obj_get_child(o,i);if(lv_obj_has_flag(c,LV_OBJ_FLAG_HIDDEN))continue;inside(c,o);if(lv_obj_check_type(c,&lv_label_class)){const lv_font_t*f=lv_obj_get_style_text_font(c,0);if(!strchr(lv_label_get_text(c),'\n')) assert(lv_obj_get_height(c)<=f->line_height+lv_obj_get_style_pad_top(c,0)+lv_obj_get_style_pad_bottom(c,0));}else labels(c);}}
static lv_obj_t *find(lv_obj_t*o,const char*t){if(lv_obj_check_type(o,&lv_label_class)&&!strcmp(lv_label_get_text(o),t))return o;for(unsigned i=0;i<lv_obj_get_child_count(o);i++){lv_obj_t*c=find(lv_obj_get_child(o,i),t);if(c)return c;}return NULL;}
static void click(lv_obj_t*o){lv_obj_send_event(o,LV_EVENT_CLICKED,NULL);}
int main(void){
 lv_init();lv_display_t*d=lv_display_create(1024,600);static uint8_t buffer[1024*60*2];lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);lv_display_set_buffers(d,buffer,NULL,sizeof(buffer),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(d,flush);
 lv_indev_t *pointer=lv_indev_create();lv_indev_set_type(pointer,LV_INDEV_TYPE_POINTER);lv_indev_set_read_cb(pointer,read_pointer);
 assert(ui_global_estop_init(sent));
 for(unsigned large=0;large<2;large++) for(unsigned density=0;density<3;density++){
  ui_theme_set_active(UI_THEME_STUDIO_DARK);ui_theme_set_density(density);ui_theme_set_accessibility((ui_accessibility_t){.large_text=large});ui_apply_root_style(lv_screen_active());
  
  ui_shell_create();ui_shell_set_active_printer_name("Sermoon D1");ui_shell_create_nav();
  
  ui_dashboard_page_t page=ui_dashboard_page_create(lv_screen_active());
  
  ui_status_banner_set(page.banner_host,"PRINTING","Spool holder.gcode","ETA 3:42 PM","64%");
  ui_active_print_set(page.active_print_host,"171 / 267","01:06","REM 00:38");
  
  ui_machine_status_set(page.machine_status_host,"210.0 / 210.0 C","60.0 / 60.0 C","45.0 C","18.0 %RH","125 mm/s","8.3 mm3/s","65%");
  ui_machine_status_set_active_hotend(page.machine_status_host,"Nozzle","210.0 / 210.0 C");
  ui_machine_status_set_connection(page.machine_status_host,true);ui_machine_status_set_filament(page.machine_status_host,true,NULL);
  
  ui_command_bar_update("printing",true);lv_obj_update_layout(lv_screen_active());
  assert(lv_obj_get_width(page.root)==976&&lv_obj_get_height(page.root)==424);labels(page.root);
  /* Three action routes retain their application contracts; paused swaps resume. */
  click(s_pause_button);assert(!strcmp(last_action,"PAUSE"));click(s_cancel_button);assert(!strcmp(last_action,"CANCEL_PRINT"));click(s_object_button);assert(!strcmp(last_action,"CANCEL_OBJECT"));
  ui_command_bar_update("paused",true);assert(lv_obj_has_flag(s_pause_button,LV_OBJ_FLAG_HIDDEN));assert(!lv_obj_has_flag(s_resume_button,LV_OBJ_FLAG_HIDDEN));click(s_resume_button);assert(!strcmp(last_action,"RESUME"));ui_command_bar_update("printing",true);
  for(int i=0;i<8;i++){const char*names[]={"Dashboard","Printer","Files","Camera","Tools","Console","Drybox","Settings"};lv_obj_t*l=find(lv_screen_active(),names[i]);assert(l);assert(lv_obj_get_height(l)<=lv_obj_get_style_text_font(l,0)->line_height);click(lv_obj_get_parent(l));assert(selected_page==i);}ui_shell_set_active_nav(0);
  lv_obj_t*estop=find(lv_screen_active(),LV_SYMBOL_WARNING " E-STOP");assert(estop);labels(lv_obj_get_parent(lv_obj_get_parent(estop)));
  
  /* Hit-test through the transparent status overlay into the real preview. */
  static uint16_t tiny[4];s_active_print_thumb_canvas_buf=tiny;
  ui_active_print_thumb_show_canvas_from_buffer(page.active_print_host,2,2,"fixture.gcode");
  int before=lightboxes;tap(pointer,160,300);assert(lightboxes==before+1);
  ui_active_print_thumb_delete_canvas();ui_active_print_thumb_set_placeholder(page.active_print_host,"PRINT\nTHUMBNAIL");
  if(density==1)screenshot(large?"studio-dark-large":"studio-dark");
  
  click(lv_obj_get_parent(estop));assert(!strcmp(last_action,"M112"));
  lv_obj_t *close=find(lv_layer_top(),LV_SYMBOL_CLOSE " CLOSE");assert(close);click(lv_obj_get_parent(close));
  /* Unknown/capability-less values remain explicit, never fake zero. */
  ui_machine_status_set(page.machine_status_host,"-- / -- C","N/A","N/A","N/A","-- mm/s","-- mm3/s","N/A");machine_status_ctx_t*m=lv_obj_get_user_data(page.machine_status_host);assert(!strcmp(lv_label_get_text(m->nozzle),"--°"));assert(!strcmp(lv_label_get_text(m->bed),"N/A"));
  ui_command_bar_update("standby",false);assert(lv_obj_has_state(s_pause_button,LV_STATE_DISABLED)&&lv_obj_has_state(s_cancel_button,LV_STATE_DISABLED));
  /* Overlay/root styles must not resize small nested empty-state widgets. */
  lv_obj_t*nested=lv_obj_create(page.root);lv_obj_set_size(nested,400,200);ui_apply_root_style(nested);assert(lv_obj_get_style_width(nested,0)==400);lv_obj_delete(nested);
  
  ui_dashboard_page_destroy(&page);ui_shell_destroy();assert(!find(lv_screen_active(),LV_SYMBOL_WARNING " E-STOP"));
 }
 /* Page roots are fixed; only explicit Files/Settings content may scroll. */
 ui_theme_set_active(UI_THEME_STUDIO_DARK);lv_obj_t*aux=lv_obj_create(lv_screen_active());lv_obj_set_size(aux,854,528);ui_apply_surface_role(aux,UI_SURFACE_PAGE_DEEP);lv_obj_update_layout(aux);assert(lv_obj_get_height(aux)==424);assert(!lv_obj_has_flag(aux,LV_OBJ_FLAG_SCROLLABLE));lv_obj_delete(aux);

 ui_theme_set_active(UI_THEME_OPERATOR);ui_shell_create();ui_shell_create_nav();assert(find(lv_screen_active(),LV_SYMBOL_WARNING " E-STOP"));ui_shell_destroy();
 puts("PASS: native STUDIO Dashboard bounds/text across densities and large text, thermal unknowns, print action routes/states, eight navigation routes, fixed page roots, E-stop recreation and repeated teardown");return 0;
}
