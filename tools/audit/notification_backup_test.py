#!/usr/bin/env python3
"""Compile the actual optional-notification backup parser against real libcJSON."""
from pathlib import Path
import subprocess,tempfile,ctypes.util
r=Path(__file__).resolve().parents[2];s=(r/'main/settings_backup.c').read_text()
a=s.index('static bool parse_notification_preference(');parser=s[a:s.index('\n}\n',a)+3]
a=s.index('static bool json_integer(');integer=s[a:s.index('\n}\n',a)+3]
assert 'UI_THEME_CLASSIC,\n            UI_THEME_STUDIO_DARK,' in s
assert 'snapshot->optional_confirmations = ui_toast_confirmations_enabled();' in s
assert 'cJSON_AddBoolToObject(appearance, "optional_confirmations", snapshot->optional_confirmations);' in s
assert 'ui_toast_set_confirmations(snapshot->optional_confirmations)' in s
library=ctypes.util.find_library('cjson')
if not library:raise SystemExit('Real libcJSON is required for this host parser audit.')
preamble='''#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
typedef struct cJSON {struct cJSON *next,*prev,*child;int type;char *valuestring;int valueint;double valuedouble;char *string;} cJSON;
extern int cJSON_IsNumber(const cJSON *);
extern cJSON *cJSON_Parse(const char *);
extern cJSON *cJSON_GetObjectItemCaseSensitive(const cJSON *,const char *);
extern int cJSON_IsBool(const cJSON *);
extern int cJSON_IsTrue(const cJSON *);
extern void cJSON_Delete(cJSON *);
static void set_status(char *out,size_t n,const char *v){snprintf(out,n,"%s",v);}
'''
checks='''
int main(void){for(unsigned i=0;i<6;i++){char json[32];snprintf(json,sizeof(json),"{\\\"theme\\\":%u}",i);cJSON *root=cJSON_Parse(json);assert(root);int theme=-1;assert(json_integer(root,"theme",0,4,&theme)==(i<5));if(i<5)assert(theme==(int)i);cJSON_Delete(root);}const char *values[]={"{}","{\\\"optional_confirmations\\\":false}","{\\\"optional_confirmations\\\":true}","{\\\"optional_confirmations\\\":0}","{\\\"optional_confirmations\\\":\\\"true\\\"}","{\\\"optional_confirmations\\\":null}"};
for(unsigned i=0;i<6;i++){cJSON *v=cJSON_Parse(values[i]);assert(v);bool enabled=true;char status[128]="";bool ok=parse_notification_preference(v,&enabled,status,sizeof(status));assert(ok==(i<3));if(ok)assert(enabled==(i==2));else assert(strstr(status,"invalid"));cJSON_Delete(v);}puts("PASS: actual backup parser, all built-in theme values, quiet older-backup default, boolean round-trip fields and malformed-preference rejection");}
'''
with tempfile.TemporaryDirectory(prefix='notice-backup-') as t:
 p=Path(t);(p/'test.c').write_text(preamble+integer+parser+checks)
 subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror',str(p/'test.c'),'-Wl,-l:'+library,'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
