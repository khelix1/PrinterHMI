#!/usr/bin/env python3
"""Run the actual background loader with deterministic HTTP/task/display adapters."""
from pathlib import Path
import subprocess,tempfile,os
root=Path(__file__).resolve().parents[2]
headers={
'bsp/esp-bsp.h':'#pragma once\n#include <stdbool.h>\nbool bsp_display_lock(unsigned);void bsp_display_unlock(void);\n',
'esp_heap_caps.h':'#pragma once\n#include <stddef.h>\n#define MALLOC_CAP_SPIRAM 1\n#define MALLOC_CAP_8BIT 2\nvoid *test_calloc(size_t,size_t);void test_free(void *);\n#define heap_caps_calloc(n,s,c) test_calloc(n,s)\n#define heap_caps_free(p) test_free(p)\n',
'esp_timer.h':'#pragma once\n#include <stdint.h>\nint64_t esp_timer_get_time(void);\n',
'freertos/FreeRTOS.h':'#pragma once\n#include <stdint.h>\ntypedef int portMUX_TYPE;\n#define portMUX_INITIALIZER_UNLOCKED 0\n#define taskENTER_CRITICAL(x) ((void)(x))\n#define taskEXIT_CRITICAL(x) ((void)(x))\n#define pdPASS 1\n',
'freertos/task.h':'#pragma once\nint xTaskCreatePinnedToCore(void (*)(void *),const char*,unsigned,void*,int,void*,int);void vTaskDelete(void*);void vTaskDelay(unsigned);\n',
'esp_http_client.h':'''#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef void *esp_http_client_handle_t;
typedef struct {const char *url,*cert_pem;bool disable_auto_redirect;int method,timeout_ms,buffer_size;} esp_http_client_config_t;
#define HTTP_METHOD_GET 0
#define ESP_OK 0
esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t*);
int esp_http_client_set_header(esp_http_client_handle_t,const char*,const char*);
int esp_http_client_open(esp_http_client_handle_t,int);
int64_t esp_http_client_fetch_headers(esp_http_client_handle_t);
int esp_http_client_get_status_code(esp_http_client_handle_t);
int esp_http_client_read(esp_http_client_handle_t,char*,int);
bool esp_http_client_is_complete_data_received(esp_http_client_handle_t);
int esp_http_client_close(esp_http_client_handle_t);
int esp_http_client_cleanup(esp_http_client_handle_t);
'''}
with tempfile.TemporaryDirectory(prefix='gcode-loader-') as d:
 tmp=Path(d)
 for name,text in headers.items():
  p=tmp/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text(text)
 exe=tmp/'test'
 subprocess.run(['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I',str(tmp),'-I',str(root/'main'),str(root/'main/gcode_toolpath.c'),str(root/'main/gcode_viewer_loader.c'),str(root/'main/gcode_surface.c'),str(root/'main/gcode_cache.c'),str(root/'main/gcode_volume.c'),str(root/'main/mesh_preview.c'),str(root/'tools/audit/gcode_viewer_loader_test.c'),'-lm','-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})

with tempfile.TemporaryDirectory(prefix='gcode-cached-loader-') as d:
 tmp=Path(d)
 for name,text in headers.items():
  p=tmp/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text(text)
 exe=tmp/'test'
 subprocess.run(['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',f'-DGCODE_CACHE_DIRECTORY="{d}/cache"','-I',str(tmp),'-I',str(root/'main'),*[str(root/'main'/s) for s in ['gcode_toolpath.c','gcode_surface.c','gcode_cache.c','gcode_volume.c','mesh_preview.c','gcode_viewer_loader.c']],str(root/'tools/audit/gcode_cached_loader_test.c'),'-lm','-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})

with tempfile.TemporaryDirectory(prefix='gcode-replay-refine-') as d:
 tmp=Path(d)
 for name,text in headers.items():
  p=tmp/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text(text)
 exe=tmp/'test'
 subprocess.run(['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I',str(tmp),'-I',str(root/'main'),str(root/'main/gcode_toolpath.c'),str(root/'main/gcode_viewer_loader.c'),str(root/'main/gcode_surface.c'),str(root/'main/gcode_cache.c'),str(root/'main/gcode_volume.c'),str(root/'main/mesh_preview.c'),str(root/'tools/audit/gcode_replay_refine_test.c'),'-lm','-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
