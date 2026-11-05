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
             'print_state_is_active', 'moonraker_state_snapshot', 'moonraker_state_reset',
             'moonraker_state_publish_http_fallback',
             'moonraker_state_set_connection', 'moonraker_state_expire_websocket']
code = r'''
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "moonraker.h"
#include "moonraker_poll.h"
#include "moonraker_websocket_json.h"
#include "moonraker_telemetry_number.h"
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
end = state.index('\n\nbool moonraker_find_thumbnail_path(', start)
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
 static moonraker_exclude_state_t objects;
 g_moonraker_exclude_state=&objects;
 printing();
 publish("{\"result\":{\"status\":{\"virtual_sdcard\":{\"progress\":0.4,\"file_position\":300},\"display_status\":{\"progress\":0.6},\"print_stats\":{\"filament_used\":50}}}}");
 print_estimate_sample_t sample = g_moonraker_state.estimate_sample;
 assert(sample.file_valid&&sample.file_progress==.4&&sample.position_valid&&sample.file_position==300);
 assert(sample.slicer_valid&&sample.slicer_progress==.6&&sample.filament_valid&&sample.filament_used==50);
 uint32_t job=g_moonraker_state.print_job_revision;
 publish("{\"result\":{\"status\":{\"print_stats\":{\"state\":\"paused\"}}}}");
 assert(g_moonraker_state.print_job_revision==job);
 publish("{\"result\":{\"status\":{\"print_stats\":{\"filename\":\"replacement.gcode\"}}}}");
 assert(g_moonraker_state.print_job_revision!=job);
 job=g_moonraker_state.print_job_revision;
 moonraker_http_fallback_update_t http={.live_data_ok=true,.moonraker_ok=true,.printer_state="printing",.printer_file="replacement.gcode"};
 http.estimate_sample=(print_estimate_sample_t){.file_valid=true,.file_progress=.2};
 moonraker_state_publish_http_fallback(&http);assert(g_moonraker_state.print_job_revision==job);
 assert(g_moonraker_state.estimate_sample.file_progress==.2&&!g_moonraker_state.estimate_sample.slicer_valid);
 http.printer_state="complete";moonraker_state_publish_http_fallback(&http);
 http.printer_state="printing";moonraker_state_publish_http_fallback(&http);
 assert(g_moonraker_state.print_job_revision!=job);
 publish("{\"result\":{\"status\":{\"toolhead\":{\"position\":[1,2,3]},\"fan\":{\"speed\":0.5},\"heater_bed\":{\"target\":60},\"gcode_macro DRYBOX_VARS\":{\"selected_program\":2,\"active_program\":3}}}}");
 assert(g_moonraker_state.toolhead_z==3&&g_moonraker_state.part_fan_speed==50&&g_moonraker_state.bed_target==60);
 assert(g_moonraker_state.drybox_selected_program==2&&g_moonraker_state.drybox_active_program==3);
 const char bad[]="{\"result\":{\"status\":{\"toolhead\":{\"position\":[9,8,1e999]},\"fan\":{\"speed\":1e308},\"heater_bed\":{\"target\":1e20},\"gcode_macro DRYBOX_VARS\":{\"selected_program\":1.5,\"active_program\":1e999}}}}";
 assert(moonraker_state_merge_websocket_json(bad,sizeof(bad)-1)==MOONRAKER_WEBSOCKET_MESSAGE_IGNORED);
 assert(g_moonraker_state.toolhead_x==1&&g_moonraker_state.toolhead_z==3&&g_moonraker_state.part_fan_speed==50&&g_moonraker_state.bed_target==60);
 assert(g_moonraker_state.drybox_selected_program==2&&g_moonraker_state.drybox_active_program==3);
 publish("{\"result\":{\"status\":{\"exclude_object\":{\"objects\":[{\"name\":\"part\",\"center\":[1e999,2],\"polygon\":[[1,2],[1e999,3]]}]}}}}");
 assert(objects.object_count==1&&!objects.objects[0].has_center&&objects.objects[0].polygon_count==1);
 moonraker_state_reset();
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
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-O1', '-fsanitize=address,undefined,float-cast-overflow', '-fno-omit-frame-pointer', '-fno-pie', '-no-pie', '-I', str(tmp), '-I', str(root/'main'), '-I', str(args.cjson_dir), str(tmp/'race.c'), str(args.cjson_dir/'cJSON.c'), '-lm', '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True, env={**os.environ, 'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'})
