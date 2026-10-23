/* Shared editor/manager with a synthetic SD root for the certificate picker. */
#include <dirent.h>
#include <string.h>
static int directory_token,directory_read;
static DIR *host_opendir(const char *path){directory_read=0;return !strcmp(path,"/sdcard")?(DIR *)&directory_token:NULL;}
static struct dirent *host_readdir(DIR *directory){static struct dirent entry;if(directory!=(DIR *)&directory_token||directory_read++)return NULL;memset(&entry,0,sizeof(entry));strcpy(entry.d_name,"simulation-ca.pem");return &entry;}
static int host_closedir(DIR *directory){(void)directory;return 0;}
#define opendir host_opendir
#define readdir host_readdir
#define closedir host_closedir
#include "ui_printer_profiles.c"
#include "ui_printer_chooser.h"
#include "moonraker_discovery.h"
void host_profile_changed(void);
void host_page_emit(const char *);
bool host_network_online(void);
static void discovered(const char *host,int port,const char *identity,uint32_t request){
 if(ui_printer_profiles_apply_discovered_endpoint(request,host,port,identity))moonraker_discovery_close_request(request);
}
static void discover(void){uint32_t request=ui_printer_profiles_discovery_request();if(!request)return;
 if(!moonraker_discovery_show_for_request("Starting sample discovery...",ui_printer_profiles_cancel_discovery_request,discovered,request))return;
 esp_ip4_addr_t ip={.addr=0x0100007f};moonraker_discovery_start(host_network_online()?&ip:NULL);
}
void host_profiles_show(void){ui_printer_profiles_show(host_profile_changed,discover);}
static void choose(int profile){int old=moonraker_config_active_profile_index();if(!moonraker_config_select_profile(profile))return;if(old!=moonraker_config_active_profile_index())host_profile_changed();ui_printer_chooser_hide();host_page_emit("@nav:dashboard");}
static void manage(int profile){ui_printer_profiles_show_for_slot(profile,host_profile_changed,discover);}
void host_chooser_show(void){ui_printer_chooser_show(choose,manage);}
void host_profiles_close(void){moonraker_discovery_close();ui_printer_profiles_close_all();}
bool host_profiles_escape(void){
 if(moonraker_discovery_is_open()){moonraker_discovery_close();return true;}
 if(s_editor_keyboard_popup){editor_keyboard_discard();return true;}
 if(s_editor_pem_picker){editor_pem_picker_close();return true;}
 if(s_editor_security_popup){editor_security_close();return true;}
 if(s_editor_auth_popup){editor_auth_close();return true;}
 if(s_editor_camera_remove_popup){editor_camera_remove_close();return true;}
 if(s_editor_camera_popup){editor_camera_close();return true;}
 if(s_editor_popup){editor_close();return true;}
 if(s_delete_popup){delete_close();return true;}
 if(s_manager_popup){ui_printer_profiles_close_all();return true;}
 return false;
}
