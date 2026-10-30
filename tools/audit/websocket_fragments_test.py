#!/usr/bin/env python3
"""Exercise actual WebSocket assembly before JSON dispatch, including hostile fragment geometry."""
from pathlib import Path
import os,re,subprocess,tempfile
root=Path(__file__).resolve().parents[2]
source=(root/'main/moonraker_live_websocket.c').read_text()
def function(name):
 mask=re.sub(r'/\*[\s\S]*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',lambda m:' '*len(m[0]),source)
 hit=re.search(r'^[\w* ]+\b'+name+r'\s*\([^;{}]*\)\s*\{',mask,re.M);assert hit,name
 end,depth=hit.end(),1
 while depth:
  depth+=(mask[end]=='{')-(mask[end]=='}');end+=1
 return source[hit.start():end]
code=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#define WS_MESSAGE_MAX_BYTES (128 * 1024)
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
#define ESP_LOGW(...) ((void)0)
static char *s_message_buffer;
static size_t s_message_capacity,s_message_received,s_message_expected;
static uint32_t s_message_generation,s_generation=1;
static bool allocation_ok=true;
static void *heap_caps_malloc(size_t n,unsigned flags){(void)flags;return allocation_ok?malloc(n):NULL;}
#define heap_caps_free free
typedef struct {const char *data_ptr;int data_len,payload_len,payload_offset,op_code;} esp_websocket_event_data_t;
'''
code+=function('ensure_message_capacity')+'\n'+function('assemble_websocket_message')
code+=r'''
static int part(const char *data,int n,int length,int offset){
 esp_websocket_event_data_t e={.data_ptr=data,.data_len=n,.payload_len=length,.payload_offset=offset,.op_code=1};
 return assemble_websocket_message(&e);
}
int main(void){
 assert(part("hello",5,5,0)==5&&!strcmp(s_message_buffer,"hello"));
 assert(part("he",2,5,0)==0&&part("llo",3,5,2)==5&&!strcmp(s_message_buffer,"hello"));
 assert(!part("lo",2,5,3)); /* Orphan tail cannot reuse the old prefix. */
 assert(!part("h",1,5,0)&&!part("lo",2,5,3)); /* Gap. */
 assert(!part("he",2,5,0)&&!part("ello",4,5,1)); /* Overlap. */
 assert(!part("he",2,5,0)&&!part("llo!",4,6,2)); /* Length changes. */
 assert(!part("he",2,5,0));s_generation++;assert(!part("llo",3,5,2));
 assert(!part("he",2,5,0));assert(part("new",3,3,0)==3&&!strcmp(s_message_buffer,"new"));
 assert(!part("he",2,5,0));assert(!part("x",1,5,-1));assert(!part("llo",3,5,2));
 assert(!part("x",1,5,INT_MAX));assert(!part("x",INT_MAX,5,1));
 assert(!part("x",1,WS_MESSAGE_MAX_BYTES,0));assert(!part("x",1,INT_MAX,0));
 free(s_message_buffer);s_message_buffer=NULL;s_message_capacity=0;allocation_ok=false;
 assert(!part("he",2,5,0));allocation_ok=true;assert(!part("llo",3,5,2));
 assert(part("ok",2,2,0)==2&&!strcmp(s_message_buffer,"ok"));
 /* Exact maximum legal size includes room for the terminator. */
 size_t n=WS_MESSAGE_MAX_BYTES-1;char *large=malloc(n);assert(large);memset(large,'a',n);
 assert(part(large,(int)n,(int)n,0)==(int)n&&s_message_buffer[n]==0);
 free(large);free(s_message_buffer);
 puts("PASS: real WebSocket assembly rejects orphan/gap/overlap/length/generation/overflow/allocation faults and accepts complete bounded messages");
}
'''
# Connection boundaries must retire a partial assembly, even without a profile change.
for event in ('CONNECTED','DISCONNECTED','CLOSED'):
 assert 's_message_received = s_message_expected = 0;' in source.split('case WEBSOCKET_EVENT_'+event+':',1)[1].split('break;',1)[0]
assert 's_message_received = s_message_expected = 0;' in function('destroy_client')
with tempfile.TemporaryDirectory(prefix='websocket-fragments-') as folder:
 p=Path(folder);(p/'test.c').write_text(code);exe=p/'test'
 subprocess.run(['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie',str(p/'test.c'),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
