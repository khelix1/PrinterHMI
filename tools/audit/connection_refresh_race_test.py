#!/usr/bin/env python3
"""Race-inject actual WebSocket JSON publication, expiry and runtime status selection."""
from pathlib import Path
import argparse
import os
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--main-source', type=Path, default=root / 'main/main.c')
parser.add_argument('--cjson-dir', type=Path, required=True)
args = parser.parse_args()
state = (root / 'main/moonraker.c').read_text()
main = args.main_source.read_text()
def function(source, name):
    mask = re.sub(r'/\*[\s\S]*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'', lambda m: ' ' * len(m[0]), source)
    hit = re.search(r'^[\w* ]+\b' + name + r'\s*\([^;{}]*\)\s*\{', mask, re.M)
    assert hit, name
    end, depth = hit.end(), 1
    while depth:
        depth += (mask[end] == '{') - (mask[end] == '}')
        end += 1
    return source[hit.start():end]

functions = ['mr_safe_copy', 'klipper_readiness_set', 'klipper_readiness_apply',
             'moonraker_state_snapshot', 'moonraker_state_reset',
             'moonraker_state_set_connection', 'moonraker_state_expire_websocket']
code = r'''
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "moonraker.h"
#include "moonraker_poll.h"
#include "cJSON.h"
#define ESP_LOGI(...) ((void)0)
static moonraker_state_t g_moonraker_state;
static moonraker_exclude_state_t *g_moonraker_exclude_state;
static moonraker_filament_state_t *g_moonraker_filament_state;
static int64_t s_websocket_status_us;
static bool s_virtual_sd_progress_known,s_klipper_ready,locked;
static char s_klipper_state[32]="--";
static int64_t now_us=1000000;
static void (*on_lock)(void);
static void state_lock(void){if(on_lock){void(*fn)(void)=on_lock;on_lock=NULL;fn();}assert(!locked);locked=true;}
static void state_unlock(void){assert(locked);locked=false;}
static int64_t esp_timer_get_time(void){assert(!on_lock);return now_us;}
static bool device_catalog_controller_merge_status(cJSON *s){(void)s;return false;}
static void motion_diagnostics_controller_reset(void){}
static bool motion_diagnostics_controller_merge_status(cJSON *s){(void)s;return false;}
static bool calibration_capability_controller_merge_status(cJSON *s){(void)s;return false;}
static bool bed_mesh_controller_merge_status(cJSON *s){(void)s;return false;}
'''
code += '\n'.join(function(state, name) for name in functions)
start = state.index('static cJSON *json_status_object(')
end = state.index('\n\nconst char *json_find_string(', start)
code += state[start:end]
code += r'''
static bool s_got_ip=true,s_moonraker_ok,s_live_data_ok;
static int s_moonraker_code;
static char moonraker_status[128];
static bool running=true,fresh_probe,inject_probe;
static unsigned polls;
static void safe_copy(char *p,size_t n,const char *s){snprintf(p,n,"%s",s);}
static void publish(const char *json){assert(moonraker_state_merge_websocket_json(json,strlen(json))!=MOONRAKER_WEBSOCKET_MESSAGE_IGNORED);}
static void printing(void){publish("{\"result\":{\"status\":{\"webhooks\":{\"state\":\"ready\"},\"print_stats\":{\"state\":\"printing\",\"filename\":\"same.gcode\"}}}}");}
static bool moonraker_live_websocket_fresh(int64_t age){assert(age==3000000);if(inject_probe){inject_probe=false;printing();}return fresh_probe;}
static bool moonraker_live_websocket_running(void){return running;}
static bool moonraker_get_live_objects(void){return true;}
moonraker_poll_result_t moonraker_poll_run(int64_t now,bool wifi,bool busy,moonraker_poll_fetch_cb_t fn){(void)now;(void)wifi;(void)busy;(void)fn;polls++;return MOONRAKER_POLL_BUSY;}
'''
if 's_moonraker_http_busy' in main:
    code += 'static bool s_moonraker_http_busy;\n'
if 's_last_moonraker_ok_us' in main:
    code += 'static int64_t s_last_moonraker_ok_us;\n'
code += function(main, 'moonraker_live_poll_tasklet')
code += r'''
int main(void){
 moonraker_state_reset();assert(!s_websocket_status_us);
 /* A frame publishes between the stale probe and the runtime expiry write. */
 inject_probe=true;moonraker_live_poll_tasklet();
 assert(g_moonraker_state.moonraker_ok&&g_moonraker_state.live_data_ok&&s_moonraker_ok&&s_live_data_ok&&!polls);
 /* A second publication can land immediately before the expiry lock, too.
    Identical values still count as a newly verified status frame. */
 now_us+=4000000;on_lock=printing;moonraker_live_poll_tasklet();
 assert(g_moonraker_state.moonraker_ok&&s_moonraker_ok&&s_websocket_status_us==now_us);
 now_us+=3000000;moonraker_live_poll_tasklet();assert(g_moonraker_state.moonraker_ok);
 now_us++;moonraker_live_poll_tasklet();assert(!g_moonraker_state.moonraker_ok&&!g_moonraker_state.live_data_ok&&!s_moonraker_ok&&!polls);
 printing();moonraker_live_poll_tasklet();assert(s_moonraker_ok&&s_live_data_ok);
 /* A real disconnect wins immediately; fresh-probe lag cannot report online. */
 moonraker_state_set_connection(false,false);assert(!s_websocket_status_us);fresh_probe=true;
 moonraker_live_poll_tasklet();assert(!s_moonraker_ok&&!s_live_data_ok&&!g_moonraker_state.moonraker_ok);
 fresh_probe=false;moonraker_state_reset();moonraker_live_poll_tasklet();assert(!s_moonraker_ok);
 printing();publish("{\"method\":\"notify_klippy_shutdown\"}");moonraker_live_poll_tasklet();
 assert(s_moonraker_ok&&!s_live_data_ok&&!strcmp(g_moonraker_state.printer_state,"shutdown"));
 publish("{\"method\":\"notify_klippy_disconnected\"}");moonraker_live_poll_tasklet();assert(!s_moonraker_ok&&!s_websocket_status_us);
 running=false;moonraker_live_poll_tasklet();assert(polls==1);
 /* Initial/future timestamps do not establish a valid recent sample. */
 g_moonraker_state.moonraker_ok=true;s_websocket_status_us=now_us+1;moonraker_state_expire_websocket(3000000);assert(!g_moonraker_state.moonraker_ok);
 puts("PASS: real JSON publication survives both injected stale-probe races, equal frames renew freshness, three-second expiry/disconnect/profile reset/shutdown remain truthful, no competing HTTP poll while WebSocket owns reconnect");
}
'''
with tempfile.TemporaryDirectory(prefix='connection-refresh-race-') as directory:
    tmp = Path(directory)
    (tmp / 'esp_err.h').write_text('#pragma once\ntypedef int esp_err_t;\n')
    (tmp / 'esp_http_client.h').write_text('#pragma once\ntypedef void *esp_http_client_handle_t;\ntypedef struct {int event_id;} esp_http_client_event_t;\n')
    (tmp / 'race.c').write_text(code)
    exe = tmp / 'race'
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-fno-pie', '-no-pie', '-I', str(tmp), '-I', str(root/'main'), '-I', str(args.cjson_dir), str(tmp/'race.c'), str(args.cjson_dir/'cJSON.c'), '-lm', '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True, env={**os.environ, 'ASAN_OPTIONS':'detect_leaks=0'})
