#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "custom_theme.h"
#include "printer_ui_controller.h"
#include "ui_filament_recovery.c"
static moonraker_state_t fixture;
static moonraker_filament_state_t sensors;
static uint32_t generation = 1;
static bool catalog_available = true, command_ok = true;
static char sent[96];
static int send_count, send_mode;
void ui_toast_show(ui_status_kind_t k,const char*t,const char*d){(void)k;(void)t;(void)d;}
static bool send(const char *s);
static void click(lv_obj_t *b);
bool custom_theme_color_override(uint32_t a,uint32_t b,uint32_t c,uint32_t *d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_metric_override(int32_t a,int32_t b,int32_t c,int32_t *d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_accent_override(uint8_t a,uint32_t *b){(void)a;(void)b;return false;}
bool custom_theme_surface_opacity(uint8_t *a){(void)a;return false;}
void moonraker_state_snapshot(moonraker_state_t *s){*s=fixture;}
void moonraker_filament_state_snapshot(moonraker_filament_state_t *s){*s=sensors;}
uint32_t moonraker_config_generation(void){return generation;}
void macro_controller_status(macro_controller_status_t *s){memset(s,0,sizeof(*s));s->discovered=catalog_available;s->count=catalog_available?4:0;}
bool macro_controller_get(size_t i,char *s,size_t n){if(i>=4)return false;snprintf(s,n,"%s",commands[i]);return true;}
static bool send(const char *s){
 send_count++;snprintf(sent,sizeof(sent),"%s",s);int mode=send_mode;send_mode=0;
 if(mode==1)click(buttons[0]);
 if(mode==2)ui_filament_recovery_close();
 if(mode==3)generation++;
 if(mode==4){ui_filament_recovery_close();ui_filament_recovery_show(send);}
 if(mode==5){ui_filament_recovery_close();generation++;ui_filament_recovery_show(send);}
 if(mode==6)lv_obj_delete(message_label);
 return command_ok;
}
static uint16_t raster[1024*600];
static void flush(lv_display_t *d,const lv_area_t *a,uint8_t *p){uint16_t *v=(uint16_t*)p;for(int y=a->y1;y<=a->y2;y++)for(int x=a->x1;x<=a->x2;x++)raster[y*1024+x]=*v++;lv_display_flush_ready(d);}
static void snapshot(int theme){const char *folder=getenv("RECOVERY_SCREENSHOTS");if(!folder)return;lv_refr_now(NULL);char path[512];snprintf(path,sizeof(path),"%s/theme%d.ppm",folder,theme);FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n1024 600\n255\n");for(int i=0;i<1024*600;i++){uint16_t v=raster[i];unsigned char c[3]={(v>>11)*255/31,((v>>5)&63)*255/63,(v&31)*255/31};fwrite(c,1,3,f);}fclose(f);}

static void fixture_reset(void){
 memset(&fixture,0,sizeof(fixture));fixture.moonraker_ok=fixture.live_data_ok=true;strcpy(fixture.printer_state,"paused");strcpy(fixture.homed_axes,"xyz");fixture.nozzle_temp=fixture.nozzle_target=205;
 memset(&sensors,0,sizeof(sensors));sensors.discovered=true;sensors.sensor_count=1;sensors.sensors[0].enabled=sensors.sensors[0].status_known=sensors.sensors[0].filament_detected=true;
 catalog_available=command_ok=true;generation=1;send_count=0;send_mode=0;action_pending=false;pending_generation=0;assert(!dispatching);
}
static void click(lv_obj_t *b){lv_obj_send_event(b,LV_EVENT_CLICKED,NULL);}
static void enabled(unsigned i,bool expected){refresh(NULL);assert(!lv_obj_has_state(buttons[i],LV_STATE_DISABLED)==expected);}
static unsigned timers(void){unsigned n=0;for(lv_timer_t*t=lv_timer_get_next(NULL);t;t=lv_timer_get_next(t))++n;return n;}
static void inside(lv_obj_t*c,lv_obj_t*p){lv_area_t a,b;lv_obj_get_coords(c,&a);lv_obj_get_coords(p,&b);if(!(a.x1>=b.x1&&a.x2<=b.x2&&a.y1>=b.y1&&a.y2<=b.y2)){fprintf(stderr,"outside %d,%d..%d,%d vs %d,%d..%d,%d label=%s\n",a.x1,a.y1,a.x2,a.y2,b.x1,b.y1,b.x2,b.y2,lv_obj_check_type(c,&lv_label_class)?lv_label_get_text(c):"object");assert(false);}}
static unsigned button_bounds(lv_obj_t *o){
 unsigned n=0;
 if(lv_obj_check_type(o,&lv_button_class)){
  n++;inside(o,lv_obj_get_parent(o));assert(lv_obj_get_height(o)>=48);
  lv_obj_t*l=lv_obj_get_child(o,0);inside(l,o);const lv_font_t*f=lv_obj_get_style_text_font(l,0);assert(lv_obj_get_height(l)==f->line_height);
  lv_point_t size;lv_text_get_size(&size,lv_label_get_text(l),f,0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);assert(size.x<=lv_obj_get_width(l));
 }
 for(unsigned i=0;i<lv_obj_get_child_count(o);i++)n+=button_bounds(lv_obj_get_child(o,i));
 return n;
}
static lv_point_t touch;static bool pressed;
static void read_touch(lv_indev_t*i,lv_indev_data_t*d){(void)i;d->point=touch;d->state=pressed?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED;}
static void tap(lv_indev_t*i,lv_obj_t*b){lv_area_t a;lv_obj_get_coords(b,&a);touch=(lv_point_t){(a.x1+a.x2)/2,(a.y1+a.y2)/2};pressed=true;lv_indev_read(i);pressed=false;lv_indev_read(i);}
static void lifetime_checks(void){
 unsigned baseline=timers();fixture_reset();ui_filament_recovery_show(send);
 lv_obj_t*first=popup;lv_timer_t*timer=refresh_timer;selected_temperature=235;ui_filament_recovery_show(send);assert(popup==first&&refresh_timer==timer&&selected_temperature==235);
 command_ok=false;click(buttons[0]);assert(send_count==1&&!action_pending&&strstr(lv_label_get_text(message_label),"retry")&&selected_temperature==235);
 command_ok=true;send_mode=1;click(buttons[0]);assert(send_count==2&&!strcmp(sent,"HMI_FILAMENT_HEAT TEMP=235")&&action_pending);
 click(buttons[0]);assert(send_count==2);ui_filament_recovery_close();ui_filament_recovery_show(send);click(buttons[0]);assert(send_count==2);lv_tick_inc(3000);click(buttons[0]);assert(send_count==3);ui_filament_recovery_close();
 for(int mode=2;mode<=6;mode++){
  fixture_reset();ui_filament_recovery_show(send);send_mode=mode;click(buttons[0]);assert(send_count==1&&!dispatching);
  if(mode==2)assert(!popup&&!refresh_timer&&!status_label&&!sensor_label&&!message_label&&!temperature_label&&!buttons[0]);
  if(mode==3){click(buttons[0]);assert(send_count==1);refresh(NULL);assert(strstr(lv_label_get_text(sensor_label),"unavailable"));}
  if(mode==4){assert(popup);click(buttons[0]);assert(send_count==1);lv_tick_inc(3000);click(buttons[0]);assert(send_count==2);}
  if(mode==5){assert(popup);click(buttons[0]);assert(send_count==2);}
  if(mode==6){assert(!message_label);refresh(NULL);}
  ui_filament_recovery_close();assert(timers()==baseline);
  refresh(NULL);action(NULL);temperature_change(NULL);assert(send_count>=1);
 }
 fixture_reset();ui_filament_recovery_show(send);
 lv_obj_delete(status_label);lv_obj_delete(sensor_label);lv_obj_delete(temperature_label);lv_obj_delete(buttons[2]);refresh(NULL);
 assert(!status_label&&!sensor_label&&!temperature_label&&!buttons[2]);ui_filament_recovery_close();assert(timers()==baseline);
 fixture_reset();for(int i=0;i<10;i++){ui_filament_recovery_show(send);lv_obj_delete(popup);assert(!popup&&!refresh_timer&&!message_label&&timers()==baseline);}
}
static void layout_checks(lv_display_t*d,lv_indev_t*pointer){
 for(int size=0;size<3;size++)for(int theme=0;theme<5;theme++)for(int density=0;density<3;density++)for(int large=0;large<2;large++){
  int width=(int[]){1024,640,480}[size];lv_display_set_resolution(d,width,size==2?400:600);
  ui_theme_set_active(theme);ui_theme_set_density(density);ui_theme_set_accessibility((ui_accessibility_t){.large_text=large});fixture_reset();ui_filament_recovery_show(send);lv_obj_update_layout(popup);
  inside(popup,lv_layer_top());lv_obj_t *body=ui_form_body(popup),*footer=lv_obj_get_child(popup,-1);inside(body,popup);inside(footer,popup);assert(button_bounds(popup)==8);
  lv_area_t before,after;lv_obj_get_coords(footer,&before);
  if(width==1024){
   if(lv_obj_get_scroll_bottom(body)>0)fprintf(stderr,"recovery overflow theme=%d density=%d large=%d bottom=%d\n",theme,density,large,(int)lv_obj_get_scroll_bottom(body));
   assert(lv_obj_get_scroll_bottom(body)<=0);inside(lv_obj_get_parent(buttons[4]),body);
   if(density==1&&large)snapshot(theme);
  }
  lv_obj_t*temperature_row=lv_obj_get_parent(temperature_label);lv_obj_scroll_to_view(temperature_row,LV_ANIM_OFF);lv_obj_update_layout(popup);tap(pointer,lv_obj_get_child(temperature_row,0));assert(selected_temperature==200);
  tap(pointer,lv_obj_get_child(temperature_row,2));assert(selected_temperature==205);
  selected_temperature=170;temperature_change(NULL);tap(pointer,lv_obj_get_child(temperature_row,0));assert(selected_temperature==170);
  selected_temperature=255;tap(pointer,lv_obj_get_child(temperature_row,2));assert(selected_temperature==255);
  /* Long/rejected feedback remains reachable while Close stays fixed. */
  command_ok=false;click(buttons[0]);lv_obj_update_layout(popup);
  if(width==1024)assert(lv_obj_get_scroll_bottom(body)<=0);
  lv_obj_scroll_to_y(body,10000,LV_ANIM_OFF);lv_obj_update_layout(popup);inside(lv_obj_get_parent(buttons[4]),body);
  lv_obj_get_coords(footer,&after);assert(!memcmp(&before,&after,sizeof(before)));tap(pointer,lv_obj_get_child(footer,0));assert(!popup&&!refresh_timer&&!temperature_label);
 }
}
int main(void){
 lv_init();lv_display_t *d=lv_display_create(1024,600);static uint8_t buffer[1024*20*4];lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);lv_display_set_buffers(d,buffer,NULL,sizeof(buffer),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(d,flush);
 for(int theme=0;theme<5;theme++)for(int density=0;density<3;density++)for(int large=0;large<2;large++){
  ui_theme_set_active(theme);ui_theme_set_density(density);ui_accessibility_t a={.large_text=large};ui_theme_set_accessibility(a);
  fixture_reset();ui_filament_recovery_show(send);lv_obj_update_layout(popup);if(density==1&&large==1)snapshot(theme);
  for(int i=0;i<5;i++){assert(lv_obj_get_width(buttons[i])>=180);assert(lv_obj_get_height(buttons[i])>=48);enabled(i,true);}
  click(buttons[0]);assert(strcmp(sent,"HMI_FILAMENT_HEAT TEMP=205")==0);enabled(4,false);lv_tick_inc(3000);enabled(4,true);
  selected_temperature=240;click(buttons[0]);assert(strcmp(sent,"HMI_FILAMENT_HEAT TEMP=240")==0);lv_tick_inc(3000);
  fixture.nozzle_temp=100;enabled(1,false);enabled(2,false);enabled(3,false);enabled(0,true);enabled(4,true);
  fixture.nozzle_temp=205;sensors.sensors[0].filament_detected=false;enabled(4,false);enabled(1,true);int before=send_count;click(buttons[4]);assert(send_count==before);
  sensors.sensors[0].filament_detected=true;click(buttons[1]);assert(strcmp(sent,"HMI_FILAMENT_UNLOAD")==0);lv_tick_inc(3000);enabled(2,false);lv_tick_inc(6000);enabled(2,true);
  for(unsigned i=2;i<5;i++){click(buttons[i]);assert(!strcmp(sent,commands[i]));int count=send_count;click(buttons[i]);assert(send_count==count);lv_tick_inc(9000);}
  catalog_available=false;enabled(0,false);enabled(1,false);enabled(4,true);catalog_available=true;
  fixture.live_data_ok=false;for(int i=0;i<5;i++)enabled(i,false);fixture.live_data_ok=true;
  sensors.truncated=true;enabled(4,false);sensors.truncated=false;sensors.sensors[0].status_known=false;enabled(4,false);sensors.sensors[0].status_known=true;
  generation++;for(int i=0;i<5;i++)enabled(i,false);ui_filament_recovery_close();assert(!popup&&!refresh_timer);
  lifetime_checks();
 }
 fixture_reset();ui_filament_recovery_show(send);command_ok=false;click(buttons[0]);assert(!action_pending);assert(strstr(lv_label_get_text(message_label),"could not"));ui_filament_recovery_close();
 fixture_reset();printer_ui_controller_init(send,NULL,NULL,NULL);lv_obj_t *b=lv_button_create(lv_screen_active());ui_printer_actions_t actions={.resume=b};printer_ui_controller_bind_actions(&actions);lv_obj_add_event_cb(b,printer_ui_controller_command_event_cb,LV_EVENT_CLICKED,"RESUME");click(b);assert(popup&&send_count==0);ui_filament_recovery_close();lv_obj_delete(b);
 fixture_reset();ui_filament_recovery_show(send);strcpy(fixture.homed_axes,"xy");enabled(1,false);enabled(4,false);ui_filament_recovery_close();
 fixture_reset();ui_filament_recovery_show(send);strcpy(fixture.printer_state,"printing");int before=send_count;click(buttons[4]);assert(send_count==before);ui_filament_recovery_close();
 lv_indev_t*pointer=lv_indev_create();lv_indev_set_type(pointer,LV_INDEV_TYPE_POINTER);lv_indev_set_read_cb(pointer,read_touch);layout_checks(d,pointer);
 puts("PASS: pinned Close and native-panel no-scroll layout; rejected retry, duplicate callbacks, reentrant deletion/reopen, retired fields/timers, recovery events, temperature, macro discovery, sensors, cooldown, printer/offline fencing, all themes/densities/accessibility");return 0;
}
