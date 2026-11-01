#include "printer_simulation_catalog.h"
/* Linux-only LVGL runtime. UI implementation is compiled from main/, never copied.
 * The only machine backend is the in-memory fixture below. No transport APIs. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <time.h>
#include "lvgl.h"
#include "src/misc/lv_event_private.h"
#include "console_controller.h"
#include "custom_theme.h"
#include "moonraker.h"
#include "printer_simulation.h"
#include "ui_theme.h"
#include "ui_printer_popups.h"
#include "ui_printer_toolhead.h"
#include "ui_drybox.h"
#include "ui_toast.h"
#include "ui_settings.h"
#include "operator_event_log.h"
#include "timezone_config.h"
#include "theme_manager.h"
#include "moonraker_config_controller.h"
#include "ui_printer_chooser.h"
#include "ui_shell.h"
#include "ui_network.h"
#include "ui_telemetry.h"
#include "telemetry_history.h"
void host_devices_refresh(void);
void host_devices_fixture(int);
void host_devices_info(void);
void host_macros_refresh(void);
void host_macros_fixture(int);
void host_macros_info(void);
void host_mesh_refresh(void);
void host_mesh_fixture(int);
void host_mesh_info(void);
void host_calibration_refresh(void);
void host_calibration_fixture(int,moonraker_state_t *);
void host_calibration_info(void);
void host_calibration_home(void);
void host_calibration_step(double);
bool host_mesh_escape(void);
bool host_calibration_escape(void);
bool host_mesh_interactive(lv_obj_t *);
#include "routes.h"
void host_tuning_show(bool (*send)(const char *));
void host_tuning_close(void);
bool host_camera_escape(void);
bool host_camera_foreground_escape(lv_obj_t *);
bool host_lightbox_escape(lv_obj_t *);
bool host_camera_inert_image(lv_obj_t *);
bool host_profiles_escape(void);
void host_page_show(const char *kind,bool (*send)(const char *));

void host_page_close(void);
const char *host_page_take_event(void);
void host_page_clear_event(void);
void host_lighting_refresh(void);
void host_lighting_fixture(int);
void host_ota_fixture(int);
void host_ota_show(void);
void host_setup_show(void);
bool host_setup_escape(void);
static bool page_mode;
static bool simulation_running;
static int64_t simulation_time;
static int64_t host_time_offset_us=600000000LL;
void host_telemetry_seed(const moonraker_state_t *,int64_t);
void host_telemetry_fixture(moonraker_state_t *,int);
int64_t esp_timer_get_time(void){static int64_t start;struct timespec now;clock_gettime(CLOCK_MONOTONIC,&now);int64_t us=(int64_t)now.tv_sec*1000000+now.tv_nsec/1000;if(!start)start=us;return us-start+host_time_offset_us;}
void host_profiles_init(void);
void host_profiles_fixture(int,int);
static moonraker_state_t machine;
void host_calibration_home(void){strcpy(machine.homed_axes,"xyz");}
void host_calibration_step(double value){machine.toolhead_z+=value;}
bool host_network_online(void);
void host_network_connection(bool online){const char *host=moonraker_config_host();machine.moonraker_ok=machine.live_data_ok=online&&!strstr(host,"fail")&&!strstr(host,"unavailable");}
static bool excluded_objects[2],sensor_enabled=true,filament_present=true,drybox_send_fail;
static struct {moonraker_state_t machine;moonraker_profile_t endpoint;bool excluded[2],sensor,present,valid;} profile_states[MOONRAKER_CONFIG_MAX_PROFILES];
static int observed_profile;
static moonraker_state_t idle_machine(void){moonraker_state_t state=machine;strcpy(state.printer_state,"standby");state.printer_file[0]=0;state.progress=state.print_duration=0;state.current_layer=state.total_layer=0;state.nozzle_temp=28;state.bed_temp=25;state.nozzle_target=state.bed_target=state.part_fan_speed=0;state.toolhead_x=state.toolhead_y=state.toolhead_z=state.z_offset=0;state.homed_axes[0]=0;state.speed_factor=state.flow_factor=100;state.drybox_active_program=state.drybox_selected_program=0;state.heater_on=false;state.heater_target=state.drybox_fan_speed=0;return state;}
void host_profile_state(int p,moonraker_state_t *out){if(p==moonraker_config_active_profile_index())*out=machine;else if(p>=0&&p<MOONRAKER_CONFIG_MAX_PROFILES&&profile_states[p].valid)*out=profile_states[p].machine;else *out=idle_machine();}
void host_profile_changed(void){
 int p=moonraker_config_active_profile_index();const moonraker_profile_t *endpoint=moonraker_config_profile(p);
 if(!endpoint)return;
 if(p!=observed_profile){profile_states[observed_profile].machine=machine;profile_states[observed_profile].sensor=sensor_enabled;profile_states[observed_profile].present=filament_present;memcpy(profile_states[observed_profile].excluded,excluded_objects,sizeof(excluded_objects));profile_states[observed_profile].valid=true;}
 bool same=profile_states[p].endpoint.configured&&!strcmp(profile_states[p].endpoint.host,endpoint->host)&&profile_states[p].endpoint.port==endpoint->port&&profile_states[p].endpoint.secure_transport==endpoint->secure_transport;
 if(p!=observed_profile||!same){
  telemetry_history_reset();
  if(same&&profile_states[p].valid){machine=profile_states[p].machine;sensor_enabled=profile_states[p].sensor;filament_present=profile_states[p].present;memcpy(excluded_objects,profile_states[p].excluded,sizeof(excluded_objects));}
  else{machine=idle_machine();sensor_enabled=filament_present=true;memset(excluded_objects,0,sizeof(excluded_objects));}
 }
 observed_profile=p;profile_states[p].endpoint=*endpoint;host_network_connection(host_network_online());
 ui_shell_set_active_printer_name(endpoint->name);ui_network_set_port(endpoint->port);
}

static uint32_t raster[1024*600],dimmed_raster[1024*600];
int host_display_percent(void);
bool host_settings_wake_target(lv_obj_t *);
void host_storage_fail(bool);
void host_display_fail(bool);
void host_network_refresh(void);
void host_network_fixture(int,int);

static uint8_t draw_buffer[1024*50*4];
static lv_indev_t *pointer,*keyboard;
static lv_indev_data_t pointer_data;
static uint32_t key_code;
static bool key_pressed;
static char last_command[640];
static bool palette_enabled;
static uint32_t palette[64];
static int metrics[3]={16,24,16};
static bool metric_enabled[3];
/* Firmware page adapters and authoring layout hooks. */
void host_printer_refresh(void);
void host_dashboard_refresh(void);
const ui_dashboard_layout_profile_t *custom_theme_dashboard_profile(void){return NULL;}
const ui_page_layout_profile_t *custom_theme_page_profile(void){return NULL;}

bool host_demo_scenario(const char *scenario){
 bool ok=printer_simulation_scenario(&machine,scenario);
 if(ok){if(!strcmp(scenario,"idle") || !strcmp(scenario,"printing") || !strcmp(scenario,"paused"))excluded_objects[0]=excluded_objects[1]=false;simulation_running=true;simulation_time=esp_timer_get_time();}return ok;
}
void host_demo_fixture(int value);
void moonraker_state_snapshot(moonraker_state_t *out){*out=machine;}
#include "filament_status.inc"
void moonraker_filament_state_snapshot(moonraker_filament_state_t *out){*out=(moonraker_filament_state_t){.discovered=true,.sensor_count=1,.total_count=1};snprintf(out->sensors[0].object_name,sizeof(out->sensors[0].object_name),"filament_switch_sensor runout");out->sensors[0].status_known=true;out->sensors[0].filament_detected=filament_present;out->sensors[0].enabled=sensor_enabled;}
bool moonraker_exclude_objects_available(void){return !excluded_objects[0] || !excluded_objects[1];}
void moonraker_exclude_state_snapshot(moonraker_exclude_state_t *out){printer_simulation_objects(out,excluded_objects);}

bool custom_theme_color_override(uint32_t a,uint32_t b,uint32_t c,uint32_t *out){if(!palette_enabled)return false;for(unsigned i=0;i<ROUTE_COUNT;i++)if(routes[i].a==a&&routes[i].b==b&&routes[i].c==c){*out=palette[i];return true;}return false;}
bool custom_theme_metric_override(int32_t a,int32_t b,int32_t c,int32_t *out){const int route[3][3]={{12,20,22},{10,18,20},{10,20,24}};for(int i=0;i<3;i++)if(metric_enabled[i]&&route[i][0]==a&&route[i][1]==b&&route[i][2]==c){*out=metrics[i];return true;}return false;}
bool custom_theme_accent_override(uint8_t tone,uint32_t *out){if(!palette_enabled)return false;const char *name=tone>=2?"accent_bright":"accent";for(unsigned i=0;i<ROUTE_COUNT;i++)if(!strcmp(routes[i].name,name)){*out=palette[i];return true;}return false;}
bool custom_theme_surface_opacity(uint8_t *out){(void)out;return false;}
static void send_command(const char *command){
 snprintf(last_command,sizeof(last_command),"%s",command);
 if(printer_simulation_is_macro(command)){
  if(printer_simulation_macro_apply(&machine,command,&filament_present) && printer_simulation_macro_starts_job(command))excluded_objects[0]=excluded_objects[1]=false;
 }else (void)printer_simulation_apply(&machine,excluded_objects,&sensor_enabled,command);
}

#ifdef PRINTERHMI_DEMO_RUNTIME
bool demo_printer_command(const char *command){
 if(printer_simulation_is_macro(command))return printer_simulation_macro_apply(&machine,command,&filament_present);
 return printer_simulation_apply(&machine,excluded_objects,&sensor_enabled,command);
}
bool demo_printer_calibration_step(double amount){host_calibration_step(amount);return true;}
void host_record_calibration_command(const char *command){snprintf(last_command,sizeof(last_command),"%s",command);}
#endif
void host_lighting_command(const char *command){send_command(command);}
static bool checked_send(const char *command){if(printer_simulation_is_macro(command)){moonraker_state_t copy=machine;bool present=filament_present;if(!printer_simulation_macro_apply(&copy,command,&present))return false;}if(drybox_send_fail && !strncmp(command,"DRY_",4))return false;send_command(command);console_controller_add(CONSOLE_ENTRY_RESPONSE,"ok (simulated)");return true;}
static void flush(lv_display_t *display,const lv_area_t *area,uint8_t *pixels){size_t w=(size_t)(area->x2-area->x1+1);for(int y=area->y1;y<=area->y2;y++)memcpy(raster+y*1024+area->x1,pixels+(size_t)(y-area->y1)*w*4,w*4);lv_display_flush_ready(display);}
static void read_pointer(lv_indev_t *device,lv_indev_data_t *data){(void)device;*data=pointer_data;}
static lv_obj_t *find_class(lv_obj_t *root,const lv_obj_class_t *cls){if(lv_obj_has_flag(root,LV_OBJ_FLAG_HIDDEN))return NULL;for(unsigned i=lv_obj_get_child_count(root);i>0;i--){lv_obj_t *child=lv_obj_get_child(root,i-1);lv_obj_t *found=find_class(child,cls);if(found)return found;}return lv_obj_check_type(root,cls)?root:NULL;}
/* Use the same layer order and hit test as LVGL pointer dispatch. Never press
 * or focus a widget while inspecting hover. Event filters distinguish generic
 * clickable containers from actual callbacks (for example temperature cards). */
static lv_obj_t *hit_object(int x,int y){
 lv_point_t point={x,y};lv_obj_update_layout(lv_screen_active());
 lv_obj_t *obj=lv_indev_search_obj(lv_layer_sys(),&point);
 if(!obj)obj=lv_indev_search_obj(lv_layer_top(),&point);
 if(!obj)obj=lv_indev_search_obj(lv_screen_active(),&point);
 return obj;
}
static void wheel_scroll(int x,int y,int delta){
 for(lv_obj_t *obj=hit_object(x,y);obj;obj=lv_obj_get_parent(obj)){
  if(lv_obj_has_state(obj,LV_STATE_DISABLED))return;
  if(lv_obj_has_flag(obj,LV_OBJ_FLAG_SCROLLABLE) && (lv_obj_get_scroll_dir(obj)&LV_DIR_VER) &&
    (delta>0?lv_obj_get_scroll_bottom(obj)>0:lv_obj_get_scroll_top(obj)>0)){
   int available=delta>0?lv_obj_get_scroll_bottom(obj):lv_obj_get_scroll_top(obj);
   int movement=delta>0?(delta<available?delta:available):(delta>-available?delta:-available);
   lv_obj_scroll_by(obj,0,-movement,LV_ANIM_OFF);return;
  }
  if(!lv_obj_has_flag(obj,LV_OBJ_FLAG_SCROLL_CHAIN_VER))return;
 }
}
static const char *hover_cursor(int x,int y){
 lv_obj_t *obj=hit_object(x,y);if(obj&&host_settings_wake_target(obj))return "pointer";if(!obj||host_camera_inert_image(obj))return "default";
 for(lv_obj_t *p=obj;p;p=lv_obj_get_parent(p))if(lv_obj_has_state(p,LV_STATE_DISABLED))return "default";
 if(host_mesh_interactive(obj))return pointer_data.state==LV_INDEV_STATE_PRESSED?"grabbing":"grab";
 if(lv_obj_has_class(obj,&lv_textarea_class))return "text";
 const lv_obj_class_t *classes[]={&lv_button_class,&lv_slider_class,&lv_dropdown_class,&lv_dropdownlist_class,&lv_buttonmatrix_class,&lv_switch_class,&lv_checkbox_class};
 for(unsigned i=0;i<sizeof(classes)/sizeof(classes[0]);i++)if(lv_obj_has_class(obj,classes[i]))return "pointer";
 for(lv_obj_t *p=obj;p;){
  for(unsigned i=0;i<lv_obj_get_event_count(p);i++){lv_event_dsc_t *d=lv_obj_get_event_dsc(p,i);if(d && d->cb && (d->filter & ~LV_EVENT_PREPROCESS)==LV_EVENT_CLICKED)return "pointer";}
  if(!lv_obj_has_flag(p,LV_OBJ_FLAG_EVENT_BUBBLE))break;
  p=lv_obj_get_parent(p);
 }
 return "default";
}
static void read_key(lv_indev_t *device,lv_indev_data_t *data){(void)device;data->key=key_code;data->state=key_pressed?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED;}
static void tick(unsigned amount){static uint64_t previous;struct timespec now;clock_gettime(CLOCK_MONOTONIC,&now);uint64_t ms=(uint64_t)now.tv_sec*1000+now.tv_nsec/1000000;unsigned delta=previous?(unsigned)(ms-previous):amount;previous=ms;lv_tick_inc(delta);lv_timer_handler();lv_refr_now(NULL);}
bool host_settings_take_restart(void);
void host_settings_restart(void);
void host_page_emit(const char *);
static void frame(void){
 int64_t now=esp_timer_get_time();
 if(simulation_running && simulation_time && now>=simulation_time)printer_simulation_advance(&machine,(now-simulation_time)/1000000.0);
 simulation_time=now;

 if(host_settings_take_restart()){host_settings_restart();host_page_show("dashboard-page",checked_send);host_page_emit("@nav:dashboard");}
 host_lighting_refresh();host_devices_refresh();host_macros_refresh();host_mesh_refresh();host_calibration_refresh();ui_telemetry_refresh(&machine,esp_timer_get_time());host_printer_refresh();host_dashboard_refresh();ui_drybox_refresh();ui_settings_refresh();host_network_refresh();ui_printer_chooser_refresh();tick(30);
 unsigned modal=lv_obj_get_child_count(lv_layer_top());if(page_mode){modal++;if(host_page_take_event()[0]){snprintf(last_command,sizeof(last_command),"%s",host_page_take_event());host_page_clear_event();}}
 /* Header precedes exactly 1024*600 little-endian BGRA pixels. */
 printf("FRAME %u %.0f %.0f %.0f %zu %.4f %.4f %.4f %.4f %.0f %.0f %s %.2f %zu %zu\n",modal,machine.nozzle_target,machine.bed_target,machine.part_fan_speed,strlen(last_command),machine.toolhead_x,machine.toolhead_y,machine.toolhead_z,machine.z_offset,machine.speed_factor,machine.flow_factor,machine.printer_state,machine.progress*100,strlen(machine.printer_file),strlen(moonraker_config_active_profile_name()));
 fwrite(last_command,1,strlen(last_command),stdout);fwrite(machine.printer_file,1,strlen(machine.printer_file),stdout);fwrite(moonraker_config_active_profile_name(),1,strlen(moonraker_config_active_profile_name()),stdout);const uint32_t *pixels=raster;int brightness=host_display_percent();
 if(brightness<100){for(size_t i=0;i<1024U*600;i++){uint32_t p=raster[i];dimmed_raster[i]=(p&0xff000000U)|(((p>>16&255)*brightness/100)<<16)|(((p>>8&255)*brightness/100)<<8)|((p&255)*brightness/100);}pixels=dimmed_raster;}
 fwrite(pixels,1,sizeof(raster),stdout);fflush(stdout);last_command[0]=0;
}
static void close_all(void){host_page_close();page_mode=false;lv_obj_set_style_bg_opa(lv_screen_active(),LV_OPA_TRANSP,0);host_tuning_close();ui_printer_toolhead_close();ui_printer_popups_close_all();lv_obj_clean(lv_layer_top());pointer_data.state=LV_INDEV_STATE_RELEASED;lv_indev_reset(pointer,NULL);lv_indev_read(pointer);}
static void collect_text(lv_obj_t *obj,char *buffer,size_t size,size_t *used){
 if(lv_obj_has_flag(obj,LV_OBJ_FLAG_HIDDEN))return;
 if(lv_obj_has_class(obj,&lv_label_class)){const char *text=lv_label_get_text(obj);size_t length=strlen(text);if(length+1<size-*used){memcpy(buffer+*used,text,length);*used+=length;buffer[(*used)++]='\n';}}
 for(unsigned i=0;i<lv_obj_get_child_count(obj);i++)collect_text(lv_obj_get_child(obj,i),buffer,size,used);
}
/* Read-only fixture introspection locates real visible buttons for pointer tests. */
static lv_obj_t *button_named(lv_obj_t *root,const char *name){
 if(lv_obj_has_flag(root,LV_OBJ_FLAG_HIDDEN))return NULL;
 if(lv_obj_has_class(root,&lv_button_class))for(unsigned i=0;i<lv_obj_get_child_count(root);i++){
  lv_obj_t *child=lv_obj_get_child(root,i);if(!lv_obj_has_class(child,&lv_label_class))continue;
  const char *label=lv_label_get_text(child);size_t length=strlen(label),n=strlen(name);if(!strcmp(label,name)||(length>n&&label[length-n-1]==' '&&!strcmp(label+length-n,name)))return root;
 }
 for(unsigned i=lv_obj_get_child_count(root);i>0;i--){lv_obj_t *found=button_named(lv_obj_get_child(root,i-1),name);if(found)return found;}
 return NULL;
}
/* Desktop Escape follows the foreground owner's actual dismissal callback.
 * Raw deletion can strand service timers, pending commands or private owners. */
static lv_obj_t *front_child(lv_obj_t *layer){
 for(unsigned i=lv_obj_get_child_count(layer);i>0;i--){lv_obj_t *obj=lv_obj_get_child(layer,i-1);if(!lv_obj_has_flag(obj,LV_OBJ_FLAG_HIDDEN)&&lv_obj_has_flag(obj,LV_OBJ_FLAG_CLICKABLE))return obj;}
 return NULL;
}
static lv_obj_t *dismiss_button(lv_obj_t *root){
 if(lv_obj_has_flag(root,LV_OBJ_FLAG_HIDDEN)||lv_obj_has_state(root,LV_STATE_DISABLED))return NULL;
 if(lv_obj_has_class(root,&lv_button_class)){
  static const char *names[]={"Close","Back","Cancel","Keep","Keep camera","Dismiss"};
  for(unsigned i=0;i<lv_obj_get_child_count(root);i++){
   lv_obj_t *label=lv_obj_get_child(root,i);if(!lv_obj_has_class(label,&lv_label_class))continue;
   const char *text=lv_label_get_text(label);size_t length=strlen(text);
   for(unsigned j=0;j<sizeof(names)/sizeof(names[0]);j++){size_t n=strlen(names[j]);if(length>=n&&!strcasecmp(text+length-n,names[j])&&(length==n||text[length-n-1]==' '))return root;}
  }
 }
 for(unsigned i=lv_obj_get_child_count(root);i>0;i--){lv_obj_t *found=dismiss_button(lv_obj_get_child(root,i-1));if(found)return found;}
 return NULL;
}
static lv_obj_t *dropdown_for_list(lv_obj_t *root,lv_obj_t *list){
 if(lv_obj_has_class(root,&lv_dropdown_class)&&lv_dropdown_is_open(root)&&lv_dropdown_get_list(root)==list)return root;
 for(unsigned i=0;i<lv_obj_get_child_count(root);i++){lv_obj_t *found=dropdown_for_list(lv_obj_get_child(root,i),list);if(found)return found;}
 return NULL;
}
static void handle_key(uint32_t key){
 lv_obj_t *top=front_child(lv_layer_top());
 lv_obj_t *screen=front_child(lv_screen_active());
 lv_obj_t *foreground=top?top:screen;
 if(!foreground)return;
 if(key==LV_KEY_ESC && lv_obj_has_class(foreground,&lv_dropdownlist_class)){
  lv_obj_t *dropdown=dropdown_for_list(lv_screen_active(),foreground);
  if(!dropdown)dropdown=dropdown_for_list(lv_layer_top(),foreground);
  if(dropdown)lv_obj_send_event(dropdown,LV_EVENT_KEY,&key);
  return;
 }
 lv_obj_t *kb=find_class(foreground,&lv_keyboard_class);
 lv_obj_t *input=kb?lv_keyboard_get_textarea(kb):NULL;
 if(key==LV_KEY_ENTER && kb){lv_obj_send_event(kb,LV_EVENT_READY,NULL);return;}
 if(key==LV_KEY_ESC){
  if(kb){lv_obj_send_event(kb,LV_EVENT_CANCEL,NULL);return;}
  if(top){
   if(host_lightbox_escape(top)||host_camera_foreground_escape(top))return;
   lv_obj_t *button=dismiss_button(top);
   if(button)lv_obj_send_event(button,LV_EVENT_CLICKED,NULL);
   /* A non-dismissible foreground never grants access to an older dialog. */
   return;
  }
  (void)(host_calibration_escape()||host_mesh_escape()||host_profiles_escape()||host_camera_escape()||host_setup_escape());
 }else if(input)lv_obj_send_event(input,LV_EVENT_KEY,&key);
}
int main(int argc,char **argv){
 if(argc==2&&!strcmp(argv[1],"--version")){
#ifdef PRINTERHMI_DEMO_RUNTIME
  puts("printerhmi-ui-host 32 test-only");
#else
  puts("printerhmi-ui-host 32");
#endif
  return 0;
 }
 (void)esp_timer_get_time();setvbuf(stdout,NULL,_IOFBF,65536);lv_init();lv_display_t *display=lv_display_create(1024,600);lv_display_set_color_format(display,LV_COLOR_FORMAT_ARGB8888);lv_display_set_buffers(display,draw_buffer,NULL,sizeof(draw_buffer),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(display,flush);
 ui_theme_set_active(UI_THEME_STUDIO_DARK);ui_theme_set_density(UI_DENSITY_COMFORTABLE);lv_obj_set_style_bg_opa(lv_screen_active(),LV_OPA_TRANSP,0);lv_obj_set_style_bg_opa(lv_layer_bottom(),LV_OPA_TRANSP,0);lv_obj_set_style_bg_opa(lv_layer_top(),LV_OPA_TRANSP,0);
 lv_group_t *group=lv_group_create();pointer=lv_indev_create();lv_indev_set_type(pointer,LV_INDEV_TYPE_POINTER);lv_indev_set_read_cb(pointer,read_pointer);keyboard=lv_indev_create();lv_indev_set_type(keyboard,LV_INDEV_TYPE_KEYPAD);lv_indev_set_read_cb(keyboard,read_key);lv_indev_set_group(keyboard,group);
 machine.moonraker_ok=machine.live_data_ok=true;strcpy(machine.printer_state,"printing");strcpy(machine.homed_axes,"xyz");machine.toolhead_position_valid=true;machine.nozzle_temp=210;machine.nozzle_target=210;machine.bed_temp=60;machine.bed_target=60;machine.part_fan_speed=65;machine.speed_factor=machine.flow_factor=100;
 strcpy(machine.printer_file,"Spool holder.gcode");machine.progress=.64;machine.print_duration=3240;machine.current_layer=64;machine.total_layer=100;machine.air_temp=28;machine.chamber_temp=29;machine.humidity=18;machine.live_velocity=85;machine.live_flow=8.4;machine.capabilities=(moonraker_capabilities_t){.discovered=true,.has_heated_bed=true,.has_part_fan=true,.has_exclude_object=true,.has_drybox_environment_sensor=true,.has_drybox_center_sensor=true,.has_drybox_heater=true,.has_drybox_fan=true,.has_drybox_macros=true};
 console_controller_init();operator_event_log_init();host_profiles_init();for(int p=0;p<MOONRAKER_CONFIG_MAX_PROFILES;p++){const moonraker_profile_t *endpoint=moonraker_config_profile(p);if(endpoint)profile_states[p].endpoint=*endpoint;}theme_manager_select(UI_THEME_STUDIO_DARK);theme_manager_select_density(UI_DENSITY_COMFORTABLE);ui_toast_init();timezone_config_init();ui_settings_module_init();operator_event_log_add(OPERATOR_EVENT_INFO,"Theme Maker local tablet simulation ready.");
 host_telemetry_seed(&machine,esp_timer_get_time());
 host_lighting_refresh();ui_printer_popups_set_control_sender(checked_send);ui_printer_toolhead_set_sender(checked_send);
 char line[2048];while(fgets(line,sizeof(line),stdin)){
  char name[64];int x=0,y,down;unsigned key;double a,b,c;
  if(!strncmp(line,"quit",4))break;
  if(sscanf(line,"select-profile %d",&x)==1){if(moonraker_config_select_profile(x))host_profile_changed();continue;}
  if(sscanf(line,"demo-mode-fixture %d",&x)==1){host_demo_fixture(x);continue;}
  if(sscanf(line,"scenario %63s",name)==1){(void)host_demo_scenario(name);frame();continue;}
  if(sscanf(line,"simulation-run %d",&x)==1){simulation_running=x!=0;simulation_time=esp_timer_get_time();frame();continue;}
  if(sscanf(line,"ota-fixture %d",&x)==1){host_ota_fixture(x);continue;}
  if(sscanf(line,"lighting-fixture %d",&x)==1){host_lighting_fixture(x);continue;}
  if(sscanf(line,"profile-fixture %d %d",&x,&y)==2){host_profiles_fixture(x,y);continue;}
  if(sscanf(line,"network-fixture %d %d",&x,&y)==2){host_network_fixture(x,y);continue;}
  if(sscanf(line,"settings-fail %d %d",&x,&y)==2){host_storage_fail(x!=0);host_display_fail(y!=0);continue;}
  if(!strncmp(line,"telemetry-info",14)){telemetry_sample_t first={0},last={0};size_t count=telemetry_history_count();if(count){telemetry_history_get(0,&first);telemetry_history_get(count-1,&last);}printf("HISTORY %zu %u %lld %lld %u %zu\n",count,telemetry_history_generation(),(long long)first.time_us,(long long)last.time_us,last.unavailable,last.hotend_count);fflush(stdout);continue;}
  if(!strncmp(line,"bounds ",7)){line[strcspn(line,"\r\n")]=0;lv_obj_update_layout(lv_screen_active());lv_obj_update_layout(lv_layer_top());lv_obj_t *button=button_named(lv_layer_top(),line+7);if(!button)button=button_named(lv_screen_active(),line+7);lv_area_t area={0};if(button)lv_obj_get_coords(button,&area);printf("BOUNDS %d %d %d %d %d\n",button!=NULL,area.x1,area.y1,area.x2,area.y2);fflush(stdout);continue;}
  if(!strncmp(line,"ui-text",7)){char text[32768];size_t used=0;collect_text(lv_screen_active(),text,sizeof(text),&used);collect_text(lv_layer_top(),text,sizeof(text),&used);printf("TEXT %zu\n",used);fwrite(text,1,used,stdout);fflush(stdout);continue;}
  if(!strncmp(line,"calibration-info",16)){host_calibration_info();continue;}
  if(sscanf(line,"calibration-fixture %d",&x)==1){host_calibration_fixture(x,&machine);continue;}
  if(!strncmp(line,"mesh-info",9)){host_mesh_info();continue;}
  if(sscanf(line,"mesh-fixture %d",&x)==1){host_mesh_fixture(x);continue;}
  if(!strncmp(line,"macros-info",11)){host_macros_info();continue;}
  if(sscanf(line,"macros-fixture %d",&x)==1){host_macros_fixture(x);continue;}
  if(!strncmp(line,"devices-info",12)){host_devices_info();continue;}
  if(sscanf(line,"devices-fixture %d",&x)==1){host_devices_fixture(x);continue;}
  if(sscanf(line,"telemetry-fixture %d",&x)==1){host_telemetry_fixture(&machine,x);continue;}
  if(sscanf(line,"advance %u",&key)==1){host_time_offset_us+=(int64_t)(key>3600000?3600000:key)*1000;lv_tick_inc(key>3600000?3600000:key);host_lighting_refresh();host_devices_refresh();host_macros_refresh();host_mesh_refresh();host_calibration_refresh();lv_timer_handler();frame();continue;}
  if(sscanf(line,"drybox-fail %d",&x)==1){drybox_send_fail=x!=0;continue;}
  if(sscanf(line,"notify %d",&x)==1){ui_toast_set_level((ui_notification_level_t)x);continue;}
  if(sscanf(line,"drybox-sensors %lf %lf %lf",&a,&b,&c)==3){machine.air_temp=a;machine.chamber_temp=b;machine.humidity=c;continue;}
  if(sscanf(line,"state %63s",name)==1){const char *allowed[]={"printing","paused","ready","standby","cancelled","complete","shutdown","error","offline"};for(unsigned i=0;i<sizeof(allowed)/sizeof(allowed[0]);i++)if(!strcmp(name,allowed[i]))snprintf(machine.printer_state,sizeof(machine.printer_state),"%.31s",name);continue;}
  if(!strncmp(line,"job ",4)){line[strcspn(line,"\r\n")]=0;snprintf(machine.printer_file,sizeof(machine.printer_file),"%.255s",line+4);continue;}
  if(sscanf(line,"color %63s %x",name,&key)==2){for(unsigned i=0;i<ROUTE_COUNT;i++)if(!strcmp(name,routes[i].name)){palette[i]=key;break;}continue;}
  if(sscanf(line,"metric %63s %d",name,&x)==2){const char *names[]={"radius_card","radius_button","radius_panel"};for(int i=0;i<3;i++)if(!strcmp(names[i],name)&&x>=0&&x<=64){metrics[i]=x;metric_enabled[i]=true;}continue;}
  double offset;
  if(sscanf(line,"position %lf %lf %lf %lf",&a,&b,&c,&offset)==4){machine.toolhead_x=a;machine.toolhead_y=b;machine.toolhead_z=c;machine.z_offset=offset;continue;}
  if(sscanf(line,"progress %lf",&a)==1){if(a>=0&&a<=100){machine.progress=a/100;machine.current_layer=(int)(machine.progress*machine.total_layer+.5);machine.print_duration=3240*machine.progress/.64;}continue;}
  if(sscanf(line,"factors %lf %lf",&a,&b)==2){machine.speed_factor=a;machine.flow_factor=b;continue;}
  if(sscanf(line,"targets %lf %lf %lf",&a,&b,&c)==3){machine.nozzle_target=a;machine.bed_target=b;machine.part_fan_speed=c;continue;}
  if(sscanf(line,"open %63s %d",name,&x)>=1&&!strncmp(line,"open ",5)){
   close_all();palette_enabled=x==1;
   if(!strcmp(name,"calibration-page")||!strcmp(name,"mesh-page")||!strcmp(name,"macros-page")||!strcmp(name,"devices-page")||!strcmp(name,"telemetry-page")||!strcmp(name,"tools-page")||!strcmp(name,"console-page")||!strcmp(name,"printer-page")||!strcmp(name,"dashboard-page")||!strcmp(name,"files-page")||!strcmp(name,"camera-page")||!strcmp(name,"drybox-page")||!strcmp(name,"settings-page")||!strcmp(name,"network-page")||!strcmp(name,"chooser-page")){page_mode=true;ui_apply_root_style(lv_screen_active());host_page_show(name,checked_send);}
   else if(!strcmp(name,"setup")){page_mode=true;host_setup_show();}
   else if(!strcmp(name,"ota"))host_ota_show();
   else if(!strcmp(name,"nozzle"))ui_printer_popups_show_nozzle(send_command,machine.nozzle_temp,machine.nozzle_target);
   else if(!strcmp(name,"bed"))ui_printer_popups_show_bed(send_command,machine.bed_temp,machine.bed_target);
   else if(!strcmp(name,"fan"))ui_printer_popups_show_part_fan(send_command,machine.part_fan_speed);
   else if(!strcmp(name,"stop"))ui_printer_popups_show_cancel(send_command);
   else if(!strcmp(name,"tune"))host_tuning_show(checked_send);
   else if(!strcmp(name,"toolhead")){static lv_obj_t *a,*b,*c;static double step=1;ui_printer_toolhead_show(&a,&b,&c,&step,send_command);}
   frame();continue;
  }
  if(sscanf(line,"wheel %d %d %d",&x,&y,&down)==3){wheel_scroll(x,y,down<-600?-600:down>600?600:down);frame();continue;}
  if(sscanf(line,"hover %d %d",&x,&y)==2){printf("HIT %s\n",hover_cursor(x,y));fflush(stdout);continue;}
  if(sscanf(line,"pointer %d %d %d",&x,&y,&down)==3){pointer_data.point.x=x<0?0:x>1023?1023:x;pointer_data.point.y=y<0?0:y>599?599:y;pointer_data.state=down?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED;lv_indev_read(pointer);frame();continue;}
  if(sscanf(line,"key %u",&key)==1){handle_key(key);frame();continue;}
  if(!strncmp(line,"close",5))close_all();
  frame();
 }
 close_all();lv_display_delete(display);lv_deinit();return 0;
}
