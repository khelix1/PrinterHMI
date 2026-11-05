#!/usr/bin/env python3
"""Test the actual file parser and metadata formatter with malformed input."""
from pathlib import Path
import argparse
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--source', type=Path, default=root / 'main/printer_files.c')
args = parser.parse_args()
fixture = r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "cJSON.h"
#include "printer_files.h"
#include "moonraker_http_objects.h"
#include "moonraker_file_metadata.h"
'''
checks = r'''
int main(void){
 char out[1024],thumb[192];
 double bad[]={INFINITY,NAN,1e100,-1};for(unsigned i=0;i<4;i++){printer_files_seconds_to_hhmm(bad[i],out,sizeof(out));assert(!strcmp(out,"--:--"));}
 printer_files_seconds_to_hhmm(3661,out,sizeof(out));assert(!strcmp(out,"1:01"));
 assert(printer_files_build_metadata_text("test.gcode","{\"result\":{\"estimated_time\":1e999,\"filament_total\":1e999,\"layer_height\":1e100,\"object_height\":-1,\"size\":1e100}}",thumb,sizeof(thumb),out,sizeof(out)));
 assert(strstr(out,"ETA: --:--")&&!strstr(out,"inf")&&!strstr(out,"nan"));
 assert(printer_files_build_metadata_text("folder/sub/part.gcode","{\"result\":{\"estimated_time\":3600,\"thumbnails\":[{\"relative_path\":\".thumbs/part.png\",\"width\":300,\"height\":300}]}}",thumb,sizeof(thumb),out,sizeof(out)));
 assert(!strcmp(thumb,"folder/sub/.thumbs/part.png")&&strstr(out,"ETA: 1:00")&&strstr(out,"Thumbnail: metadata found"));
 const char mixed[]="{\"result\":{\"thumbnails\":["
   "{\"relative_path\":\"../outside.png\",\"width\":999,\"height\":999},"
   "{\"relative_path\":\".thumbs/long-name.png\",\"width\":900,\"height\":500},"
   "{\"relative_path\":\"ok.png\",\"width\":300,\"height\":300}]}}";
 char short_path[18];
 assert(printer_files_build_metadata_text("folder/part.gcode",mixed,short_path,sizeof(short_path),out,sizeof(out)));
 assert(!strcmp(short_path,"folder/ok.png")&&strstr(out,"Thumbnail: metadata found"));
 assert(moonraker_metadata_thumbnail_path(mixed,"folder/part.gcode",0,thumb,sizeof(thumb)));
 assert(!strcmp(thumb,"folder/.thumbs/long-name.png"));
 assert(!moonraker_metadata_thumbnail_path(mixed,"../folder/part.gcode",0,thumb,sizeof(thumb))&&!thumb[0]);
 assert(printer_files_build_metadata_text("test.gcode","{\"estimated_time\":7200,\"result\":{\"other\":{\"estimated_time\":10800}}}",thumb,sizeof(thumb),out,sizeof(out)));
 assert(strstr(out,"ETA: --:--"));
 assert(!printer_files_build_metadata_text("test.gcode","{\"result\":{}}junk",thumb,sizeof(thumb),out,sizeof(out))&&!thumb[0]);
 assert(!printer_files_build_metadata_text("test.gcode","{\"error\":{\"estimated_time\":7200}}",thumb,sizeof(thumb),out,sizeof(out)));
 const char live[]="{\"result\":{\"status\":{\"extruder\":{\"temperature\":201.5},\"heater_bed\":{\"target\":60},\"fan\":{\"speed\":1e308},\"gcode_move\":{\"speed_factor\":1.2}}}}";
 const cJSON *status;
 cJSON *response=moonraker_http_objects_parse(live,sizeof(live)-1,4095,&status);assert(response&&status);
 double value=-1;
 assert(!moonraker_http_object_number(status,"extruder","target",1,&value)&&value==-1);
 assert(moonraker_http_object_number(status,"heater_bed","target",1,&value)&&value==60);
 assert(!moonraker_http_object_number(status,"fan","speed",100,&value)&&value==60);
 assert(moonraker_http_object_number(status,"gcode_move","speed_factor",100,&value)&&value==120);
 cJSON_Delete(response);
 assert(!moonraker_http_objects_parse(live,sizeof(live),4095,&status)&&!status);
 assert(!moonraker_http_objects_parse(live,sizeof(live)-1,20,&status)&&!status);
 const char wrong[]="{\"result\":{\"status\":[]}}";
 assert(!moonraker_http_objects_parse(wrong,sizeof(wrong)-1,4095,&status)&&!status);
 printer_file_entry_t entries[3];memset(entries,0,sizeof(entries));
 assert(printer_files_parse_entries("{\"result\":[7,{\"path\":\"quote\\\"back\\\\slash.gcode\",\"size\":1e999,\"modified\":-1}]}",entries,3)==1);
 assert(!strcmp(entries[0].path,"quote\"back\\slash.gcode")&&entries[0].size==0&&entries[0].modified==0);
 const char *bad_lists[]={"{\"result\":[]}{\"path\":\"fake.gcode\"}","{\"result\":[{\"path\":\"fake.gcode\"}]", "{\"error\":{\"path\":\"fake.gcode\"}}","{}","[]junk"};
 for(unsigned i=0;i<sizeof(bad_lists)/sizeof(bad_lists[0]);i++)assert(printer_files_parse_entries(bad_lists[i],entries,3)==-1);
 assert(printer_files_parse_entries("[]",entries,3)==0);
 char path[161];memset(path,'f',160);path[160]=0;cJSON *array=cJSON_CreateArray();assert(array);cJSON *item=cJSON_CreateObject();cJSON_AddStringToObject(item,"path",path);cJSON_AddItemToArray(array,item);
 item=cJSON_CreateObject();path[159]=0;cJSON_AddStringToObject(item,"path",path);cJSON_AddItemToArray(array,item);
 char *json=cJSON_PrintUnformatted(array);assert(json);assert(printer_files_parse_entries(json,entries,3)==1&&strlen(entries[0].path)==159);free(json);cJSON_Delete(array);
 char deep[64];memset(deep,'[',17);memset(deep+17,']',17);deep[34]=0;assert(printer_files_parse_entries(deep,entries,3)==-1);
 char *huge=malloc(PRINTER_FILES_LIST_CAPACITY+1);assert(huge);memset(huge,' ',PRINTER_FILES_LIST_CAPACITY);huge[0]='[';huge[1]=']';huge[PRINTER_FILES_LIST_CAPACITY]=0;assert(printer_files_parse_entries(huge,entries,3)==-1);free(huge);
 puts("PASS: file list strict JSON, escaped and full-width names, overlong omission, invalid row skipping, bounded numeric metadata and hostile size/depth rejection");
}
'''
with tempfile.TemporaryDirectory(prefix='printer-files-') as directory:
    p = Path(directory)
    (p / 'esp_err.h').write_text('#pragma once\ntypedef int esp_err_t;\n')
    (p / 'esp_http_client.h').write_text('#pragma once\ntypedef struct {int unused;} esp_http_client_event_t;\n')
    (p / 'test.c').write_text(fixture + checks)
    cjson = root / 'tools/theme-maker/linux/native/vendor/cjson'
    subprocess.run(['cc', '-std=c11', '-D_POSIX_C_SOURCE=200809L', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined,float-cast-overflow', '-fno-pie', '-no-pie',
                    '-I', str(p), '-I', str(root / 'main'), '-I', str(cjson),
                    str(p / 'test.c'), str(args.source), str(cjson / 'cJSON.c'), '-lm', '-o', str(p / 'test')], check=True)
    subprocess.run([str(p / 'test')], check=True,
                   env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0', 'UBSAN_OPTIONS': 'halt_on_error=1'})
