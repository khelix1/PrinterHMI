#!/usr/bin/env python3
"""Exercise authoritative printer-health freshness and endpoint reconciliation."""
from pathlib import Path
import argparse
import os
import subprocess
import tempfile
root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--source", type=Path, default=root/"main/printer_profile_health.c")
args = parser.parse_args()
fixture = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "moonraker_config_controller.h"
#include "printer_profile_health.h"
static uint32_t generation=1;
static int64_t now=1000000;
static moonraker_profile_t profiles[4]={{.configured=true,.host="a.local",.port=7125},{.configured=true,.host="b.local",.port=7126}};
size_t strlcpy(char*d,const char*s,size_t n){size_t len=strlen(s);if(n)snprintf(d,n,"%s",s);return len;}
uint32_t moonraker_config_generation(void){return generation;}
const moonraker_profile_t *moonraker_config_profile(int i){assert(i>=0&&i<4);return &profiles[i];}
int64_t esp_timer_get_time(void){return now;}
int main(void){
 printer_profile_health_reconcile();
 bool known=true;char state[32];
 assert(!printer_profile_health_get(1,&known)&&!known);
 printer_profile_health_set_live_state(1,true,true,"printing");
 assert(printer_profile_health_get(1,&known)&&known);
 assert(printer_profile_health_live_state_fresh(1,5000000));
 assert(printer_profile_health_get_live_state(1,state,sizeof(state))&&!strcmp(state,"printing"));
 now+=5000001;assert(!printer_profile_health_live_state_fresh(1,5000000));
 /* Demo ticks and other authoritative live publishers refresh their timestamp. */
 printer_profile_health_set_live_state(1,true,true,"paused");
 assert(printer_profile_health_live_state_fresh(1,5000000));
 generation++;printer_profile_health_reconcile();
 assert(printer_profile_health_get(1,&known)&&known);
 assert(printer_profile_health_live_state_fresh(1,5000000));
 printer_profile_health_set(1,true,false);
 assert(!printer_profile_health_live_state_fresh(1,5000000));
 assert(!printer_profile_health_get_live_state(1,state,sizeof(state)));
 printer_profile_health_set_live_state(1,false,true,"printing");
 assert(!printer_profile_health_get(1,&known)&&!known);
 assert(!printer_profile_health_live_state_fresh(1,5000000));
 /* Background misses keep the original successful timestamp, not a new one. */
 printer_profile_health_report_live_state(1,true,"printing");
 now+=5000001;printer_profile_health_report_live_state(1,false,NULL);
 assert(printer_profile_health_get(1,&known)&&known);
 assert(!printer_profile_health_live_state_fresh(1,5000000));
 printer_profile_health_report_live_state(1,false,NULL);
 assert(!printer_profile_health_get(1,&known)&&known);
 printer_profile_health_report_live_state(1,true,"standby");
 assert(printer_profile_health_live_state_fresh(1,5000000));
 profiles[1].port++; /* Inactive edit does not change active generation. */
 assert(!printer_profile_health_get(1,&known)&&!known);
 assert(!printer_profile_health_live_state_fresh(1,5000000));
 assert(!printer_profile_health_get_live_state(1,state,sizeof(state))&&!state[0]);
 (void)printer_profile_health_take_next_index();
 assert(!printer_profile_health_get(1,&known)&&!known);
 assert(!printer_profile_health_live_state_fresh(1,5000000));
 printer_profile_health_report_live_state(1,true,"paused");
 assert(printer_profile_health_get(1,&known)&&known&&printer_profile_health_live_state_fresh(1,5000000));
 puts("PASS: authoritative/demo freshness, explicit offline/unknown clearing, preserved endpoint health and background failure hysteresis");
}
'''
with tempfile.TemporaryDirectory(prefix="health-freshness-") as directory:
    tmp = Path(directory)
    (tmp/"esp_log.h").write_text('#pragma once\n#include <stdio.h>\n#define ESP_LOGI(tag,...) do{(void)(tag);if(0)printf(__VA_ARGS__);}while(0)\n#define ESP_LOGW ESP_LOGI\n')
    (tmp/"esp_timer.h").write_text('#pragma once\n#include <stdint.h>\nint64_t esp_timer_get_time(void);\n')
    # Expose the same bounded copy declaration that ESP-IDF supplies.
    (tmp/"compat.h").write_text('#include <stddef.h>\nsize_t strlcpy(char*,const char*,size_t);\n')
    (tmp/"test.c").write_text(fixture)
    exe = tmp/"test"
    subprocess.run(["cc", "-std=c11", "-D_POSIX_C_SOURCE=200809L", "-O1", "-g",
                    "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined",
                    "-fno-pie", "-no-pie", "-include", str(tmp/"compat.h"),
                    "-I", str(tmp), "-I", str(root/"main"),
                    str(tmp/"test.c"), str(args.source), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True,
                   env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0"})
