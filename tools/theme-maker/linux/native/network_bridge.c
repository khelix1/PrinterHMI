/* Network layout, password/keyboard UI and application callbacks are shared
 * unchanged with firmware. Only Wi-Fi operations and device events are local. */
#include <stdio.h>
#include <string.h>
#include "lvgl.h"
#include "ui_network.h"
#include "ui_network_tools.h"
#include "ui_widgets.h"
#include "ui_cards.h"
#include "ui_shell.h"
#include "ui_printer_profiles.h"
#include "network_status_controller.h"
#include "network_activity_controller.h"
#include "wifi_credentials_store.h"
#include "moonraker_config_controller.h"
#include "esp_netif_ip_addr.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
static const char *TAG="native-network";
static bool network_open,s_got_ip=true,s_moonraker_ok=true,s_wifi_credentials_configured=true;
static int s_moonraker_code=200,s_retry_num;
static esp_ip4_addr_t s_ip={.addr=0x0100007f};
static char connected_wifi_ssid[33]="Studio Network",wifi_status[128]="Connected (sample)";
static char saved_wifi_ssid[33],saved_wifi_password[65];
static char ui_network_tools_selected_wifi_ssid[33],ui_network_tools_selected_wifi_password[65];
static char ui_network_tools_network_scan_status[512]="Ready to scan";
static lv_obj_t *network_selected_ssid_label,*network_password_ta,*network_keyboard;
static int scan_mode,connect_mode;
static bool exclusive_owned;
static wifi_config_t pending_wifi;
void host_page_emit(const char *);
void host_profiles_show(void);
bool host_network_online(void){return s_got_ip;}
void host_network_connection(bool);
static void safe_copy(char *out,size_t size,const char *text){snprintf(out,size,"%s",text);}
static void ui_network_tools_wifi_scan_now(void);
static void ui_network_tools_open_printer_profiles_cb(lv_event_t *event){(void)event;host_profiles_show();}
static lv_obj_t *make_printer_info(lv_obj_t *parent,const char *title,const char *value,int x,int y){return ui_info_card_create(parent,title,value,x,y,250,98);}
static void moonraker_configuration_changed(const char *status,bool reset){(void)status;(void)reset;ui_network_set_port(moonraker_config_port());}
/* These functions are extracted from main.c at build time. */
#include "network_builder.inc"
void host_network_topbar(void){ui_shell_set_wifi_signal(s_got_ip,s_got_ip?-48:-100);}
void host_network_refresh(void){if(network_open)ui_network_refresh_bridge();}
void host_network_show(void){network_open=true;snprintf(ui_network_tools_network_scan_status,sizeof(ui_network_tools_network_scan_status),"Ready to scan");ui_network_show();host_network_refresh();host_network_topbar();}
void host_network_close(void){if(!network_open)return;network_open=false;ui_network_hide();memset(ui_network_tools_selected_wifi_password,0,sizeof(ui_network_tools_selected_wifi_password));}
void host_network_fixture(int scan,int connect){scan_mode=scan>=0&&scan<=6?scan:0;connect_mode=connect>=0&&connect<=3?connect:0;}
bool network_activity_controller_request_exclusive(void){if(scan_mode==5||exclusive_owned)return false;exclusive_owned=true;return true;}
bool network_activity_controller_exclusive_ready(void){return exclusive_owned&&scan_mode!=6;}
void network_activity_controller_release_exclusive(void){exclusive_owned=false;}
esp_err_t esp_wifi_scan_start(const wifi_scan_config_t *config,bool block){(void)config;(void)block;return scan_mode==1?ESP_FAIL:ESP_OK;}
esp_err_t esp_wifi_scan_get_ap_num(uint16_t *count){if(scan_mode==2)return ESP_FAIL;*count=scan_mode==4?0:10;return ESP_OK;}
esp_err_t esp_wifi_scan_get_ap_records(uint16_t *count,wifi_ap_record_t *out){
 if(scan_mode==3)return ESP_FAIL;
 const char *names[]={"Studio Network","Workshop","Guest","Workshop & Test","Printer Lab","Drybox Station","Upstairs","Sample network with long name"};
 if(*count>8)*count=8;
 /* Keep the completed fixture scan status through periodic page refreshes. */
 snprintf(ui_network_tools_network_scan_status,sizeof(ui_network_tools_network_scan_status),"10 found - showing %u",(unsigned)*count);
 for(unsigned i=0;i<*count;i++){out[i]=(wifi_ap_record_t){.rssi=-40-(int)i*6};snprintf((char *)out[i].ssid,sizeof(out[i].ssid),"%s",names[i]);}return ESP_OK;
}
esp_err_t esp_wifi_disconnect(void){s_got_ip=s_moonraker_ok=false;s_moonraker_code=0;host_network_connection(false);host_network_topbar();return ESP_OK;}
esp_err_t esp_wifi_set_config(int iface,const wifi_config_t *config){(void)iface;if(connect_mode==1)return ESP_FAIL;pending_wifi=*config;return ESP_OK;}
esp_err_t esp_wifi_connect(void){
 if(connect_mode==2){memset(pending_wifi.sta.password,0,sizeof(pending_wifi.sta.password));return ESP_FAIL;}
 if(connect_mode==3){memset(pending_wifi.sta.password,0,sizeof(pending_wifi.sta.password));snprintf(ui_network_tools_network_scan_status,sizeof(ui_network_tools_network_scan_status),"Authentication rejected (sample). Scan and retry.");return ESP_OK;}
 snprintf(connected_wifi_ssid,sizeof(connected_wifi_ssid),"%.*s",(int)sizeof(pending_wifi.sta.ssid),pending_wifi.sta.ssid);
 memset(pending_wifi.sta.password,0,sizeof(pending_wifi.sta.password));s_got_ip=s_moonraker_ok=true;s_moonraker_code=200;
 snprintf(ui_network_tools_network_scan_status,sizeof(ui_network_tools_network_scan_status),"Connected (sample)");host_network_connection(true);host_network_topbar();return ESP_OK;
}
