/* Real Settings page/dialog assembly with routing for unmigrated workflows. */
#include "ui_settings.c"
#include "ui_event_history_popup.h"
#include "ui_shell.h"
#include "moonraker_config_controller.h"
#include "theme_manager.h"
#include "timezone_config.h"
#include "ui_toast.h"
void host_page_emit(const char *event);
static void ota(lv_event_t *event){(void)event;host_page_emit("@setting:ota");}
static void network(lv_event_t *event){(void)event;host_page_emit("@nav:network");}
static void setup(lv_event_t *event){(void)event;host_page_emit("@setting:setup");}
void host_settings_close(void){ui_event_history_popup_close();if(settings_panel)hide_settings_tab();}
void host_settings_show(void);
void host_page_rebuild_settings(void);
static void rebuild(void){host_page_rebuild_settings();}
void host_settings_show(void){ui_settings_show_page("Mounted (sample)","8.2 GB / 32 GB (sample)",ota,network,setup,rebuild);}

void host_preferences_restart(void);
void host_profile_changed(void);
void host_settings_restart(void){host_settings_close();moonraker_config_load();host_profile_changed();host_preferences_restart();theme_manager_init();timezone_config_init();ui_toast_init();ui_settings_module_init();}

bool host_settings_wake_target(lv_obj_t *object){return s_display_sleeping&&object==s_sleep_wake_overlay;}
