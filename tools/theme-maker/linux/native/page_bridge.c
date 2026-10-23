/* Host routing and fixtures only; shell and Tools layout/widgets are shared UI. */
#include <string.h>
#include <stdio.h>
#include "ui_shell.h"
#include "ui_global_estop.h"
#include "ui_tools.c"
#include "ui_console.h"
#include "ui_drybox.h"
#include "ui_toast.h"
void host_printer_show(bool (*send)(const char *));
void host_printer_close(void);
void host_dashboard_show(bool (*send)(const char *));
void host_dashboard_close(void);
void host_files_show(bool (*send)(const char *));
void host_files_close(void);
void host_camera_show(void);
void host_camera_close(void);
void host_profiles_close(void);
void host_drybox_status(lv_event_t *event);
static bool host_shell_created,host_drybox_open;
static char host_page_event[64];
void ui_shell_page_action(ui_shell_page_t page){static const char *names[]={"dashboard","printer","files","camera","tools","console","drybox","settings"};if(page>=0&&page<8)snprintf(host_page_event,sizeof(host_page_event),"@nav:%s",names[page]);}
static void choose_printer(void){snprintf(host_page_event,sizeof(host_page_event),"@printers");}
static void calibration(void){snprintf(host_page_event,sizeof(host_page_event),"@tool:Calibration");}
static void mesh(void){snprintf(host_page_event,sizeof(host_page_event),"@tool:Bed mesh");}
static void devices(void){snprintf(host_page_event,sizeof(host_page_event),"@tool:Devices");}
static void macros(void){snprintf(host_page_event,sizeof(host_page_event),"@tool:Macros");}
void host_page_close(void){ui_toast_close();if(host_drybox_open){host_drybox_open=false;ui_drybox_hide();}host_profiles_close();host_camera_close();host_files_close();host_dashboard_close();host_printer_close();ui_console_hide();if(s_root)lv_obj_delete(s_root);if(host_shell_created)ui_shell_destroy();host_page_event[0]=0;}
void host_page_show(const char *kind,bool (*send)(const char *)){
 host_page_close();ui_global_estop_init(send);ui_shell_create();host_shell_created=true;ui_shell_create_nav();ui_shell_set_active_printer_name("Sermoon D1");ui_shell_set_printer_switch_callback(choose_printer);ui_shell_set_wifi_signal(true,-48);ui_shell_set_active_nav(UI_SHELL_PAGE_TOOLS);
 if(!strcmp(kind,"drybox-page")){ui_shell_set_active_nav(UI_SHELL_PAGE_DRYBOX);ui_drybox_set_callbacks(send,host_drybox_status);ui_drybox_show();host_drybox_open=true;}
 else if(!strcmp(kind,"camera-page")){ui_shell_set_active_nav(UI_SHELL_PAGE_CAMERA);host_camera_show();}
 else if(!strcmp(kind,"files-page")){ui_shell_set_active_nav(UI_SHELL_PAGE_FILES);host_files_show(send);}
 else if(!strcmp(kind,"dashboard-page")){ui_shell_set_active_nav(UI_SHELL_PAGE_DASHBOARD);host_dashboard_show(send);}
 else if(!strcmp(kind,"printer-page")){ui_shell_set_active_nav(UI_SHELL_PAGE_PRINTER);host_printer_show(send);}
 else if(!strcmp(kind,"console-page")){ui_shell_set_active_nav(UI_SHELL_PAGE_CONSOLE);ui_console_show(send);}
 else {ui_tools_set_callbacks(calibration,mesh,devices,macros);ui_tools_show();}ui_shell_raise_topbar();ui_shell_raise_nav();
}
const char *host_page_take_event(void){return host_page_event;}
void host_page_clear_event(void){host_page_event[0]=0;}
