#!/usr/bin/env python3
"""Compile the actual WiFi connect bridge against typed storage/ESP fixtures."""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[2]
source=(root/"main/main.c").read_text()
def function(signature):
    start=source.index(signature)
    return source[start:source.index("\n}",start)+2]
production="\n".join(function(name) for name in (
    "static void clear_sensitive_buffer(",
    "static bool connect_selected_wifi(void)",
    "static void ui_network_tools_save_wifi_password_only_cb("))
fixture=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
typedef int esp_err_t;
typedef void lv_event_t;
#define ESP_OK 0
#define WIFI_IF_STA 0
#define ESP_LOGI(...) ((void)0)
typedef struct {struct {unsigned char ssid[32],password[64];} sta;} wifi_config_t;
static char ui_network_tools_selected_wifi_ssid[33]="Sermoon WiFi";
static char ui_network_tools_selected_wifi_password[65],ui_network_tools_network_scan_status[192];
static char saved_wifi_ssid[33],saved_wifi_password[65],wifi_status[128],inline_status[192];
static const char input[]="network-secret";
static bool storage_ok,s_wifi_credentials_configured;
static int config_result,connect_result,config_calls,connect_calls,closed,s_retry_num;
static size_t strlcpy(char *out,const char *in,size_t size){size_t n=strlen(in);if(size){size_t take=n<size?n:size-1;memcpy(out,in,take);out[take]=0;}return n;}
static void safe_copy(char *out,size_t n,const char *in){strlcpy(out,in,n);}
static const char *esp_err_to_name(esp_err_t e){return e?"fixture failure":"OK";}
static esp_err_t esp_wifi_disconnect(void){return ESP_OK;}
static esp_err_t esp_wifi_set_config(int interface,wifi_config_t *config){assert(interface==WIFI_IF_STA);assert(!strcmp((char *)config->sta.password,input));config_calls++;return config_result;}
static esp_err_t esp_wifi_connect(void){connect_calls++;return connect_result;}
static bool wifi_credentials_store_save(const char *ssid,const char *password){assert(!strcmp(ssid,"Sermoon WiFi")&&!strcmp(password,input));return storage_ok;}
static void ui_network_tools_wifi_password_copy_owned(char *out,size_t n){strlcpy(out,input,n);}
static void ui_network_tools_wifi_password_set_status(const char *s){strlcpy(inline_status,s,sizeof(inline_status));}
static void ui_network_tools_wifi_scan_set_status(const char *s){(void)s;}
static void ui_network_tools_wifi_scan_close_owned(void){closed++;}
'''
tests=r'''
int main(void){
 storage_ok=false;ui_network_tools_save_wifi_password_only_cb(NULL);
 assert(!config_calls&&!connect_calls&&!closed&&strstr(inline_status,"Could not save")&&!ui_network_tools_selected_wifi_password[0]);
 storage_ok=true;config_result=1;ui_network_tools_save_wifi_password_only_cb(NULL);
 assert(config_calls==1&&!connect_calls&&!closed&&strstr(inline_status,"applying WiFi configuration")&&!ui_network_tools_selected_wifi_password[0]);
 config_result=0;connect_result=1;ui_network_tools_save_wifi_password_only_cb(NULL);
 assert(config_calls==2&&connect_calls==1&&!closed&&strstr(inline_status,"starting WiFi")&&!ui_network_tools_selected_wifi_password[0]);
 connect_result=0;ui_network_tools_save_wifi_password_only_cb(NULL);
 assert(config_calls==3&&connect_calls==2&&closed==1&&!ui_network_tools_selected_wifi_password[0]);
 assert(!strcmp(saved_wifi_ssid,"Sermoon WiFi")&&!strcmp(saved_wifi_password,input));
 ui_network_tools_selected_wifi_ssid[0]=0;assert(!connect_selected_wifi()&&config_calls==3);
 puts("PASS: actual WiFi bridge retains editor on storage/config/start failures, closes on accepted request and clears temporary credentials");
}
'''
with tempfile.TemporaryDirectory(prefix="wifi-connect-") as directory:
    src=Path(directory)/"test.c";src.write_text(fixture+production+tests)
    executable=Path(directory)/"test"
    subprocess.run(["cc","-std=c11","-O2","-Wall","-Wextra","-Werror",str(src),"-o",str(executable)],check=True)
    subprocess.run([str(executable)],check=True)
