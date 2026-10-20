#!/usr/bin/env python3
"""Compile main.c's actual profile-discovery bridges against request-aware SDK mocks."""
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / "main/main.c").read_text()
mask = re.sub(r'/\*[\s\S]*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',
              lambda match: " " * len(match[0]), source)


def function(name):
    match = re.search(r"static void " + name + r"\s*\([^;]*?\)\s*\{", mask)
    if match is None:
        raise RuntimeError("Missing main bridge: " + name)
    position = match.end()
    depth = 1
    while depth:
        depth += (mask[position] == "{") - (mask[position] == "}")
        position += 1
    return source[match.start():position]


fixture = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "moonraker_discovery.h"
static bool s_got_ip,running,show_ok=true,start_ok=true,replace_on_apply,retire_on_show;
static esp_ip4_addr_t s_ip;
static uint32_t current_request,shown_request,applied_request,open_request;
static unsigned starts,shows,closes,applies;
static char status[160];
static moonraker_discovery_select_request_cb_t selected_callback;
static moonraker_discovery_close_request_cb_t closed_callback;
uint32_t ui_printer_profiles_discovery_request(void){return current_request;}
void ui_printer_profiles_cancel_discovery_request(uint32_t request){if(request==current_request)current_request=0;}
bool ui_printer_profiles_apply_discovered_endpoint(uint32_t request,const char *host,int port,const char *identity)
{
    assert(host[0]&&port>0&&identity[0]);applies++;applied_request=request;
    if(!request||request!=current_request)return false;
    current_request=0;if(replace_on_apply)open_request=99;return true;
}
bool moonraker_discovery_show_for_request(const char *text,moonraker_discovery_close_request_cb_t close,
    moonraker_discovery_select_request_cb_t select,uint32_t request)
{
    assert(text&&close&&select&&request);shows++;shown_request=request;selected_callback=select;closed_callback=close;
    snprintf(status,sizeof(status),"%s",text);
    if(!show_ok)return false;
    open_request=request;if(retire_on_show)current_request=0;return true;
}
void moonraker_discovery_close_request(uint32_t request)
{
    if(request&&request==open_request){closes++;open_request=0;if(closed_callback)closed_callback(request);}
}
bool moonraker_discovery_is_running(void){return running;}
bool moonraker_discovery_start(const esp_ip4_addr_t *ip){assert(ip==&s_ip);starts++;return start_ok;}
void moonraker_discovery_set_status(const char *text){snprintf(status,sizeof(status),"%s",text);}
static void moonraker_discovery_selected_bridge(const char *,int,const char *,uint32_t);
'''
fixture += function("scan_moonraker_now") + "\n" + function("moonraker_discovery_selected_bridge")
fixture += r'''
int main(void)
{
    scan_moonraker_now();assert(!shows&&!starts);
    current_request=7;scan_moonraker_now();assert(shown_request==7&&!starts&&strstr(status,"not connected"));
    s_got_ip=true;running=true;scan_moonraker_now();assert(!starts&&strstr(status,"finishing"));running=false;
    show_ok=false;scan_moonraker_now();assert(!starts);show_ok=true;
    retire_on_show=true;scan_moonraker_now();assert(!starts&&!open_request&&closes==1);retire_on_show=false;
    current_request=8;scan_moonraker_now();assert(starts==1&&shown_request==8&&selected_callback);
    selected_callback("selected.local",7125,"Selected",8);assert(applied_request==8&&!current_request&&!open_request);
    current_request=9;start_ok=false;scan_moonraker_now();assert(starts==2&&strstr(status,"Unable"));start_ok=true;
    current_request=10;open_request=10;selected_callback("old.local",7125,"Old",9);assert(current_request==10&&open_request==10);
    replace_on_apply=true;selected_callback("selected.local",7125,"Selected",10);assert(!current_request&&open_request==99);
    unsigned before=applies;selected_callback("",0,"Invalid",10);assert(applies==before&&open_request==99);
    puts("PASS: main discovery bridge request capture, Wi-Fi/busy/start failures, Show replacement, retired drafts and stale-close isolation");
}
'''
with tempfile.TemporaryDirectory(prefix="printerhmi-discovery-bridge-") as directory:
    tmp = Path(directory)
    (tmp / "esp_netif_ip_addr.h").write_text(
        "#pragma once\n#include <stdint.h>\ntypedef struct { uint32_t addr; } esp_ip4_addr_t;\n")
    (tmp / "bridge.c").write_text(fixture)
    executable = tmp / "bridge"
    subprocess.run(["cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
                    "-I", str(tmp), "-I", str(root / "main"), str(tmp / "bridge.c"),
                    "-o", str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
