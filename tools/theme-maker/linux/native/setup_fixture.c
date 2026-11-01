/* Network identities are process-local. The Setup UI and saved-step controller
 * are compiled unchanged from firmware. */
#include "esp_netif.h"
#include "esp_wifi.h"
#include "ui_setup_wizard.c"
#include "onboarding_controller.c"
#include "camera_discovery_controller.h"
#include "moonraker_config_controller.h"
#include <stdio.h>
#include <string.h>
bool host_network_online(void);
bool host_network_setup_connect(const char *,const char *);
void host_page_rebuild_settings(void);
esp_err_t esp_wifi_sta_get_ap_info(wifi_ap_record_t *out)
{
    if(!out||!host_network_online())return ESP_FAIL;
    memset(out,0,sizeof(*out));
    extern const char *host_network_ssid(void);
    snprintf((char *)out->ssid,sizeof(out->ssid),"%s",host_network_ssid());out->rssi=-48;return ESP_OK;
}
esp_netif_t *esp_netif_get_handle_from_ifkey(const char *key){static esp_netif_t netif;return key&&!strcmp(key,"WIFI_STA_DEF")?&netif:NULL;}
esp_err_t esp_netif_get_ip_info(esp_netif_t *netif,esp_netif_ip_info_t *out){if(!netif||!out||!host_network_online())return ESP_FAIL;out->ip.addr=0x0100007f;return ESP_OK;}
bool camera_discovery_start(const char *host,int port,const char *key){return camera_discovery_start_for_profile(moonraker_config_active_profile_index(),host,port,key);}
void host_setup_show(void){ui_setup_wizard_show_with_appearance_callback(host_network_setup_connect,host_page_rebuild_settings);}

void host_setup_close(void){ui_setup_wizard_close();setup_completion_done_cb(NULL);}
bool host_setup_escape(void){
 for(unsigned i=0;i<lv_obj_get_child_count(lv_layer_top());i++)if(!lv_obj_has_flag(lv_obj_get_child(lv_layer_top(),i),LV_OBJ_FLAG_HIDDEN))return false;
 if(s_complete_popup){setup_completion_done_cb(NULL);return true;}
 if(!s_center)return false;
 later_cb(NULL);return true;
}

void host_onboarding_restart(void){s_initialized=false;s_complete=false;s_steps=0;onboarding_controller_init();}
