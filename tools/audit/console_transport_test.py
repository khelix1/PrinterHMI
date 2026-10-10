#!/usr/bin/env python3
"""Verify actual Console routing keeps literal WS/HTTP fallback and inline-owned failures."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[2]
s=(root/'main/main.c').read_text()
def function(name):
 pos=s.index('static bool '+name+'(');return s[pos:s.index('\n}\n',pos)+3]
assert 'ui_console_show(\n            console_send_gcode);' in s
fixture=r'''
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
typedef int esp_err_t;
#define ESP_FAIL -1
#define UI_STATUS_DANGER 4
#define UI_STATUS_OK 2
#define UI_SHELL_PAGE_CONSOLE 5
#define CONSOLE_ENTRY_SYSTEM 0
#define TAG "fixture"
#define ESP_LOGI(...) ((void)0)
static bool s_got_ip=true,ws_accept,http_accept;
static char moonraker_status[320],command[320];
static unsigned ws_calls,http_calls,toasts;
static void safe_copy(char *d,size_t n,const char *v){snprintf(d,n,"%s",v);}
static const char *moonraker_config_host(void){return "fixture-host";}
static int moonraker_config_port(void){return 7126;}
static const char *moonraker_config_api_key(void){return "fixture-key";}
static bool moonraker_live_websocket_send_gcode(const char *v){ws_calls++;snprintf(command,sizeof(command),"%s",v);return ws_accept;}
static bool moonraker_send_gcode_script(const char *h,int p,const char *k,const char *v,int *code,esp_err_t *err){assert(!strcmp(h,"fixture-host")&&p==7126&&!strcmp(k,"fixture-key"));http_calls++;snprintf(command,sizeof(command),"%s",v);*code=http_accept?200:503;*err=http_accept?0:ESP_FAIL;return http_accept;}
static void ui_toast_show_link(int k,const char *t,const char *d,int page){(void)k;(void)t;(void)d;assert(page==UI_SHELL_PAGE_CONSOLE);toasts++;}
typedef struct { bool macro_used; char command[320]; } printer_action_resolution_t;
static bool printer_action_resolver_resolve(const char *requested,printer_action_resolution_t *out){if(!requested)return false;out->macro_used=!strcmp(requested,"G28");snprintf(out->command,sizeof(out->command),"%s",out->macro_used?"HOME_MACHINE":requested);return true;}
static const char *moonraker_command_admin_method(const char *requested){return requested&&!strcmp(requested,"FIRMWARE_RESTART")?"firmware_restart":NULL;}
static void console_controller_add(int kind,const char *format,...){(void)kind;(void)format;}
static void ui_printer_banner_show_notice(const char *n,int k,int ms){(void)n;(void)k;(void)ms;}
'''
checks=r'''
int main(void){
 assert(!console_send_gcode(NULL)&&!console_send_gcode(""));assert(!ws_calls&&!http_calls&&!toasts);
 ws_accept=true;assert(console_send_gcode("G28"));assert(!strcmp(command,"G28")&&ws_calls==1&&!http_calls&&!toasts);
 ws_accept=false;http_accept=true;assert(console_send_gcode("M112"));assert(!strcmp(command,"M112")&&http_calls==1&&!toasts);
 http_accept=false;assert(!console_send_gcode("RESTART"));assert(!strcmp(command,"RESTART")&&http_calls==2&&!toasts);
 s_got_ip=false;assert(!console_send_gcode("STATUS"));assert(http_calls==2&&!toasts);
 assert(!moonraker_send_gcode_raw("STATUS"));assert(toasts==1);
 s_got_ip=true;assert(!moonraker_send_gcode_raw("STATUS"));assert(http_calls==3&&toasts==2);
 unsigned prior=toasts;
 assert(!action_send_gcode_inline("G28"));assert(!strcmp(command,"HOME_MACHINE")&&toasts==prior);
 assert(!action_send_gcode_inline("FIRMWARE_RESTART"));assert(!strcmp(command,"FIRMWARE_RESTART")&&toasts==prior);
 assert(!moonraker_send_gcode("G28"));assert(!strcmp(command,"HOME_MACHINE")&&toasts==prior+1);
 puts("PASS: actual Console literal dispatch, WS/HTTP fallback, host/port/key, no duplicate failure toast; other callers retain feedback");return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='console-transport-') as tmp:
 p=Path(tmp);(p/'test.c').write_text(fixture+'\n'+ '\n'.join(function(n) for n in ['moonraker_send_gcode_http','moonraker_send_gcode_raw','console_send_gcode','moonraker_send_gcode_impl','moonraker_send_gcode','action_send_gcode_inline'])+checks)
 subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror','-Wrestrict',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
