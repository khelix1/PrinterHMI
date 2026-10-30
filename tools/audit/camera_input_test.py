#!/usr/bin/env python3
"""Exercise the real camera connection-test worker with bounded hostile streams."""
from pathlib import Path
import argparse, os, subprocess, tempfile
root=Path(__file__).resolve().parents[2]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--source',type=Path,default=root/'main/camera_test_controller.c')
args=parser.parse_args()
headers={
'esp_err.h':'#pragma once\ntypedef int esp_err_t;\n#define ESP_OK 0\n#define ESP_FAIL -1\n',
'esp_heap_caps.h':'#pragma once\n#include <stdlib.h>\n#define MALLOC_CAP_SPIRAM 1\n#define MALLOC_CAP_8BIT 2\nvoid *heap_caps_malloc(size_t,unsigned);\nvoid heap_caps_free(void*);\n',
'esp_timer.h':'#pragma once\n#include <stdint.h>\nint64_t esp_timer_get_time(void);\n',
'esp_http_client.h':'''#pragma once
#include "esp_err.h"
#include <stdint.h>
typedef void *esp_http_client_handle_t;
#define HTTP_METHOD_GET 0
typedef struct {const char *url;int method,timeout_ms,buffer_size;} esp_http_client_config_t;
esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t*);
esp_err_t esp_http_client_open(esp_http_client_handle_t,int);
int64_t esp_http_client_fetch_headers(esp_http_client_handle_t);
int esp_http_client_get_status_code(esp_http_client_handle_t);
int esp_http_client_read(esp_http_client_handle_t,char*,int);
esp_err_t esp_http_client_close(esp_http_client_handle_t);
esp_err_t esp_http_client_cleanup(esp_http_client_handle_t);
''',
'freertos/FreeRTOS.h':'''#pragma once
#include <stddef.h>
typedef void *TaskHandle_t;
#define pdPASS 1
#define tskNO_AFFINITY -1
size_t strlcpy(char*,const char*,size_t);
''',
'freertos/task.h':'#pragma once\n#include "FreeRTOS.h"\nvoid vTaskDelete(void*);\n',
'freertos/idf_additions.h':'#pragma once\n#include "FreeRTOS.h"\nint xTaskCreatePinnedToCoreWithCaps(void(*)(void*),const char*,unsigned,void*,unsigned,TaskHandle_t*,int,unsigned);\n',
}
fixture=r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "camera_under_test.c"
static unsigned reads,allocations,frees,closes,cleanups,lanes,ends,creates;
static size_t offset;
static int scenario;
static int64_t clock_us;
static void (*worker)(void*);
static const unsigned char jpeg[]={255,216,255,192,0,8,8,0,16,0,32,0,255,217};
void *heap_caps_malloc(size_t n,unsigned flags){(void)flags;if(scenario==5)return NULL;allocations++;return malloc(n);}
void heap_caps_free(void *p){if(p){frees++;free(p);}}
size_t strlcpy(char *d,const char *s,size_t n){size_t z=strlen(s);if(n)snprintf(d,n,"%s",s);return z;}
int64_t esp_timer_get_time(void){return clock_us;}
bool network_activity_controller_acquire_shared(uint32_t ms){assert(ms==4000);if(scenario==6)return false;lanes++;return true;}
void network_activity_controller_end_shared(void){ends++;}
esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t *c){assert(!strcmp(c->url,"http://camera/stream")&&c->timeout_ms==3500);return scenario==7?NULL:(void*)1;}
esp_err_t esp_http_client_open(esp_http_client_handle_t c,int n){assert(c==(void*)1&&!n);return scenario==8?ESP_FAIL:ESP_OK;}
int64_t esp_http_client_fetch_headers(esp_http_client_handle_t c){assert(c==(void*)1);return scenario==9?-1:0;}
int esp_http_client_get_status_code(esp_http_client_handle_t c){assert(c==(void*)1);return scenario==4?404:200;}
int esp_http_client_read(esp_http_client_handle_t c,char *out,int capacity){
 assert(c==(void*)1&&capacity==1024);reads++;clock_us+=scenario==3?1000000:1000;
 if(scenario==3){memset(out,'x',(size_t)capacity);return reads<10?capacity:0;}
 size_t prefix=scenario==1?7:0;
 size_t size=scenario==1?CAMERA_TEST_FRAME_CAPACITY+100:scenario==2?CAMERA_TEST_FRAME_CAPACITY:sizeof(jpeg);
 size_t total=prefix+size;if(offset>=total)return 0;
 size_t count=total-offset;if(count>(size_t)capacity)count=(size_t)capacity;
 if(scenario==10&&count>1)count=1; /* Split SOI/EOI across every read. */
 for(size_t i=0;i<count;i++){
  size_t pos=offset+i;
  out[i]=pos<prefix?'x':pos-prefix<sizeof(jpeg)-2?(char)jpeg[pos-prefix]:
         pos-prefix==size-2?(char)255:pos-prefix==size-1?(char)217:'x';
 }
 offset+=count;return (int)count;
}
esp_err_t esp_http_client_close(esp_http_client_handle_t c){assert(c==(void*)1);closes++;return ESP_OK;}
esp_err_t esp_http_client_cleanup(esp_http_client_handle_t c){assert(c==(void*)1);cleanups++;return ESP_OK;}
int xTaskCreatePinnedToCoreWithCaps(void(*fn)(void*),const char*n,unsigned stack,void*a,unsigned p,TaskHandle_t*t,int core,unsigned caps){
 (void)n;(void)a;(void)p;(void)core;(void)caps;assert(stack==8192);creates++;worker=fn;*t=(void*)2;return pdPASS;
}
void vTaskDelete(void *p){assert(!p);}
int main(int argc,char **argv){
 scenario=argc>1?atoi(argv[1]):0;
 char long_url[sizeof(s_url)+1];memset(long_url,'a',sizeof(long_url)-1);long_url[sizeof(long_url)-1]=0;
 if(scenario==11){assert(!camera_test_start(long_url)&&!creates);puts("PASS: long URL rejected without truncating endpoint");return 0;}
 assert(camera_test_start("http://camera/stream")&&camera_test_busy());
 assert(!camera_test_start("http://camera/other"));worker(NULL);assert(!camera_test_busy());
 bool ok=true;int w=0,h=0;size_t bytes=0;assert(camera_test_take_result(&ok,&w,&h,&bytes));
 assert(!camera_test_take_result(NULL,NULL,NULL,NULL));
 bool expected=scenario==0||scenario==2||scenario==10;
 assert(ok==expected);if(ok){assert(w==32&&h==16);assert(bytes==(scenario==2?CAMERA_TEST_FRAME_CAPACITY:sizeof(jpeg)));}
 assert(allocations==frees&&lanes==ends);
 if(scenario==3)assert(reads<=5);
 if(scenario==4||scenario==9)assert(!reads);
 if(scenario!=5&&scenario!=7)assert(cleanups==1);
 if(lanes)assert(closes==1);
 printf("PASS: camera input scenario %d, bounded reads %u and complete resource cleanup\n",scenario,reads);
}
'''
with tempfile.TemporaryDirectory(prefix='camera-input-') as folder:
 p=Path(folder)
 for name,body in headers.items():
  f=p/name;f.parent.mkdir(parents=True,exist_ok=True);f.write_text(body)
 (p/'camera_under_test.c').write_bytes(args.source.read_bytes())
 (p/'test.c').write_text(fixture)
 exe=p/'test'
 subprocess.run(['cc','-std=c11','-D_POSIX_C_SOURCE=200809L','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie','-I',str(p),'-I',str(root/'main'),str(p/'test.c'),'-o',str(exe)],check=True)
 for scenario in range(12):
  subprocess.run([str(exe),str(scenario)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
