/* Host routing and fixtures only; shell and Tools layout/widgets are shared UI. */
#include <string.h>
#include <stdio.h>
#include "ui_shell.h"
#include "ui_global_estop.h"
#include "ui_tools.c"
#include "ui_console.h"
#include "ui_drybox.h"
#include "ui_toast.h"
#include "ui_settings.h"
#include "ui_printer_chooser.h"
#include "ui_telemetry.h"
#include "ui_devices.h"
#include "ui_macros.h"
#include "ui_bed_mesh.h"
#include "ui_calibration.h"
void host_calibration_show(void (*mesh)(void),bool (*send)(const char *));
void host_mesh_show(bool (*send)(const char *));
void host_macros_show(bool (*send)(const char *));
#include "moonraker_config_controller.h"
void host_printer_show(bool (*send)(const char *));
void host_printer_close(void);
void host_dashboard_show(bool (*send)(const char *));
void host_dashboard_close(void);
void host_files_show(bool (*send)(const char *));
void host_files_close(void);
void host_camera_show(void);
void host_camera_close(void);
void host_profiles_close(void);
void host_chooser_show(void);
void host_settings_show(void);
void host_network_show(void);
void host_network_close(void);
void host_network_topbar(void);
void host_settings_close(void);
void host_drybox_status(lv_event_t *event);
static bool host_shell_created,host_drybox_open,host_calibration_open;
static bool (*host_page_send)(const char *);
static char host_page_event[64];
void ui_shell_page_action(ui_shell_page_t page){static const char *names[]={"dashboard","printer","files","camera","tools","console","drybox","settings"};if(page==UI_SHELL_PAGE_DEVICES){snprintf(host_page_event,sizeof(host_page_event),"@tool:Devices");return;}if(page>=0&&page<8)snprintf(host_page_event,sizeof(host_page_event),"@nav:%s",names[page]);}
static void choose_printer(void){snprintf(host_page_event,sizeof(host_page_event),"@printers");}
static void calibration(void){snprintf(host_page_event,sizeof(host_page_event),"@tool:Calibration");}
static void open_mesh(void){snprintf(host_page_event,sizeof(host_page_event),"@tool:Bed mesh");}
static void mesh(void){snprintf(host_page_event,sizeof(host_page_event),"@tool:Bed mesh");}
static void graphs(void){snprintf(host_page_event,sizeof(host_page_event),"@nav:telemetry");}
static void devices(void){snprintf(host_page_event,sizeof(host_page_event),"@tool:Devices");}
static void macros(void){snprintf(host_page_event,sizeof(host_page_event),"@tool:Macros");}
void host_page_emit(const char *event){snprintf(host_page_event,sizeof(host_page_event),"%s",event);}
void ui_shell_raise(void){ui_shell_raise_topbar();ui_shell_raise_nav();}
void host_page_close(void){host_calibration_open=false;ui_calibration_hide();ui_bed_mesh_close();ui_macros_hide();ui_devices_hide();ui_telemetry_hide();host_network_close();host_settings_close();ui_toast_close();if(host_drybox_open){host_drybox_open=false;ui_drybox_hide();}host_profiles_close();ui_printer_chooser_hide();host_camera_close();host_files_close();host_dashboard_close();host_printer_close();ui_console_hide();if(s_root)lv_obj_delete(s_root);if(host_shell_created)ui_shell_destroy();host_page_event[0]=0;}
void host_page_show(const char *kind,bool (*send)(const char *)){
 host_page_close();host_page_send=send;ui_global_estop_init(send);ui_shell_create();host_shell_created=true;ui_shell_create_nav();ui_shell_set_active_printer_name(moonraker_config_active_profile_name());ui_shell_set_printer_switch_callback(choose_printer);ui_shell_set_wifi_signal(true,-48);ui_shell_set_active_nav(UI_SHELL_PAGE_TOOLS);
 if(!strcmp(kind,"calibration-page")){host_calibration_open=true;host_calibration_show(open_mesh,send);}
 else if(!strcmp(kind,"mesh-page")){host_mesh_show(send);}
 else if(!strcmp(kind,"macros-page")){host_macros_show(send);}
 else if(!strcmp(kind,"devices-page")){ui_shell_set_active_nav(UI_SHELL_PAGE_TOOLS);ui_devices_show(graphs);}
 else if(!strcmp(kind,"telemetry-page")){ui_shell_set_active_nav(UI_SHELL_PAGE_DEVICES);ui_telemetry_show();}
 else if(!strcmp(kind,"chooser-page")){ui_shell_set_active_nav(-1);host_chooser_show();}
 else if(!strcmp(kind,"network-page")){ui_shell_set_active_nav(UI_SHELL_PAGE_SETTINGS);host_network_show();}
 else if(!strcmp(kind,"settings-page")){ui_shell_set_active_nav(UI_SHELL_PAGE_SETTINGS);host_settings_show();}
 else if(!strcmp(kind,"drybox-page")){ui_shell_set_active_nav(UI_SHELL_PAGE_DRYBOX);ui_drybox_set_callbacks(send,host_drybox_status);ui_drybox_show();host_drybox_open=true;}
 else if(!strcmp(kind,"camera-page")){ui_shell_set_active_nav(UI_SHELL_PAGE_CAMERA);host_camera_show();}
 else if(!strcmp(kind,"files-page")){ui_shell_set_active_nav(UI_SHELL_PAGE_FILES);host_files_show(send);}
 else if(!strcmp(kind,"dashboard-page")){ui_shell_set_active_nav(UI_SHELL_PAGE_DASHBOARD);host_dashboard_show(send);}
 else if(!strcmp(kind,"printer-page")){ui_shell_set_active_nav(UI_SHELL_PAGE_PRINTER);host_printer_show(send);}
 else if(!strcmp(kind,"console-page")){ui_shell_set_active_nav(UI_SHELL_PAGE_CONSOLE);ui_console_show(send);}
 else {ui_tools_set_callbacks(calibration,mesh,devices,macros);ui_tools_show();}host_network_topbar();ui_shell_raise_topbar();ui_shell_raise_nav();
}
const char *host_page_take_event(void){return host_page_event;}
void host_page_clear_event(void){host_page_event[0]=0;}

void host_page_rebuild_settings(void){host_page_show("settings-page",host_page_send);}

/* Escape follows the displayed dismissal callback so private popup owners retire.
 * Never choose a measurement, Accept, Abort, or configuration action. */
static lv_obj_t *calibration_dismiss_button(lv_obj_t *root){
 if(lv_obj_has_flag(root,LV_OBJ_FLAG_HIDDEN))return NULL;
 if(lv_obj_check_type(root,&lv_button_class)&&!lv_obj_has_state(root,LV_STATE_DISABLED)){
  for(uint32_t i=0;i<lv_obj_get_child_count(root);i++){
   lv_obj_t *label=lv_obj_get_child(root,i);if(!lv_obj_check_type(label,&lv_label_class))continue;
   const char *value=lv_label_get_text(label);size_t length=strlen(value);
   const char *names[]={"Back","Close","Cancel"};
   for(unsigned j=0;j<3;j++){size_t n=strlen(names[j]);if(length>=n&&!strcmp(value+length-n,names[j]))return root;}
  }
 }
 for(uint32_t i=0;i<lv_obj_get_child_count(root);i++){lv_obj_t *button=calibration_dismiss_button(lv_obj_get_child(root,i));if(button)return button;}
 return NULL;
}
bool host_calibration_escape(void){
 if(!host_calibration_open)return false;
 for(int i=(int)lv_obj_get_child_count(lv_layer_top())-1;i>=0;i--){
  lv_obj_t *modal=lv_obj_get_child(lv_layer_top(),i);if(lv_obj_has_flag(modal,LV_OBJ_FLAG_HIDDEN))continue;
  lv_obj_t *button=calibration_dismiss_button(modal);if(!button)return false;
  lv_obj_send_event(button,LV_EVENT_CLICKED,NULL);return true;
 }
 return false;
}
