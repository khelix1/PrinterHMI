#!/usr/bin/env python3
"""Compile the actual optional-notification backup parser against real libcJSON."""
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
