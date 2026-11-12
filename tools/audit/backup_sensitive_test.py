#!/usr/bin/env python3
"""Fault-inject production backup cleanup with real JSON and crypto/storage spies."""
from pathlib import Path
import argparse, os, subprocess, tempfile
root=Path(__file__).resolve().parents[2]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--source',type=Path,default=root/'main/settings_backup.c')
source=parser.parse_args().source.read_text()
def function(name):
 at=source.index(name+'(');start=source.rfind('\n',0,at)+1
 return source[start:source.index('\n}\n',at)+3]
start=source.index('typedef struct {');end=source.index('} settings_backup_snapshot_t;',start)+len('} settings_backup_snapshot_t;')
types=source[start:end]
production='\n'.join(function(n) for n in ['settings_backup_export_with_progress','settings_backup_export_encrypted_with_progress','settings_backup_encrypted_preflight','settings_backup_restore_encrypted_with_progress','settings_backup_restore_with_progress'])
fixture=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include "moonraker_config_controller.h"
#include "settings_backup.h"
#include "settings_backup_sensitive.h"
typedef int ui_theme_id_t,ui_accent_id_t,ui_density_id_t,ui_notification_level_t;
typedef struct {bool large_text,high_contrast,reduced_transparency,reduced_motion;} ui_accessibility_t;
#define CUSTOM_THEME_ID_MAX 32
#define OPERATOR_EVENT_INFO 1
#define OPERATOR_EVENT_ERROR 2
/* TYPES */
static unsigned clears,encryptions,writes,applies,filesystem_calls;
static bool profile_store_readable=true;
bool moonraker_config_storage_readable(void){return profile_store_readable;}
#define SETTINGS_BACKUP_DIRECTORY "/not-written-by-this-test"
#define SETTINGS_BACKUP_TEMP_PATH "/not-written-by-this-test/temp"
#undef SETTINGS_BACKUP_PATH
#define SETTINGS_BACKUP_PATH "/not-written-by-this-test/backup"
#define SETTINGS_BACKUP_PREVIOUS_PATH "/not-written-by-this-test/previous"
static int blocked_mkdir(const char *p,int mode){(void)p;(void)mode;filesystem_calls++;errno=EACCES;return -1;}
#define mkdir blocked_mkdir
static bool promote_backup_file(const char*a,const char*b,const char*c,char*d,size_t e){(void)a;(void)b;(void)c;(void)d;(void)e;assert(false);return false;}
static bool fail_json,fail_print,fail_encrypt,fail_write,fail_load,fail_apply,fail_rollback;
static void *watched[32];static size_t sizes[32];static size_t watches;
static void watch(void *p,size_t n){assert(watches<32);watched[watches]=p;sizes[watches++]=n;}
static void checked_free(void *p){for(size_t i=0;i<watches;i++)if(watched[i]==p){for(size_t j=0;j<sizes[i];j++)assert(!((unsigned char*)p)[j]);watched[i]=watched[--watches];sizes[i]=sizes[watches];break;}free(p);}
static void tracked_clear(void *p,size_t n){backup_sensitive_clear(p,n);if(n==sizeof(settings_backup_snapshot_t))clears++;for(size_t i=0;p&&i<n;i++)assert(!((unsigned char*)p)[i]);}
#define backup_sensitive_clear tracked_clear
static void set_status(char *out,size_t n,const char *s){if(out&&n)snprintf(out,n,"%s",s);}
static void operator_event_log_add(int level,const char *format,...){(void)level;(void)format;}
static void capture_current(settings_backup_snapshot_t *s){memset(s,0,sizeof(*s));strcpy(s->profiles[0].api_key,"private-key");}
static cJSON *snapshot_to_json(const settings_backup_snapshot_t *s,bool include_keys){assert(!strcmp(s->profiles[0].api_key,"private-key"));(void)include_keys;if(fail_json)return NULL;cJSON *r=cJSON_CreateObject();cJSON *v=cJSON_AddStringToObject(r,"api_key","private-key");assert(v);watch(v->valuestring,strlen(v->valuestring));watch(v->string,strlen(v->string));return r;}
static char *print_json(cJSON *r,int n,int formatted){if(fail_print)return NULL;return cJSON_PrintBuffered(r,n,formatted);}
#define cJSON_PrintBuffered print_json
static bool settings_backup_crypto_encrypt(const char *p,const uint8_t *b,size_t n,uint8_t **out,size_t *size,char *status,size_t cap){assert(p&&strstr((const char*)b,"private-key"));watch((void*)b,n);encryptions++;if(fail_encrypt){set_status(status,cap,"encrypt failure");return false;}*out=malloc(1);**out=0x42;*size=1;return true;}
static bool write_encrypted_backup_file(const uint8_t *b,size_t n,char *status,size_t cap){assert(b&&n==1);writes++;if(fail_write)set_status(status,cap,"write failure");return !fail_write;}
static void settings_backup_crypto_free(void *p,size_t n){tracked_clear(p,n);free(p);}
static bool load_encrypted_backup_snapshot(const char *p,settings_backup_snapshot_t *s,char *out,size_t n){assert(p);capture_current(s);if(fail_load)set_status(out,n,"load failure");return !fail_load;}
static bool load_backup_snapshot(settings_backup_snapshot_t *s,char *out,size_t n){return load_encrypted_backup_snapshot("plain",s,out,n);}
static void format_preflight(const settings_backup_snapshot_t *s,bool encrypted,char *out,size_t n){assert(encrypted&&!strcmp(s->profiles[0].api_key,"private-key"));set_status(out,n,"verified");}
static bool apply_snapshot(const settings_backup_snapshot_t *s){assert(!strcmp(s->profiles[0].api_key,"private-key"));applies++;return applies==1?!fail_apply:!fail_rollback;}
/* PRODUCTION */
int main(void){
 cJSON_Hooks hooks={malloc,checked_free};cJSON_InitHooks(&hooks);
 char status[256];
 profile_store_readable=false;
 assert(!settings_backup_export_with_progress(NULL,NULL,status,sizeof(status)));
 assert(strstr(status,"could not be fully read")&&!clears&&!filesystem_calls&&!watches);
 assert(!settings_backup_export_encrypted_with_progress("passphrase",NULL,NULL,status,sizeof(status)));
 assert(strstr(status,"could not be fully read")&&!clears&&!encryptions&&!writes&&!watches);
 profile_store_readable=true;
 for(unsigned mode=0;mode<5;mode++){
  fail_json=mode==1;fail_print=mode==2;fail_encrypt=mode==3;fail_write=mode==4;
  unsigned old=clears;bool ok=settings_backup_export_encrypted_with_progress("passphrase",NULL,NULL,status,sizeof(status));
  assert(ok==(mode==0)&&clears==old+1&&!watches);
 }
 fail_load=false;unsigned old=clears;assert(settings_backup_encrypted_preflight("passphrase",status,sizeof(status))&&clears==old+1);
 fail_load=true;old=clears;assert(!settings_backup_encrypted_preflight("passphrase",status,sizeof(status))&&clears==old+1);
 for(unsigned plain=0;plain<2;plain++)for(unsigned mode=0;mode<4;mode++){
  fail_load=mode==1;fail_apply=mode>=2;fail_rollback=mode==3;applies=0;old=clears;
  bool ok=plain?settings_backup_restore_with_progress(NULL,NULL,status,sizeof(status)):
   settings_backup_restore_encrypted_with_progress("passphrase",NULL,NULL,status,sizeof(status));
  assert(ok==(mode==0)&&clears==old+2);
  assert(applies==(mode==1?0:mode>=2?2:1));
 }
 char borrowed[]="borrowed-secret";cJSON *r=cJSON_CreateObject();cJSON *ref=cJSON_CreateStringReference(borrowed);assert(cJSON_AddItemToObjectCS(r,"constant-key",ref));
 backup_sensitive_json_delete(r);assert(!strcmp(borrowed,"borrowed-secret"));
 assert(!watches&&encryptions==3&&writes==2);
 cJSON_InitHooks(NULL);
 puts("PASS: production encrypted export/preflight and plain/encrypted restore cleanup, JSON/plaintext zero-before-free, failures/rollback, incomplete-configuration export protection, borrowed JSON isolation");
}
'''.replace('/* TYPES */',types).replace('/* PRODUCTION */',production)
with tempfile.TemporaryDirectory(prefix='backup-sensitive-') as directory:
 p=Path(directory);(p/'test.c').write_text(fixture)
 cjson=root/'tools/theme-maker/linux/native/vendor/cjson'
 subprocess.run(['cc','-std=c11','-D_POSIX_C_SOURCE=200809L','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-no-pie','-I',str(root/'main'),'-I',str(cjson),str(p/'test.c'),str(cjson/'cJSON.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
