#!/usr/bin/env python3
"""Check actual backup parsing, export/restore progress and rollback control flow on the host."""
from pathlib import Path
import subprocess,tempfile,ctypes.util
r=Path(__file__).resolve().parents[2];s=(r/'main/settings_backup.c').read_text()
a=s.index('static bool parse_notification_preference(');parser=s[a:s.index('\n}\n',a)+3]
a=s.index('static bool json_integer(');integer=s[a:s.index('\n}\n',a)+3]
assert 'UI_THEME_CLASSIC,\n            UI_THEME_STUDIO_DARK,' in s
assert 'snapshot->notification_level = ui_toast_level();' in s
assert 'cJSON_AddNumberToObject(appearance, "notification_level", snapshot->notification_level);' in s
assert 'ui_toast_set_level(snapshot->notification_level)' in s
library=ctypes.util.find_library('cjson')
if not library:raise SystemExit('Real libcJSON is required for this host parser audit.')
preamble='''#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
typedef enum { UI_NOTIFY_QUIET=0,UI_NOTIFY_STANDARD=1,UI_NOTIFY_DETAILED=2 } ui_notification_level_t;
typedef struct cJSON {struct cJSON *next,*prev,*child;int type;char *valuestring;int valueint;double valuedouble;char *string;} cJSON;
extern int cJSON_IsNumber(const cJSON *);
extern cJSON *cJSON_Parse(const char *);
extern cJSON *cJSON_GetObjectItemCaseSensitive(const cJSON *,const char *);
extern int cJSON_IsBool(const cJSON *);
extern int cJSON_IsTrue(const cJSON *);
extern void cJSON_Delete(cJSON *);
static void set_status(char *out,size_t n,const char *v){snprintf(out,n,"%s",v);}
'''
checks=r'''
int main(void){
 const char *values[]={"{}","{\"optional_confirmations\":false}","{\"optional_confirmations\":true}","{\"notification_level\":0}","{\"notification_level\":1}","{\"notification_level\":2}","{\"notification_level\":1,\"optional_confirmations\":true}","{\"notification_level\":3}","{\"notification_level\":1.5}","{\"notification_level\":null}","{\"notification_level\":\"1\"}","{\"notification_level\":-1}","{\"optional_confirmations\":0}","{\"optional_confirmations\":null}"};
 int expected[]={0,0,2,0,1,2,1,-1,-1,-1,-1,-1,-1,-1};
 for(unsigned i=0;i<sizeof(expected)/sizeof(expected[0]);i++) {
  cJSON *v=cJSON_Parse(values[i]);assert(v);ui_notification_level_t level=UI_NOTIFY_DETAILED;char status[128]="";
  bool ok=parse_notification_preference(v,&level,status,sizeof(status));assert(ok==(expected[i]>=0));
  if(ok)assert((int)level==expected[i]);else assert(strstr(status,"invalid"));cJSON_Delete(v);
 }
 for(unsigned i=0;i<6;i++){char json[32];snprintf(json,sizeof(json),"{\"theme\":%u}",i);cJSON *v=cJSON_Parse(json);assert(v);int theme=-1;assert(json_integer(v,"theme",0,4,&theme)==(i<5));cJSON_Delete(v);}
 puts("PASS: actual level backup parser, all built-in themes, old boolean migration, level precedence and malformed/fractional/out-of-range rejection");}
'''
with tempfile.TemporaryDirectory(prefix='notice-backup-') as t:
 p=Path(t);(p/'test.c').write_text(preamble+integer+parser+checks)
 subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror',str(p/'test.c'),'-Wl,-l:'+library,'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)


# Backup/restore progress and compatibility entry point checks.
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[2]
source=(root/'main/settings_backup.c').read_text()
def function(name):
    a=source.index('bool '+name+'(')
    return source[a:source.index('\n}',a)+3]
functions='\n'.join(function(name) for name in [
    'settings_backup_export_with_progress','settings_backup_restore_with_progress',
    'settings_backup_restore_encrypted_with_progress'])
wrappers=source[source.index('/* Compatibility entry points'):]
preamble=r'''
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "settings_backup.h"
typedef struct {int value;} settings_backup_snapshot_t;
typedef int cJSON;
static char directory[512],backup[512],temporary[512],previous[512];
#define SETTINGS_BACKUP_DIRECTORY directory
#undef SETTINGS_BACKUP_PATH
#define SETTINGS_BACKUP_PATH backup
#define SETTINGS_BACKUP_TEMP_PATH temporary
#define SETTINGS_BACKUP_PREVIOUS_PATH previous
#define OPERATOR_EVENT_ERROR 1
#define OPERATOR_EVENT_INFO 2
static bool load_ok=true,apply_ok=true;
static unsigned applies,captures,loads,phase_count;
static settings_backup_stage_t phases[8];
static void set_status(char *out,size_t n,const char *message){snprintf(out,n,"%s",message);}
static void operator_event_log_add(int level,const char *message){(void)level;(void)message;}
static void capture_current(settings_backup_snapshot_t *s){captures++;s->value=7;}
static bool load_backup_snapshot(settings_backup_snapshot_t *s,char *out,size_t n){loads++;s->value=42;if(!load_ok)set_status(out,n,"Invalid backup");return load_ok;}
static bool load_encrypted_backup_snapshot(const char *p,settings_backup_snapshot_t *s,char *out,size_t n){
 loads++;s->value=42;if(strcmp(p,"correct-passphrase")){set_status(out,n,"Passphrase is incorrect or the encrypted backup was changed.");return false;}return load_ok;}
static bool apply_snapshot(const settings_backup_snapshot_t *s){applies++;assert(s->value==(applies%2==0&&!apply_ok?7:42));return apply_ok;}
static cJSON *snapshot_to_json(const settings_backup_snapshot_t *s,bool keys){assert(s->value==7&&!keys);static cJSON json;return &json;}
static char *cJSON_PrintBuffered(cJSON *s,int n,bool pretty){(void)s;(void)n;(void)pretty;return strdup("{\"backup\":true}");}
static void cJSON_Delete(cJSON *s){(void)s;}
static void cJSON_free(void *s){free(s);}
static void progress(settings_backup_stage_t phase,void *context){assert(context==phases);assert(phase_count<8);phases[phase_count++]=phase;}
static void reset(void){applies=captures=loads=phase_count=0;load_ok=apply_ok=true;}
'''
checks=r'''
int main(int argc,char **argv){
 assert(argc==2);snprintf(directory,sizeof(directory),"%s/data",argv[1]);
 snprintf(backup,sizeof(backup),"%s/data/backup.json",argv[1]);snprintf(temporary,sizeof(temporary),"%s/data/backup.tmp",argv[1]);snprintf(previous,sizeof(previous),"%s/data/previous.json",argv[1]);
 char status[256];reset();
 assert(settings_backup_export_with_progress(progress,phases,status,sizeof(status)));
 assert(phase_count==2&&phases[0]==SETTINGS_BACKUP_STAGE_PREPARING&&phases[1]==SETTINGS_BACKUP_STAGE_WRITING);
 FILE *f=fopen(backup,"rb");assert(f);char data[64]={0};assert(fread(data,1,sizeof(data)-1,f)>0);fclose(f);assert(strstr(data,"backup"));
 assert(settings_backup_export(status,sizeof(status)));assert(access(temporary,F_OK)!=0&&access(previous,F_OK)!=0);
 reset();load_ok=false;assert(!settings_backup_restore_with_progress(progress,phases,status,sizeof(status)));
 assert(phase_count==1&&phases[0]==SETTINGS_BACKUP_STAGE_READING&&!applies&&!captures);
 reset();assert(!settings_backup_restore_encrypted_with_progress("wrong-passphrase",progress,phases,status,sizeof(status)));
 assert(strstr(status,"Passphrase is incorrect")&&phase_count==1&&phases[0]==SETTINGS_BACKUP_STAGE_AUTHENTICATING&&!applies&&!captures);
 reset();assert(settings_backup_restore_with_progress(progress,phases,status,sizeof(status)));
 assert(applies==1&&captures==1&&loads==1&&phase_count==2&&phases[1]==SETTINGS_BACKUP_STAGE_APPLYING&&strstr(status,"Reboot"));
 reset();apply_ok=false;assert(!settings_backup_restore_with_progress(progress,phases,status,sizeof(status)));
 assert(applies==2&&phase_count==3&&phases[2]==SETTINGS_BACKUP_STAGE_ROLLING_BACK&&strstr(status,"previous settings"));
 reset();assert(settings_backup_restore_encrypted_with_progress("correct-passphrase",progress,phases,status,sizeof(status)));
 assert(applies==1&&captures==1&&phase_count==2&&phases[0]==SETTINGS_BACKUP_STAGE_AUTHENTICATING&&phases[1]==SETTINGS_BACKUP_STAGE_APPLYING&&strstr(status,"Reboot"));
 reset();apply_ok=false;assert(!settings_backup_restore_encrypted_with_progress("correct-passphrase",progress,phases,status,sizeof(status)));
 assert(applies==2&&phase_count==3&&phases[2]==SETTINGS_BACKUP_STAGE_ROLLING_BACK);
 reset();assert(settings_backup_restore(status,sizeof(status))&&applies==1&&!phase_count);
 reset();assert(settings_backup_restore_encrypted("correct-passphrase",status,sizeof(status))&&applies==1&&!phase_count);
 puts("PASS: actual plain export and plain/encrypted restore stage order, validation/authentication rejection before settings changes, rollback, compatibility callers and real temporary-file promotion");
}
'''
with tempfile.TemporaryDirectory(prefix='backup-progress-') as tmp:
    p=Path(tmp);(p/'test.c').write_text(preamble+functions+wrappers+checks)
    subprocess.run(['cc','-std=c11','-D_POSIX_C_SOURCE=200809L','-Wall','-Wextra','-Werror','-I',str(root/'main'),str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test'),str(p)],check=True)
