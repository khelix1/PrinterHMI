#!/usr/bin/env python3
"""Fault-inject actual encrypted-worker and release-catalog control paths.

FreeRTOS task/mutex behavior is mocked. Catalog parsing uses real libcJSON.
"""
from pathlib import Path
import ctypes.util
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def function(source, name):
    pos = source.index(name + '(')
    return source[source.rfind('\n', 0, pos) + 1:source.index('\n}\n', pos) + 3]


compat = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static inline size_t strlcpy(char *d,const char *s,size_t n){size_t len=strlen(s);if(n){size_t c=len<n-1?len:n-1;memcpy(d,s,c);d[c]=0;}return len;}
'''
worker = r'''
#include "settings_backup_encrypted_worker.c"
static void (*task)(void *);
static bool task_ok=true,operation_ok=true;
static int starts,deletes;
static char used[100];
BaseType_t xTaskCreate(void(*f)(void*),const char*n,unsigned stack,void*a,unsigned priority,void*out){(void)n;(void)a;(void)out;assert(stack==8192&&priority==4);starts++;task=f;return task_ok?pdPASS:0;}
void vTaskDelete(void *p){assert(!p);deletes++;}
bool settings_backup_encrypted_preflight(const char *p,char *s,size_t n){strlcpy(used,p,sizeof(used));strlcpy(s,"Verified",n);return operation_ok;}
bool settings_backup_export_encrypted_with_progress(const char*p,settings_backup_encrypted_progress_cb_t cb,void*u,char*s,size_t n){strlcpy(used,p,sizeof(used));cb(SETTINGS_BACKUP_ENCRYPTED_ENCRYPTING,u);cb(SETTINGS_BACKUP_ENCRYPTED_WRITING,u);strlcpy(s,"Saved",n);return operation_ok;}
int main(void){
 char status[256],pass[100];memset(pass,'p',sizeof(pass));pass[99]=0;
 assert(!settings_backup_encrypted_worker_start_export(pass,status,sizeof(status))&&!starts);
 pass[97]=0;assert(!settings_backup_encrypted_worker_start_verify(pass,status,sizeof(status))&&!starts);
 pass[96]=0;assert(settings_backup_encrypted_worker_start_export(pass,status,sizeof(status))&&starts==1);
 assert(!settings_backup_encrypted_worker_start_export("another-long-passphrase",status,sizeof(status))&&starts==1);
 task(NULL);assert(!strcmp(used,pass)&&settings_backup_encrypted_worker_state()==SETTINGS_BACKUP_ENCRYPTED_WORKER_SUCCEEDED&&deletes==1);
 for(size_t i=0;i<sizeof(s_passphrase);i++)assert(!s_passphrase[i]);
 assert(settings_backup_encrypted_worker_start_verify(pass,status,sizeof(status)));operation_ok=false;task(NULL);
 assert(settings_backup_encrypted_worker_state()==SETTINGS_BACKUP_ENCRYPTED_WORKER_FAILED&&!strcmp(used,pass));
 task_ok=false;assert(!settings_backup_encrypted_worker_start_export(pass,status,sizeof(status)));
 for(size_t i=0;i<sizeof(s_passphrase);i++)assert(!s_passphrase[i]);
 assert(!settings_backup_encrypted_worker_start_export("short",status,sizeof(status)));
 puts("PASS: actual encrypted-worker byte limits, exact 96-byte passphrase preservation, busy rejection, failed operations/tasks, secret retirement");
}
'''
catalog = r'''
#include <ctype.h>
#include <sys/stat.h>
#include "json_document_limits.h"
#include "ota_release_catalog.h"
#include "firmware_identity.h"
typedef struct cJSON {struct cJSON *next,*prev,*child;int type;char *valuestring;int valueint;double valuedouble;char *string;} cJSON;
extern cJSON *cJSON_Parse(const char *);
extern cJSON *cJSON_ParseWithLength(const char *,size_t);
extern cJSON *cJSON_ParseWithLengthOpts(const char *,size_t,const char **,int);
extern cJSON *cJSON_GetObjectItemCaseSensitive(const cJSON *,const char *);
extern cJSON *cJSON_GetArrayItem(const cJSON *,int);
extern int cJSON_GetArraySize(const cJSON *);
extern int cJSON_IsString(const cJSON *),cJSON_IsArray(const cJSON *),cJSON_IsObject(const cJSON *),cJSON_IsTrue(const cJSON *),cJSON_IsNumber(const cJSON *);
extern void cJSON_Delete(cJSON *);
typedef struct {ota_release_entry_t entries[OTA_RELEASE_CATALOG_MAX];size_t count;} release_catalog_t;
typedef void *SemaphoreHandle_t;
static SemaphoreHandle_t s_lock;
static release_catalog_t *s_catalog;
static bool s_task_running;
static char *s_error;
static ota_release_catalog_state_t s_state=OTA_RELEASE_CATALOG_IDLE;
static const char TAG[]="fixture";
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
#define CUSTOM_THEME_MAX_BYTES (48 * 1024)
typedef int custom_theme_entry_t;
#define heap_caps_free free
#define pdTRUE 1
#define pdPASS 1
#define portMAX_DELAY 999
typedef int BaseType_t;
#define ESP_LOGE(t,...) ((void)(t))
static bool mutex_ok,take_ok=true,task_ok=true;
static int takes,gives,starts;
static void *xSemaphoreCreateMutexWithCaps(int caps){assert(caps==3);return mutex_ok?(void*)1:NULL;}
static int xSemaphoreTake(void *p,unsigned ticks){assert(p&&ticks==portMAX_DELAY);takes++;return take_ok;}
static void xSemaphoreGive(void *p){assert(p);gives++;}
static void *heap_caps_malloc(size_t n,int caps){assert(caps==3);return malloc(n);}
static void release_catalog_task(void *p){(void)p;}
static int xTaskCreateWithCaps(void(*f)(void*),const char*n,unsigned stack,void*a,unsigned priority,void*out,int caps){(void)n;(void)a;(void)out;assert(f==release_catalog_task&&stack==8192&&priority==5&&caps==3);starts++;return task_ok;}
void firmware_identity_get(firmware_identity_t *out){memset(out,0,sizeof(*out));}
int firmware_identity_compare(const firmware_identity_t *i,firmware_channel_t c,const char *v){(void)i;(void)c;(void)v;return 1;}
#include "catalog.c"
int main(int argc,char **argv){
 assert(argc==2);const char *theme_path=argv[1];custom_theme_entry_t theme_entry=0;
 const char *theme_docs[]={"{}\n","{} trailing","{} {}","[[[[[[[[[[[[[[[[[]]]]]]]]]]]]]]]]]","{\"literal\":\"[[[\\\"{\\\\}\"}"};
 for(unsigned i=0;i<5;i++){FILE *f=fopen(theme_path,"wb");assert(f);size_t n=strlen(theme_docs[i]);assert(fwrite(theme_docs[i],1,n,f)==n&&!fclose(f));cJSON *j=read_theme_document(theme_path,&theme_entry);assert((j!=NULL)==(i==0||i==4));cJSON_Delete(j);}
 ota_release_catalog_snapshot_t snapshot;ota_release_entry_t entry;
 assert(!ota_release_catalog_start()&&!takes&&!starts);
 ota_release_catalog_snapshot(&snapshot);assert(snapshot.state==OTA_RELEASE_CATALOG_ERROR&&strstr(snapshot.error,"memory")&&!takes);
 assert(!ota_release_catalog_entry(0,&entry)&&!takes);catalog_fail("error");assert(!takes);
 mutex_ok=true;take_ok=false;assert(!ota_release_catalog_start()&&!starts);take_ok=true;
 assert(ota_release_catalog_start()&&starts==1);assert(ota_release_catalog_start()&&starts==1);
 catalog_fail("deliberate failure");ota_release_catalog_snapshot(&snapshot);assert(snapshot.state==OTA_RELEASE_CATALOG_ERROR&&strstr(snapshot.error,"deliberate"));
 task_ok=false;assert(!ota_release_catalog_start());assert(!s_task_running&&s_state==OTA_RELEASE_CATALOG_ERROR);
 char json[1400],url[240];memset(url,'x',sizeof(url));url[239]=0;
 snprintf(json,sizeof(json),"[{\"tag_name\":\"v7.0.0\",\"assets\":[{\"name\":\"PrinterHMI-v7.0.0-ota.bin\",\"browser_download_url\":\"%s\"}]}]",url);
 release_catalog_t parsed={0};assert(parse_releases(json,&parsed)&&!parsed.count);
 url[191]=0;snprintf(json,sizeof(json),"[{\"tag_name\":\"v7.0.0\",\"assets\":[{\"name\":\"PrinterHMI-v7.0.0-ota.bin\",\"browser_download_url\":\"%s\"}]}]",url);
 assert(parse_releases(json,&parsed)&&parsed.count==1&&!strcmp(parsed.entries[0].asset_url,url));
 s_catalog=&parsed;assert(ota_release_catalog_entry(0,&entry)&&!strcmp(entry.asset_url,url));assert(!ota_release_catalog_entry(1,&entry));
 assert(gives>0);free(s_error);
 puts("PASS: actual SD-theme read/parse stage, catalog mutex OOM/recovery, failed takes/tasks, snapshots, entries, busy requests, and asset URL boundaries");
}
'''

with tempfile.TemporaryDirectory(prefix='printerhmi-deep-') as tmp:
    folder = Path(tmp)
    (folder / 'freertos').mkdir()
    (folder / 'freertos/FreeRTOS.h').write_text('''#pragma once
typedef int BaseType_t,portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(p) ((void)(p))
#define portEXIT_CRITICAL(p) ((void)(p))
#define pdPASS 1
''')
    (folder / 'freertos/task.h').write_text('''#pragma once
#include "FreeRTOS.h"
BaseType_t xTaskCreate(void(*)(void*),const char*,unsigned,void*,unsigned,void*);
void vTaskDelete(void *);
''')
    source = (ROOT / 'main/ota_release_catalog.c').read_text()
    functions = ['ensure_lock', 'lock_catalog', 'unlock_catalog', 'psram_calloc',
                 'catalog_fail', 'parse_semver', 'copy_json_string', 'normalize_notes',
                 'valid_nightly_tag', 'parse_releases', 'ota_release_catalog_start',
                 'ota_release_catalog_snapshot', 'ota_release_catalog_entry']
    theme = function((ROOT / 'main/custom_theme.c').read_text(), 'parse_theme_file')
    # Exercise the original reader/parser stage before schema extraction.
    theme_reader = theme[:theme.index('    memset(entry,')].replace(
        'static bool parse_theme_file(', 'static cJSON *read_theme_document(')
    theme_reader += '    return root;\n}\n'
    (folder / 'catalog.c').write_text(theme_reader + '\n' + '\n'.join(function(source, name) for name in functions))
    library = ctypes.util.find_library('cjson')
    if not library:
        raise SystemExit('Real libcJSON is required.')
    for name, fixture in [('worker', worker), ('catalog', catalog)]:
        (folder / (name + '_fixture.c')).write_text(compat + fixture)
        cmd = ['cc', '-D_POSIX_C_SOURCE=200809L', '-std=c11', '-O1', '-Wall', '-Wextra',
               '-Werror', '-fsanitize=undefined', '-I', str(folder), '-I', str(ROOT / 'main'),
               str(folder / (name + '_fixture.c')), '-o', str(folder / name)]
        if name == 'catalog':
            cmd += ['-Wl,-l:' + library]
        subprocess.run(cmd, check=True)
        subprocess.run([str(folder / name), str(folder / 'theme.json')], check=True)
