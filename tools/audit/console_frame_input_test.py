#!/usr/bin/env python3
"""Test actual Console frame parsing with nonterminated and hostile buffers."""
from pathlib import Path
import argparse
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--source', type=Path, default=root / 'main/console_controller.c')
args = parser.parse_args()
cjson = root / 'tools/theme-maker/linux/native/vendor/cjson'
headers = {
    'esp_heap_caps.h': '#pragma once\n#include <stdlib.h>\n#define MALLOC_CAP_SPIRAM 1\n#define MALLOC_CAP_INTERNAL 2\n#define MALLOC_CAP_8BIT 4\n#define heap_caps_calloc(n,s,c) calloc(n,s)\n',
    'esp_log.h': '#pragma once\n#define ESP_LOGI(tag,...) ((void)(tag))\n#define ESP_LOGE(tag,...) ((void)(tag))\n',
    'esp_timer.h': '#pragma once\n#include <stdint.h>\nstatic inline int64_t esp_timer_get_time(void){return 0;}\n',
    'freertos/FreeRTOS.h': '#pragma once\ntypedef int portMUX_TYPE;\n#define portMUX_INITIALIZER_UNLOCKED 0\n#define portENTER_CRITICAL(lock) ((void)(lock))\n#define portEXIT_CRITICAL(lock) ((void)(lock))\n',
}
fixture = r'''
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "console_controller.h"
static bool ingest(const char *s){return console_controller_ingest_websocket_json(s,strlen(s));}
int main(void){
 assert(console_controller_init());console_controller_clear();
 char *raw=malloc(3);assert(raw);memcpy(raw,"abc",3);assert(!console_controller_ingest_websocket_json(raw,3));free(raw);
 const char *valid="{\"method\":\"notify_gcode_response\",\"params\":[\"ok\\n!! failed\"]}";
 size_t n=strlen(valid);raw=malloc(n);assert(raw);memcpy(raw,valid,n);
 assert(console_controller_ingest_websocket_json(raw,n));free(raw);assert(console_controller_count()==2);
 console_entry_t entry;assert(console_controller_get(0,&entry)&&entry.type==CONSOLE_ENTRY_ERROR&&!strcmp(entry.message,"!! failed"));
 unsigned count=console_controller_count();
 assert(!ingest("{\"method\":\"notify_gcode_response\",\"params\":[\"bad\"]}{}"));
 assert(!ingest("{\"method\":\"notify_gcode_response\",\"params\":[\"bad\"]}junk"));
 assert(!ingest("{\"method\":\"notify_gcode_response\""));
 char binary[]="{\"method\":\"notify_gcode_response\",\"params\":[\"bad\"]}\0hidden";
 assert(!console_controller_ingest_websocket_json(binary,sizeof(binary)-1));
 assert(!console_controller_ingest_websocket_json(valid,5));
 assert(!console_controller_ingest_websocket_json(valid,128*1024+1));
 assert(!ingest("{\"method\":\"other\",\"params\":[\"notify_gcode_response\"]}"));
 char deep[256];size_t at=0;for(int i=0;i<32;i++)deep[at++]='[';
 memcpy(deep+at,valid,n);at+=n;for(int i=0;i<32;i++)deep[at++]=']';
 assert(!console_controller_ingest_websocket_json(deep,at));assert(console_controller_count()==count);
 assert(ingest("{\"method\":\"notify_gcode_response\",\"params\":[\"fine\"]} \r\n"));
 assert(console_controller_count()==count+1);
 char command[CONSOLE_COMMAND_MAX+2], history[CONSOLE_COMMAND_MAX+1], short_history[CONSOLE_COMMAND_MAX];
 memset(command,'A',sizeof(command)-1);command[sizeof(command)-1]=0;
 console_controller_add_command(command);assert(!console_controller_history_count());
 command[CONSOLE_COMMAND_MAX]=0;console_controller_add_command(command);
 assert(console_controller_history_count()==1&&console_controller_history_get(0,history,sizeof(history))&&!strcmp(history,command));
 assert(!console_controller_history_get(0,short_history,sizeof(short_history))&&!short_history[0]);
 puts("PASS: Console respects exact buffer lengths, accepts nonterminated frames and rejects malformed/deep/oversized/trailing input without log changes");
}
'''
with tempfile.TemporaryDirectory(prefix='console-frame-') as directory:
    p = Path(directory)
    for name, content in headers.items():
        path = p / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content)
    (p / 'test.c').write_text(fixture)
    subprocess.run(['cc', '-std=c11', '-D_POSIX_C_SOURCE=200809L', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-fno-pie', '-no-pie',
                    '-I', str(p), '-I', str(root / 'main'), '-I', str(cjson),
                    str(p / 'test.c'), str(args.source), str(cjson / 'cJSON.c'),
                    '-o', str(p / 'test')], check=True)
    subprocess.run([str(p / 'test')], check=True,
                   env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0'})
