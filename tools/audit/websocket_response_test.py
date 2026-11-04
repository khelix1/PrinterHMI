#!/usr/bin/env python3
"""Test actual response routing and discovery publication with real cJSON."""
from pathlib import Path
import os
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / 'main/moonraker_live_websocket.c').read_text()
vendor = root / 'tools/theme-maker/linux/native/vendor/cjson'


def function(name):
    start = re.search(r'^static [^\n]*\b' + name + r'\(', source, re.M).start()
    return source[start:source.index('\n}\n', start) + 3]


fixture = r'''
#include <assert.h>
#include <stdio.h>
#include "moonraker.h"
#include "moonraker_websocket_json.h"
#define WS_COMMAND_ID_FIRST 2000U
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
#define ESP_LOGE(...) ((void)0)
#define CONSOLE_ENTRY_ERROR 0
#define OPERATOR_EVENT_INFO 0
static uint32_t s_generation=7;
static char subscription[128];
static char *s_subscription=subscription;
static size_t s_subscription_capacity=sizeof(subscription);
static bool s_macro_config_pending, s_subscription_ready, s_subscribe_pending;
static unsigned catalogs, configs, commands, subscriptions;
static bool case_lighting_response(const cJSON *r,uint32_t g){assert(r&&g==7);return false;}
static bool endstop_status_controller_merge(const cJSON *r,uint32_t g){assert(r&&g==7);return false;}
static void macro_controller_update_parameters(const cJSON *c){assert(cJSON_IsObject(c));configs++;}
static void motion_diagnostics_controller_update_config(const cJSON *c){assert(cJSON_IsObject(c));configs++;}
static void case_lighting_config(const cJSON *c,uint32_t g){assert(cJSON_IsObject(c)&&g==7);configs++;}
static void macro_controller_update_from_objects(const cJSON *a){assert(cJSON_IsArray(a));catalogs++;}
static void motion_diagnostics_controller_update_objects(const cJSON *a){assert(cJSON_IsArray(a));catalogs++;}
static void device_catalog_controller_update_from_objects(const cJSON *a){assert(cJSON_IsArray(a));catalogs++;}
static void case_lighting_objects(const cJSON *a,uint32_t g){assert(cJSON_IsArray(a)&&g==7);catalogs++;}
static void console_controller_add(int level,const char *fmt,...){assert(level==0&&fmt);commands++;}
static void operator_event_log_add(int level,const char *fmt,...){assert(level==0&&fmt);}
void moonraker_state_configure_hotends(const char names[][MOONRAKER_HOTEND_NAME_MAX],size_t n){assert(n==1&&!strcmp(names[0],"extruder"));}
void moonraker_filament_state_configure(const char names[][MOONRAKER_FILAMENT_SENSOR_NAME_MAX],size_t n,size_t total){assert(names&&n==0&&total==0);}
void moonraker_state_configure_capabilities(const moonraker_capabilities_t *c){assert(c->discovered&&c->has_heated_bed);}
static bool build_subscription(char names[][MOONRAKER_HOTEND_NAME_MAX],size_t n,
 char sensors[][MOONRAKER_FILAMENT_SENSOR_NAME_MAX],size_t f,const moonraker_capabilities_t *c,const cJSON *a){
 assert(names&&n==1&&sensors&&f==0&&c->discovered&&cJSON_IsArray(a));subscriptions++;return true;
}
'''
checks = r'''
static bool dispatch(const char *s){return handle_websocket_response(s,strlen(s));}
int main(void){
 size_t used=0;
 const char key[]="temperature_sensor quote\"slash\\line\n\001";
 assert(append_subscription_text(&used,"{"));
 assert(append_subscription_object(&used,key,"[\"temperature\"]"));
 subscription[used-1]='}';
 cJSON *parsed=cJSON_Parse(subscription);
 assert(cJSON_IsArray(cJSON_GetObjectItemCaseSensitive(parsed,key)));
 assert(cJSON_GetArraySize(parsed)==1);
 cJSON_Delete(parsed);
 strcpy(subscription,"{");used=1;s_subscription_capacity=12;
 assert(!append_subscription_object(&used,key,NULL));
 assert(used==1&&!strcmp(subscription,"{"));
 assert(append_subscription_object(&used,"x",NULL)&&!strcmp(subscription,"{\"x\":null,"));
 const char list[]="{\"id\":1001,\"result\":{\"objects\":[\"extruder\",\"heater_bed\"]}}";
 char exact[sizeof(list)-1];memcpy(exact,list,sizeof(exact));
 assert(handle_websocket_response(exact,sizeof(exact)));
 assert(catalogs==4&&subscriptions==1&&s_subscription_ready&&s_subscribe_pending&&s_macro_config_pending);
 assert(dispatch("{\"id\":1003,\"result\":{\"status\":{\"configfile\":{\"config\":{}}}}} \r\n"));
 assert(configs==3);
 assert(dispatch("{\"id\":2000,\"error\":{\"message\":\"failure\"}}")&&commands==1);
 assert(dispatch("{\"id\":1001,\"error\":{}}"));
 assert(dispatch("{\"id\":1001,\"result\":{\"objects\":{}}}"));
 assert(dispatch("{\"id\":1001,\"result\":{\"objects\":[\"extruder\",42]}}"));
 assert(dispatch("{\"id\":1003,\"error\":{}}"));
 assert(dispatch("{\"id\":1003,\"result\":{\"status\":{\"configfile\":{\"config\":[]}}}}"));
 const char *bad[]={
  "{\"id\":1001.5,\"result\":{\"objects\":[\"extruder\"]}}",
  "{\"id\":1003.5,\"result\":{}}",
  "{\"id\":2000.5,\"error\":{}}", "{\"id\":1e999,\"error\":{}}",
  "{\"id\":4294967296,\"error\":{}}", "{\"id\":\"2000\",\"error\":{}}",
  "{\"id\":2000}{}", "{\"id\":2000}junk", "[{\"id\":2000}]"
 };
 for(size_t i=0;i<sizeof(bad)/sizeof(*bad);i++)assert(!dispatch(bad[i]));
 static const char hidden[]="{\"id\":2000}\0{\"id\":1001}";
 assert(!handle_websocket_response(hidden,sizeof(hidden)-1));
 char deep[100];strcpy(deep,"{\"id\":2000,\"result\":");
 size_t p=strlen(deep);memset(deep+p,'[',16);p+=16;deep[p++]='0';
 memset(deep+p,']',16);p+=16;deep[p++]='}';
 assert(!handle_websocket_response(deep,p));
 char large[MOONRAKER_WS_MESSAGE_CAPACITY];memset(large,' ',sizeof(large));
 memcpy(large,list,sizeof(list)-1);
 assert(!handle_websocket_response(large,sizeof(large)));
 assert(catalogs==4&&configs==3&&commands==1&&subscriptions==1);
 assert(s_subscription_ready&&s_subscribe_pending&&s_macro_config_pending);
 assert(dispatch("{\"id\":4294967295,\"result\":null}"));
 puts("PASS: strict WebSocket documents and IDs; invalid discovery preserves live capabilities");
}
'''
with tempfile.TemporaryDirectory(prefix='websocket-response-') as directory:
    p = Path(directory)
    (p / 'esp_err.h').write_text('typedef int esp_err_t;\n')
    (p / 'esp_http_client.h').write_text('typedef void *esp_http_client_handle_t; typedef struct esp_http_client_event esp_http_client_event_t;\n')
    (p / 'test.c').write_text(fixture + '\n'.join(function(n) for n in (
        'append_subscription_text', 'append_subscription_object', 'copy_text', 'object_list_contains', 'object_list_count_prefix', 'object_list_collect_prefix',
        'handle_tools_response', 'handle_command_response', 'handle_object_list_response',
        'handle_websocket_response')) + checks)
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-g',
                    '-fsanitize=address,undefined,float-cast-overflow', '-no-pie',
                    '-I', str(p), '-I', str(root / 'main'), '-I', str(vendor),
                    str(p / 'test.c'), str(vendor / 'cJSON.c'), '-o', str(p / 'test')], check=True)
    subprocess.run([str(p / 'test')], check=True,
                   env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0'})
