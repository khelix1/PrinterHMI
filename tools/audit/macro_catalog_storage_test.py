#!/usr/bin/env python3
"""Test actual macro favorites storage, catalog bounds and revision wrap."""
from pathlib import Path
import os
import argparse
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--source', type=Path, default=root / 'main/macro_controller.c')
parser.add_argument('--read-only', action='store_true')
args = parser.parse_args()
cjson = root / 'tools/theme-maker/linux/native/vendor/cjson'
headers = {
    'esp_heap_caps.h': '#pragma once\n#include <stdlib.h>\n#define MALLOC_CAP_SPIRAM 1\n#define MALLOC_CAP_INTERNAL 2\n#define MALLOC_CAP_8BIT 4\n#define heap_caps_calloc(n,s,c) calloc(n,s)\n',
    'esp_log.h': '#pragma once\n#define ESP_LOGI(tag,...) ((void)(tag))\n#define ESP_LOGW(tag,...) ((void)(tag))\n#define ESP_LOGE(tag,...) ((void)(tag))\n',
    'freertos/FreeRTOS.h': '#pragma once\ntypedef int portMUX_TYPE;\n#define portMUX_INITIALIZER_UNLOCKED 0\n#define portENTER_CRITICAL(lock) ((void)(lock))\n#define portEXIT_CRITICAL(lock) ((void)(lock))\n',
    'nvs.h': '''#pragma once
#include <stddef.h>
typedef int esp_err_t;
typedef int nvs_handle_t;
#define ESP_OK 0
#define ESP_ERR_NVS_NOT_FOUND -1
#define NVS_READONLY 0
#define NVS_READWRITE 1
int nvs_open(const char *,int,nvs_handle_t *);
int nvs_get_blob(nvs_handle_t,const char *,void *,size_t *);
int nvs_set_blob(nvs_handle_t,const char *,const void *,size_t);
int nvs_commit(nvs_handle_t);
void nvs_close(nvs_handle_t);
''',
}
fixture = r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "macro_controller.c"
static int open_error,read_error,set_error,commit_error;
static unsigned writes,commits,opens,closes;
static size_t blob_size=sizeof(macro_favorites_t);
static macro_favorites_t stored;
int nvs_open(const char *name,int mode,nvs_handle_t *h){assert(!strcmp(name,"netcfg")&&(mode==0||mode==1));opens++;*h=1;return open_error;}
int nvs_get_blob(nvs_handle_t h,const char *key,void *out,size_t *size){assert(h==1&&!strcmp(key,"mac_favs")&&*size==sizeof(stored));memcpy(out,&stored,sizeof(stored));*size=blob_size;return read_error;}
int nvs_set_blob(nvs_handle_t h,const char *key,const void *data,size_t size){assert(h==1&&!strcmp(key,"mac_favs")&&size==sizeof(stored)&&data);writes++;return set_error;}
int nvs_commit(nvs_handle_t h){assert(h==1);commits++;return commit_error;}
void nvs_close(nvs_handle_t h){assert(h==1);closes++;}
static void reset(void){s_favorites_loaded=false;memset(&s_favorites,0,sizeof(s_favorites));memset(&stored,0,sizeof(stored));blob_size=sizeof(stored);open_error=read_error=set_error=commit_error=0;writes=commits=opens=closes=0;}
int main(void){
 assert(macro_controller_init());reset();
 memset(&stored,'x',sizeof(stored));char query[65];memset(query,'x',64);query[64]=0;
 assert(!macro_controller_is_favorite(query));assert(macro_controller_set_favorite("CLEAN",true));assert(macro_controller_is_favorite("CLEAN"));
 reset();strcpy(stored.names[0],"DUPLICATE");strcpy(stored.names[1],"DUPLICATE");
 assert(macro_controller_set_favorite("DUPLICATE",false)&&!macro_controller_is_favorite("DUPLICATE"));
 reset();blob_size--;assert(!macro_controller_set_favorite("CLEAN",true)&&!writes);blob_size++;
 assert(macro_controller_set_favorite("CLEAN",true)&&writes==1&&commits==1);
 reset();open_error=-9;assert(!macro_controller_set_favorite("CLEAN",true)&&!writes&&!closes);open_error=0;
 assert(macro_controller_set_favorite("CLEAN",true));
 reset();read_error=-9;assert(!macro_controller_set_favorite("CLEAN",true)&&!writes);read_error=0;
 set_error=-9;assert(!macro_controller_set_favorite("CLEAN",true)&&writes==1&&!commits&&!macro_controller_is_favorite("CLEAN"));
 set_error=0;commit_error=-9;assert(!macro_controller_set_favorite("CLEAN",true)&&commits==1&&!macro_controller_is_favorite("CLEAN"));
 commit_error=0;assert(macro_controller_set_favorite("CLEAN",true)&&macro_controller_is_favorite("CLEAN"));
 unsigned before=writes;assert(macro_controller_set_favorite("CLEAN",true)&&writes==before);
 set_error=-9;assert(!macro_controller_set_favorite("CLEAN",false)&&macro_controller_is_favorite("CLEAN"));
 set_error=0;assert(macro_controller_set_favorite("CLEAN",false)&&!macro_controller_is_favorite("CLEAN"));
 reset();for(unsigned i=0;i<8;i++){char name[16];snprintf(name,sizeof(name),"MACRO_%u",i);assert(macro_controller_set_favorite(name,true));}
 before=writes;assert(!macro_controller_set_favorite("NINTH",true)&&writes==before);
 assert(macro_controller_set_favorite("MACRO_3",false)&&macro_controller_set_favorite("NINTH",true));
 before=writes;assert(!macro_controller_set_favorite(query,true)&&!macro_controller_set_favorite("",true)&&writes==before);
 reset();read_error=ESP_ERR_NVS_NOT_FOUND;assert(macro_controller_set_favorite("CLEAN",true));assert(opens==closes);
 char long_object[100];snprintf(long_object,sizeof(long_object),"gcode_macro %s",query);
 cJSON *objects=cJSON_CreateArray();assert(objects);cJSON_AddItemToArray(objects,cJSON_CreateString(long_object));
 cJSON_AddItemToArray(objects,cJSON_CreateString("gcode_macro CLEAN"));
 macro_controller_update_from_objects(objects);cJSON_Delete(objects);
 macro_controller_status_t status;macro_controller_status(&status);assert(status.count==1&&status.total_count==2&&status.truncated);
 char name[64];assert(macro_controller_get(0,name,sizeof(name))&&!strcmp(name,"CLEAN"));
 s_store->status.generation=UINT32_MAX;macro_controller_update_parameters(NULL);macro_controller_status(&status);assert(status.generation==1);
 free(s_store);s_store=NULL;
 puts("PASS: macro favorites blob validation, load/save retry, RAM rollback, capacity/no-op cases, overlong catalog omission and nonzero generation wrap");
}
'''
if args.read_only:
    fixture = fixture[:fixture.index('int main(void){')] + r'''int main(void){
        reset();memset(&stored,'x',sizeof(stored));char query[65];memset(query,'x',64);query[64]=0;
        assert(!macro_controller_is_favorite(query));puts("PASS: unterminated favorites rejected safely");
    }'''
with tempfile.TemporaryDirectory(prefix='macro-storage-') as directory:
    p = Path(directory)
    for name, content in headers.items():
        path = p / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content)
    (p / 'macro_controller.c').write_bytes(args.source.read_bytes())
    (p / 'test.c').write_text(fixture)
    subprocess.run(['cc', '-std=c11', '-D_POSIX_C_SOURCE=200809L', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-fno-pie', '-no-pie',
                    '-I', str(p), '-I', str(root / 'main'), '-I', str(cjson),
                    str(p / 'test.c'), str(root / 'main/macro_parameter_utils.c'), str(cjson / 'cJSON.c'),
                    '-o', str(p / 'test')], check=True)
    subprocess.run([str(p / 'test')], check=True,
                   env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0'})
