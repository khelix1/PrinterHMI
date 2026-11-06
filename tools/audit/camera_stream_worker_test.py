#!/usr/bin/env python3
"""Run the real MJPEG worker against creation, HTTP, queue and stop failures."""
from pathlib import Path
import argparse
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--source", type=Path, default=root/"main/camera_stream_controller.c")
args = parser.parse_args()
headers = {
    "esp_err.h": "#pragma once\ntypedef int esp_err_t;\n#define ESP_OK 0\n#define ESP_FAIL -1\n",
    "esp_heap_caps.h": """#pragma once
#include <stdlib.h>
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
void *heap_caps_malloc(size_t,unsigned);
void heap_caps_free(void*);
""",
    "esp_http_client.h": """#pragma once
#include "esp_err.h"
#include <stdint.h>
typedef void *esp_http_client_handle_t;
#define HTTP_METHOD_GET 0
typedef struct {const char *url,*cert_pem;int method,timeout_ms,buffer_size;} esp_http_client_config_t;
esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t*);
esp_err_t esp_http_client_open(esp_http_client_handle_t,int);
int64_t esp_http_client_fetch_headers(esp_http_client_handle_t);
int esp_http_client_get_status_code(esp_http_client_handle_t);
int esp_http_client_read(esp_http_client_handle_t,char*,int);
esp_err_t esp_http_client_close(esp_http_client_handle_t);
esp_err_t esp_http_client_cleanup(esp_http_client_handle_t);
""",
    "freertos/FreeRTOS.h": """#pragma once
#include <stddef.h>
typedef void *TaskHandle_t;
#define pdPASS 1
#define tskNO_AFFINITY -1
#define pdMS_TO_TICKS(ms) (ms)
size_t strlcpy(char*,const char*,size_t);
""",
    "freertos/task.h": '#pragma once\n#include "FreeRTOS.h"\nvoid vTaskDelete(void*);\nvoid vTaskDelay(unsigned);\n',
    "freertos/idf_additions.h": '#pragma once\n#include "FreeRTOS.h"\nint xTaskCreatePinnedToCoreWithCaps(void(*)(void*),const char*,unsigned,void*,unsigned,TaskHandle_t*,int,unsigned);\n',
    "freertos/queue.h": """#pragma once
typedef void *QueueHandle_t;
QueueHandle_t xQueueCreate(unsigned,unsigned);
int xQueueReceive(QueueHandle_t,void*,unsigned);
int xQueueSend(QueueHandle_t,const void*,unsigned);
""",
}
fixture = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stream_under_test.c"

enum { NORMAL, QUEUE_FAIL, TASK_FAIL, ALLOC_FAIL, INIT_FAIL, OPEN_FAIL,
       HEADER_FAIL, HTTP_ERROR, STOP_ACQUIRE, LONG_URL, LANE_BUSY, QUEUE_FULL,
       DECODE_FAIL, STOP_BEFORE_RUN };
static int scenario;
static unsigned allocations,frees,creates,lanes,ends,clients,cleanups,closes,reads,decoded,frames;
static bool queued;
static camera_frame_result_t pending;
static void (*worker)(void*);

size_t strlcpy(char*d,const char*s,size_t n){size_t z=strlen(s);if(n)snprintf(d,n,"%s",s);return z;}
void *heap_caps_malloc(size_t n,unsigned caps){(void)caps;if(scenario==ALLOC_FAIL)return NULL;allocations++;return malloc(n);}
void heap_caps_free(void*p){if(p){frees++;free(p);}}
QueueHandle_t xQueueCreate(unsigned n,unsigned size){assert(n==1&&size==sizeof(pending));return scenario==QUEUE_FAIL?NULL:(void*)1;}
int xQueueReceive(QueueHandle_t q,void*out,unsigned ticks){assert(q==(void*)1&&!ticks);if(!queued)return 0;memcpy(out,&pending,sizeof(pending));queued=false;return pdPASS;}
int xQueueSend(QueueHandle_t q,const void*in,unsigned ticks){
 assert(q==(void*)1&&!ticks);
 const camera_frame_result_t *frame=in;
 if(scenario==QUEUE_FULL&&frame->ok)return 0;
 if(queued)return 0;
 pending=*frame;queued=true;
 if(frame->ok){
  uint8_t *pixels=NULL;size_t bytes=0;int w=0,h=0;bool ok=false;
  assert(camera_stream_take_result(&pixels,&bytes,&w,&h,&ok));
  assert(ok&&pixels&&bytes==12&&w==3&&h==2);frames++;heap_caps_free(pixels);
 }
 return pdPASS;
}
bool network_activity_controller_acquire_shared(uint32_t ms){
 assert(ms==3000);if(scenario==LANE_BUSY)return false;
 lanes++;if(scenario==STOP_ACQUIRE)camera_stream_request_stop();return true;
}
void network_activity_controller_end_shared(void){ends++;}
esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t*c){
 assert(!strcmp(c->url,"http://camera/stream"));if(scenario==INIT_FAIL)return NULL;clients++;return (void*)2;
}
esp_err_t esp_http_client_open(esp_http_client_handle_t c,int n){assert(c==(void*)2&&!n);return scenario==OPEN_FAIL?ESP_FAIL:ESP_OK;}
int64_t esp_http_client_fetch_headers(esp_http_client_handle_t c){assert(c==(void*)2);return scenario==HEADER_FAIL?-1:0;}
int esp_http_client_get_status_code(esp_http_client_handle_t c){assert(c==(void*)2);return scenario==HTTP_ERROR?401:200;}
int esp_http_client_read(esp_http_client_handle_t c,char*out,int capacity){
 assert(c==(void*)2&&capacity==2048);reads++;
 if(reads==1){unsigned char jpeg[]={255,216,0,255,217};memcpy(out,jpeg,sizeof(jpeg));return sizeof(jpeg);}
 camera_stream_request_stop();return 0;
}
esp_err_t esp_http_client_close(esp_http_client_handle_t c){assert(c==(void*)2);closes++;return ESP_OK;}
esp_err_t esp_http_client_cleanup(esp_http_client_handle_t c){assert(c==(void*)2);cleanups++;return ESP_OK;}
bool camera_jpeg_decode_rgb565(const uint8_t*jpeg,size_t n,uint16_t**pixels,int*w,int*h){
 assert(n==5&&jpeg[0]==255&&jpeg[4]==217);decoded++;
 if(scenario==DECODE_FAIL)return false;
 *pixels=heap_caps_malloc(12,0);*w=3;*h=2;return true;
}
void vTaskDelay(unsigned ms){assert(ms<=50);camera_stream_request_stop();}
void vTaskDelete(void*p){assert(!p);}
int xTaskCreatePinnedToCoreWithCaps(void(*fn)(void*),const char*n,unsigned stack,void*a,unsigned p,TaskHandle_t*t,int core,unsigned caps){
 (void)n;(void)a;(void)p;(void)core;(void)caps;assert(stack==12288);creates++;
 if(scenario==TASK_FAIL)return 0;
 worker=fn;if(scenario!=STOP_BEFORE_RUN)fn(NULL);
 if(t)*t=(void*)3; /* Creator may publish a handle after its task has finished. */
 return pdPASS;
}
int main(int argc,char**argv){
 scenario=argc>1?atoi(argv[1]):0;
 if(scenario==LONG_URL){
  char url[193];memset(url,'x',192);url[192]=0;
  assert(!camera_stream_start(url)&&!creates&&!camera_stream_busy());return 0;
 }
 if(scenario==QUEUE_FAIL||scenario==TASK_FAIL){
  assert(!camera_stream_start("http://camera/stream")&&!camera_stream_busy());
  assert(!camera_stream_start("http://camera/stream")&&!camera_stream_busy());return 0;
 }
 assert(camera_stream_start("http://camera/stream"));
 if(scenario==STOP_BEFORE_RUN){
  assert(camera_stream_busy()&&!camera_stream_start("http://camera/stream"));
  camera_stream_request_stop();worker(NULL);assert(!lanes);
 }
 assert(!camera_stream_busy());
 if(scenario==ALLOC_FAIL){
  bool ok=true;assert(camera_stream_take_result(NULL,NULL,NULL,NULL,&ok)&&!ok);
  assert(!camera_stream_take_result(NULL,NULL,NULL,NULL,NULL));
 }
 assert(allocations==frees&&lanes==ends&&clients==cleanups);
 if(scenario==HTTP_ERROR||scenario==HEADER_FAIL||scenario==STOP_ACQUIRE)assert(!reads&&!decoded);
 if(scenario==NORMAL)assert(frames==1&&decoded==1);
 if(scenario==QUEUE_FULL)assert(!frames&&decoded==1);
 if(scenario==DECODE_FAIL)assert(!frames&&decoded==1);
 unsigned before=creates;scenario=ALLOC_FAIL;
 assert(camera_stream_start("http://camera/stream")&&!camera_stream_busy()&&creates==before+1);
 camera_stream_stop();
 assert(allocations==frees&&lanes==ends&&clients==cleanups);
 puts("PASS: MJPEG worker lifecycle, HTTP validation and balanced frame/transport ownership");
}
'''
with tempfile.TemporaryDirectory(prefix="camera-stream-") as directory:
    tmp = Path(directory)
    for name, content in headers.items():
        path = tmp/name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content)
    (tmp/"stream_under_test.c").write_bytes(args.source.read_bytes())
    (tmp/"test.c").write_text(fixture)
    executable = tmp/"test"
    subprocess.run(["cc", "-std=c11", "-D_POSIX_C_SOURCE=200809L", "-O1", "-g",
                    "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined",
                    "-fno-pie", "-no-pie", "-I", str(tmp), "-I", str(root/"main"),
                    str(tmp/"test.c"), "-o", str(executable)], check=True)
    for scenario in range(14):
        subprocess.run([str(executable), str(scenario)], check=True,
                       env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0"})
