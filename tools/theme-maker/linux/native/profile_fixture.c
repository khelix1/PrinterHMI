/* Local probe/health/trust adapters. Profiles and camera catalogs use the
 * firmware controllers and the host NVS store, with no sockets or SD access. */
#include <stdio.h>
#include <string.h>
#include "lvgl.h"
#include "moonraker.h"
#include "moonraker_config_controller.h"
#include "camera_catalog_controller.h"
#include "moonraker_endpoint_test.h"
#include "moonraker_transport_security_controller.h"
#include "printer_profile_health.h"
#include "printer_preview_cache.h"
#include "printer_preview_store.h"
bool host_network_online(void);
void host_profile_state(int,moonraker_state_t *);
static int probe_mode,discovery_mode;
static bool probe_pending,probe_verified,approved_ca[MOONRAKER_CONFIG_MAX_PROFILES];
static uint32_t probe_started;
static moonraker_probe_result_t probe_result;
void host_profiles_fixture(int test,int discovery){probe_mode=test;discovery_mode=discovery;}
int host_discovery_mode(void){return discovery_mode;}
void host_profiles_init(void){
 moonraker_config_load();moonraker_config_save_profile(0,"Sermoon D1","simulation.invalid",7125,"");
 moonraker_config_save_profile(1,"Sermoon D1 #2","simulation-2.invalid",7126,"");
 camera_catalog_set(0,0,"Front camera","http://simulation.invalid/camera/1");
 camera_catalog_set(0,1,"Side camera","http://simulation.invalid/camera/2");
 camera_catalog_set(1,0,"Top camera","http://simulation-2.invalid/camera/2");
}
bool moonraker_endpoint_test_start(const char *host,int port,const char *key){
 (void)key;if(probe_mode==3)return false;
 probe_pending=true;probe_started=lv_tick_get();probe_verified=host_network_online()&&host&&host[0]&&port>0&&port<=65535&&probe_mode!=1&&!strstr(host,"fail")&&!strstr(host,"unavailable");
 probe_result=(moonraker_probe_result_t){.reachable=probe_verified,.klippy_ready=probe_verified&&probe_mode!=2};
 snprintf(probe_result.identity,sizeof(probe_result.identity),"%s",port==7126?"Sermoon D1 #2":"Sermoon D1");return true;
}
bool moonraker_endpoint_test_busy(void){return false;}
bool moonraker_endpoint_test_take_result(moonraker_probe_result_t *out,bool *verified){if(!probe_pending||lv_tick_elaps(probe_started)<200)return false;probe_pending=false;*out=probe_result;*verified=probe_verified;return true;}
const char *moonraker_transport_security_ca_pem_for_profile(int p){return p>=0&&p<MOONRAKER_CONFIG_MAX_PROFILES&&approved_ca[p]?"SIMULATION ONLY - no TLS material":NULL;}
bool moonraker_transport_security_import_ca_file_for_profile(int p,const char *path){if(p<0||p>=MOONRAKER_CONFIG_MAX_PROFILES||!path||strcmp(path,"/sdcard/simulation-ca.pem"))return false;approved_ca[p]=true;return true;}
void printer_preview_cache_invalidate(int p){(void)p;}
void printer_preview_store_invalidate(int p){(void)p;}
void printer_profile_health_reconcile(void){}
bool printer_profile_health_get(int p,bool *known){const moonraker_profile_t *profile=moonraker_config_profile(p);*known=profile&&profile->configured;return *known&&host_network_online()&&!strstr(profile->host,"fail");}
void printer_profile_health_set(int p,bool known,bool online){(void)p;(void)known;(void)online;}
bool printer_profile_health_get_live_state(int p,char *out,size_t size){const moonraker_profile_t *profile=moonraker_config_profile(p);if(!profile||!profile->configured)return false;moonraker_state_t state;host_profile_state(p,&state);snprintf(out,size,"%s",state.printer_state);return true;}
bool printer_profile_health_live_state_fresh(int p,int64_t age){(void)age;const moonraker_profile_t *profile=moonraker_config_profile(p);return profile&&profile->configured&&host_network_online();}
