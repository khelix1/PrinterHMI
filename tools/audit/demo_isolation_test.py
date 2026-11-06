#!/usr/bin/env python3
"""Compile the real demo boundary, RAM transactions and printer runtime with SDK spies."""
from pathlib import Path
import argparse,os,subprocess,tempfile
root=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--cjson-dir',type=Path,required=True);args=p.parse_args()
with tempfile.TemporaryDirectory(prefix='printerhmi-demo-') as folder:
 t=Path(folder)
 (t/'esp_err.h').write_text('''#pragma once
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_ARG 1
#define ESP_ERR_NO_MEM 2
#define ESP_ERR_INVALID_STATE 3
#define ESP_ERR_NOT_SUPPORTED 4
#define ESP_ERR_NVS_NOT_FOUND 11
#define ESP_ERR_NVS_INVALID_HANDLE 12
#define ESP_ERR_NVS_TYPE_MISMATCH 13
#define ESP_ERR_NVS_INVALID_LENGTH 14
#define ESP_ERR_NVS_READ_ONLY 15
static inline const char *esp_err_to_name(esp_err_t error){(void)error;return "TEST_ERROR";}
''')
 nvs=(root/'tools/theme-maker/linux/native/shims/nvs.h').read_text().replace('#define ESP_ERR_NVS_NOT_FOUND 0x1102','').replace('typedef unsigned nvs_handle_t;','typedef unsigned nvs_handle_t;\ntypedef int nvs_open_mode_t;')
 (t/'nvs.h').write_text(nvs);(t/'nvs_flash.h').write_text('#pragma once\n#include "esp_err.h"\nesp_err_t nvs_flash_erase(void);\n')
 (t/'esp_http_client.h').write_text('''#pragma once
#include "esp_err.h"
typedef void *esp_http_client_handle_t;
typedef struct {int event_id;} esp_http_client_event_t;
typedef struct {int unused;} esp_http_client_config_t;
esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t *);
''')
 (t/'esp_websocket_client.h').write_text('''#pragma once
typedef void *esp_websocket_client_handle_t;
typedef struct {int unused;} esp_websocket_client_config_t;
esp_websocket_client_handle_t esp_websocket_client_init(const esp_websocket_client_config_t *);
''')
 (t/'esp_heap_caps.h').write_text('''#pragma once
#include <stdlib.h>
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
void *demo_test_calloc(size_t,size_t);
#define heap_caps_calloc(n,s,c) demo_test_calloc(n,s)
#define heap_caps_malloc(n,c) demo_test_calloc(1,n)
#define heap_caps_free(p) free(p)
''')
 (t/'esp_log.h').write_text('#pragma once\nstatic inline void demo_test_log(const char *format,...){(void)format;}\n#define ESP_LOGI(tag,...) ((void)(tag),demo_test_log(__VA_ARGS__))\n#define ESP_LOGW(tag,...) ((void)(tag),demo_test_log(__VA_ARGS__))\n#define ESP_LOGE(tag,...) ((void)(tag),demo_test_log(__VA_ARGS__))\n')
 sources=['demo_setup.c','demo_calibration.c','console_controller.c','calibration_session_controller.c','calibration_capability_controller.c','demo_camera.c','demo_camera_frame.c','camera_catalog_controller.c','demo_services.c','printer_simulation_catalog.c','device_catalog_controller.c','motion_diagnostics_controller.c','macro_controller.c','macro_parameter_utils.c','endstop_status_controller.c','demo_files.c','demo_storage.c','demo_platform.c','demo_printer.c','printer_simulation.c','moonraker_config_controller.c','case_lighting.c','bed_mesh_controller.c']
 flags=['cc','-std=c11','-D_POSIX_C_SOURCE=200809L','-DPRINTERHMI_DEMO_RUNTIME=1','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-include',str(root/'tools/theme-maker/linux/native/shims/host_compat.h'),'-I',str(t),'-I',str(root/'main'),'-I',str(args.cjson_dir),'-I',str(root/'tools/theme-maker/linux/native/shims')]
 objects=[]
 for name in sources:
  obj=t/(name+'.o');cmd=flags+['-include',str(root/'main/demo_platform.h')]
  if name=='demo_platform.c':cmd+=['-DPRINTERHMI_DEMO_PLATFORM_IMPLEMENTATION=1']
  subprocess.run(cmd+['-c',str(root/'main'/name),'-o',str(obj)],check=True);objects.append(str(obj))
 text=(root/'main/moonraker.c').read_text()
 pieces=[]
 for signature in ('static bool moonraker_fetch_thumbnail_encoded_internal(', 'bool moonraker_fetch_thumbnail_encoded_quiet(', 'bool moonraker_fetch_thumbnail_encoded_quiet_with_api_key(', 'bool moonraker_fetch_file_metadata(', 'bool moonraker_start_print_file('):
  start=text.index(signature);end=text.index('#endif',start)+len('#endif')
  pieces.append(text[start:end]+'\n ++demo_transport_live_calls;return false;\n}\n')
 ws=(root/'main/moonraker_live_websocket.c').read_text()
 for signature in ('bool moonraker_live_websocket_connected(', 'bool moonraker_live_websocket_request_endstops('):
  start=ws.index(signature);end=ws.index('#endif',start)+len('#endif');pieces.append(ws[start:end]+'\n ++demo_transport_live_calls;return false;\n}\n')
 (t/'demo_file_transport.inc').write_text('#pragma GCC diagnostic push\n#pragma GCC diagnostic ignored "-Wunused-parameter"\n'+''.join(pieces)+'#pragma GCC diagnostic pop\n')
 exe=t/'demo-test'
 subprocess.run(flags+['-DPRINTERHMI_DEMO_PLATFORM_IMPLEMENTATION=1',str(root/'tools/audit/demo_isolation_test.c'),str(args.cjson_dir/'cJSON.c'),*objects,'-lm','-no-pie','-o',str(exe)],check=True)
 env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1')
 for mode in ('live','demo','oom'):subprocess.run([str(exe),mode],check=True,env=env)
# Check integration placement, not just isolated test fixtures.
main=(root/'main/main.c').read_text();boot=main[main.index('void app_main(void)'):]
assert boot.index('demo_mode_init()')<boot.index('theme_manager_init()')<boot.index('demo_printer_init()')<boot.index('wifi_prepare_transport()')
for fn in ('wifi_prepare_transport','wifi_init_sta','init_sd_card_storage','app_start_runtime_services'):
 import re
 m=re.search(r'(?:static )?(?:void|bool) '+fn+r'\(void\)\n\{(.*?)\n\}',main,re.S)
 assert m and 'demo_mode_enabled()' in m[1][:220],fn+' lacks early demo boundary'
cmake=(root/'main/CMakeLists.txt').read_text();assert '-include${CMAKE_CURRENT_SOURCE_DIR}/demo_platform.h' in cmake
assert 'PRINTERHMI_DEMO_PLATFORM_IMPLEMENTATION=1' in cmake
# Refuse future NVS APIs that bypass the reviewed app boundary.
import re
allowed={'nvs_open','nvs_close','nvs_commit','nvs_erase_key','nvs_get_u8','nvs_set_u8','nvs_get_i32','nvs_set_i32','nvs_get_str','nvs_set_str','nvs_get_blob','nvs_set_blob','nvs_flash_init','nvs_flash_erase'}
used=set()
for path in (root/'main').glob('*.c'):
 if not path.name.startswith('demo_'):used.update(re.findall(r'\b(nvs_[a-z0-9_]+)\s*\(',path.read_text()))
assert used<=allowed,'Review new NVS APIs for demo isolation: '+repr(used-allowed)
print('PASS: live delegation, demo boot isolation, staged RAM settings, bounded/atomic failures, printer commands/scenarios/profile switching/lighting/mesh, and allocation failure')

# Every demo camera entry is intercepted before a worker or network client starts.
for name, functions in {
 'camera_stream_controller.c': ('camera_stream_start', 'camera_stream_busy', 'camera_stream_take_result', 'camera_stream_request_stop', 'camera_stream_stop'),
 'camera_test_controller.c': ('camera_test_start', 'camera_test_busy', 'camera_test_take_result'),
 'camera_discovery_controller.c': ('camera_discovery_start_for_profile', 'camera_discovery_busy', 'camera_discovery_take_result'),
}.items():
 text=(root/'main'/name).read_text()
 for fn in functions:
  match=re.search(r'(?:bool|void)\s+'+fn+r'\([^;]*?\)\s*\{(.*?)\}',text,re.S)
  assert match and 'demo_mode_enabled()' in match[1][:220], fn+' lacks early demo boundary'
print('PASS: camera entry points intercept demo operations before network workers')

# OTA demo paths must terminate before real OTA or network workers.
for filename, functions in {
 'ota_manager.c':('ota_manager_start','ota_manager_is_running','ota_manager_cancel','ota_manager_pump_ui'),
 'ota_release_catalog.c':('ota_release_catalog_start','ota_release_catalog_snapshot','ota_release_catalog_entry'),
 'ota_ui_controller.c':('ota_ui_controller_open_event_cb',),
}.items():
 text=(root/'main'/filename).read_text()
 for fn in functions:
  match=re.search(r'(?:bool|void)\s+'+fn+r'\([^;]*?\)\s*\{(.*?)\}',text,re.S)
  assert match and 'demo_mode_enabled()' in match[1][:220], fn+' lacks early demo guard'
print('PASS: OTA demo guards precede firmware and catalog workers')

# Setup must short-circuit Wi-Fi scans, status and printer discovery in demo.
setup=(root/'main/ui_setup_wizard.c').read_text()
for needle in ('demo_setup_network', 'demo_mode_enabled()', 'Demo network'):
 assert needle in setup
discovery=(root/'main/moonraker_discovery.c').read_text()
start=discovery[discovery.index('bool moonraker_discovery_start('):]
assert start.index('demo_mode_enabled()')<start.index('xTaskCreate(')
main=(root/'main/main.c').read_text()
start=main[main.index('static void app_open_setup_wizard(void)\n{'):];assert start.index('demo_setup_connect')<start.index('setup_connect_wifi')
print('PASS: Setup demo paths guard scan and discovery workers')

# Network demo callbacks must not reach real Wi-Fi or credential writes.
for fn,blocked in (('connect_selected_wifi','esp_wifi_disconnect'),('ui_network_tools_save_wifi_password_only_cb','wifi_credentials_store_save'),('ui_network_tools_wifi_scan_now','network_activity_controller_request_exclusive'),('scan_moonraker_now','if(!s_got_ip)')):
 match=re.search(r'(?:static )?(?:void|bool)\s+'+fn+r'\([^;]*?\)\s*\{(.*?)\n\}',main,re.S)
 assert match, fn
 body=match[1];assert body.index('demo_mode_enabled()')<body.index(blocked),fn
print('PASS: Network demo guards precede Wi-Fi and credential operations')
