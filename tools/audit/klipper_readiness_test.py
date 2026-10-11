#!/usr/bin/env python3
"""Test actual readiness helpers and their lifecycle/status integration."""
from pathlib import Path
import subprocess,tempfile,re
root=Path(__file__).resolve().parents[2];source=(root/'main/moonraker.c').read_text()
def function(name):
 match=re.search(r'^(?:static )?\w+(?: \*)? '+name+r'\(',source,re.M);assert match,name;start=match.start();return source[start:source.index('\n}\n',start)+3]
code='''#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
static bool s_klipper_ready;
static char s_klipper_state[32]="--";
static struct {bool live_data_ok;char printer_state[32];} g_moonraker_state;
'''+function('mr_safe_copy')+function('klipper_readiness_set')+function('klipper_readiness_apply')+'''
static void partial(const char*print_state){g_moonraker_state.live_data_ok=true;snprintf(g_moonraker_state.printer_state,32,"%s",print_state);klipper_readiness_apply();}
int main(void){
 klipper_readiness_set("--");partial("printing");assert(!g_moonraker_state.live_data_ok&&!strcmp(g_moonraker_state.printer_state,"--"));
 klipper_readiness_set("ready");partial("printing");assert(g_moonraker_state.live_data_ok&&!strcmp(g_moonraker_state.printer_state,"printing"));
 const char*unavailable[]={"shutdown","error","disconnected","startup"};
 for(unsigned i=0;i<4;i++){
  klipper_readiness_set(unavailable[i]);
  for(unsigned j=0;j<100;j++){partial("printing");assert(!g_moonraker_state.live_data_ok&&!strcmp(g_moonraker_state.printer_state,unavailable[i]));}
  klipper_readiness_set("ready");partial("standby");assert(g_moonraker_state.live_data_ok&&!strcmp(g_moonraker_state.printer_state,"standby"));
 }
 puts("PASS: unverified bootstrap blocked, readiness latched across partial telemetry, shutdown/error/disconnected/startup persist and explicit ready restores");}
'''
# Check these tested helpers are used at real publication/lifecycle boundaries.
merge=function('moonraker_state_merge_websocket_json')
assert 'klipper_readiness_set("shutdown")' in merge and 'klipper_readiness_set("disconnected")' in merge and 'klipper_readiness_set("ready")' in merge
assert merge.index('klipper_readiness_apply();')>merge.index('g_moonraker_state.live_data_ok = true;')
assert 'klipper_readiness_set(klipper_state->valuestring)' in merge
assert 'klipper_readiness_set("--")' in function('moonraker_state_reset')
ws=(root/'main/moonraker_live_websocket.c').read_text();assert '\\"webhooks\\":[\\"state\\",\\"state_message\\"]' in ws
http=(root/'main/moonraker_live_transport.c').read_text();assert '&toolhead&webhooks' in http
main=(root/'main/main.c').read_text();assert main.index('if (!klipper_ready)')<main.index('    double val = 0.0;',main.index('static bool moonraker_get_live_objects(void)\n{'))
with tempfile.TemporaryDirectory(prefix='klipper-readiness-') as t:
 p=Path(t);(p/'test.c').write_text(code)
 subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
