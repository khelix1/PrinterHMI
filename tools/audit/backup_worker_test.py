#!/usr/bin/env python3
"""Run the actual encrypted-backup worker through completion and retry boundaries."""
from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[2]
fixture=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "settings_backup_encrypted_worker.c"
static bool fail_create,immediate,fail_work;
static void (*task)(void*);
static const char *worker_secret;
static unsigned exports,verifies,deletes;
size_t strlcpy(char*d,const char*s,size_t n){size_t z=strlen(s);if(n)snprintf(d,n,"%s",s);return z;}
static void operation(const char *p){assert(!strcmp(p,"test-passphrase")&&!s_passphrase[0]);worker_secret=p;char status[128];assert(!settings_backup_encrypted_worker_start_export(p,status,sizeof(status)));}
bool settings_backup_export_encrypted_with_progress(const char*p,settings_backup_encrypted_progress_cb_t cb,void*u,char*s,size_t n){operation(p);exports++;cb(SETTINGS_BACKUP_ENCRYPTED_ENCRYPTING,u);cb(SETTINGS_BACKUP_ENCRYPTED_WRITING,u);snprintf(s,n,"export %s",fail_work?"failed":"saved");return !fail_work;}
bool settings_backup_encrypted_preflight(const char*p,char*s,size_t n){operation(p);verifies++;snprintf(s,n,"verify %s",fail_work?"failed":"accepted");return !fail_work;}
int xTaskCreate(void(*fn)(void*),const char*n,unsigned stack,void*a,unsigned priority,void*h){(void)n;(void)a;(void)priority;assert(stack==8192&&!h);if(fail_create)return 0;task=fn;if(immediate)fn(NULL);return pdPASS;}
void vTaskDelete(void*p){assert(!p&&worker_secret);for(size_t i=0;i<WORKER_PASSPHRASE_MAX+1;i++)assert(!worker_secret[i]);deletes++;}
int main(void){
 char status[256];
 assert(!settings_backup_encrypted_worker_start_export("short",status,sizeof(status)));
 fail_create=true;assert(!settings_backup_encrypted_worker_start_export("test-passphrase",status,sizeof(status)));assert(!s_passphrase[0]&&settings_backup_encrypted_worker_state()==SETTINGS_BACKUP_ENCRYPTED_WORKER_FAILED);fail_create=false;
 assert(settings_backup_encrypted_worker_start_export("test-passphrase",status,sizeof(status)));
 assert(!settings_backup_encrypted_worker_start_verify("test-passphrase",status,sizeof(status)));task(NULL);
 assert(settings_backup_encrypted_worker_state()==SETTINGS_BACKUP_ENCRYPTED_WORKER_SUCCEEDED);
 settings_backup_encrypted_worker_status(status,sizeof(status));assert(!strcmp(status,"export saved"));
 immediate=true;
 for(unsigned fail=0;fail<2;fail++)for(unsigned verify=0;verify<2;verify++){
  fail_work=fail;assert(verify?settings_backup_encrypted_worker_start_verify("test-passphrase",status,sizeof(status)):settings_backup_encrypted_worker_start_export("test-passphrase",status,sizeof(status)));
  assert(settings_backup_encrypted_worker_state()==(fail?SETTINGS_BACKUP_ENCRYPTED_WORKER_FAILED:SETTINGS_BACKUP_ENCRYPTED_WORKER_SUCCEEDED));
  assert(!s_passphrase[0]);
 }
 assert(exports==3&&verifies==2&&deletes==5);
 puts("PASS: real backup worker, deferred/immediate completion, creation/work failure retry, duplicate work refusal, terminal status and passphrase clearing");
}
'''
with tempfile.TemporaryDirectory(prefix='backup-worker-') as directory:
 p=Path(directory);(p/'freertos').mkdir()
 (p/'freertos/FreeRTOS.h').write_text('#pragma once\n#include <stddef.h>\ntypedef int portMUX_TYPE;\ntypedef int BaseType_t;\n#define portMUX_INITIALIZER_UNLOCKED 0\n#define portENTER_CRITICAL(p) ((void)(p))\n#define portEXIT_CRITICAL(p) ((void)(p))\n#define pdPASS 1\nsize_t strlcpy(char*,const char*,size_t);\n')
 (p/'freertos/task.h').write_text('#pragma once\nint xTaskCreate(void(*)(void*),const char*,unsigned,void*,unsigned,void*);\nvoid vTaskDelete(void*);\n')
 (p/'test.c').write_text(fixture)
 subprocess.run(['cc','-std=c11','-D_POSIX_C_SOURCE=200809L','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-no-pie','-I',str(p),'-I',str(root/'main'),str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
