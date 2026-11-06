#!/usr/bin/env python3
"""Exercise actual camera discovery publication, fast completion and retry."""
from pathlib import Path
import argparse
import os
import subprocess
import tempfile
root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--source", type=Path, default=root/"main/camera_discovery_controller.c")
args = parser.parse_args()
headers = {
    "camera_discovery_controller.h": """
#pragma once
#include <stdbool.h>
#include <stddef.h>
typedef struct { char name[32],stream_url[192]; } moonraker_webcam_t;
bool camera_discovery_start_for_profile(int,const char*,int,const char*);
""",
    "moonraker_config_controller.h": """
#pragma once
#include <stdint.h>
#define MOONRAKER_CONFIG_HOST_LENGTH 128
#define MOONRAKER_CONFIG_API_KEY_LENGTH 64
#define MOONRAKER_CONFIG_MAX_PROFILES 4
uint32_t moonraker_config_generation(void);
int moonraker_config_active_profile_index(void);
""",
    "camera_catalog_controller.h": """
#pragma once
#include "camera_discovery_controller.h"
#define CAMERA_CATALOG_MAX_CAMERAS 4
size_t moonraker_probe_webcams_with_api_key(const char*,int,const char*,moonraker_webcam_t*,size_t);
bool camera_catalog_set(int,size_t,const char*,const char*);
""",
    "freertos/FreeRTOS.h": """
#pragma once
#include <stddef.h>
typedef void *TaskHandle_t;
#define pdPASS 1
#define tskNO_AFFINITY -1
size_t strlcpy(char*,const char*,size_t);
""",
    "freertos/task.h": '#include "FreeRTOS.h"\nvoid vTaskDelete(void*);\n',
    "freertos/idf_additions.h": '#include "FreeRTOS.h"\nint xTaskCreatePinnedToCoreWithCaps(void(*)(void*),const char*,unsigned,void*,unsigned,TaskHandle_t*,int,unsigned);\n',
    "esp_heap_caps.h": "#define MALLOC_CAP_SPIRAM 1\n#define MALLOC_CAP_8BIT 2\n",
}
fixture = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "discovery_under_test.c"
static uint32_t generation=1;
static bool immediate,fail_create;
static unsigned writes;
static void(*worker)(void*);
uint32_t moonraker_config_generation(void){return generation;}
int moonraker_config_active_profile_index(void){return 1;}
size_t strlcpy(char*d,const char*s,size_t n){size_t z=strlen(s);if(n)snprintf(d,n,"%s",s);return z;}
void vTaskDelete(void*p){assert(!p);}
size_t moonraker_probe_webcams_with_api_key(const char*h,int port,const char*key,moonraker_webcam_t*out,size_t cap){
 assert(!strcmp(h,"printer.local")&&port==7125&&!strcmp(key,"secret")&&cap==4);
 strcpy(out[0].name,"Front");strcpy(out[0].stream_url,"http://camera/stream");return 1;
}
bool camera_catalog_set(int profile,size_t index,const char*name,const char*url){
 assert(profile==1&&!index&&!strcmp(name,"Front")&&!strcmp(url,"http://camera/stream"));
 /* A consumer must keep the result stable while committing it to the catalog. */
 assert(!camera_discovery_start("printer.local",7125,"secret"));
 writes++;return true;
}
int xTaskCreatePinnedToCoreWithCaps(void(*fn)(void*),const char*n,unsigned stack,void*a,unsigned p,TaskHandle_t*t,int core,unsigned caps){
 (void)n;(void)stack;(void)a;(void)p;(void)core;(void)caps;
 if(fail_create)return 0;
 worker=fn;if(immediate)fn(NULL);if(t)*t=(void*)2;return pdPASS;
}
int main(void){
 fail_create=true;assert(!camera_discovery_start("printer.local",7125,"secret")&&!camera_discovery_busy());
 assert(!s_api_key[0]);fail_create=false;
 assert(camera_discovery_start("printer.local",7125,"secret")&&camera_discovery_busy());
 assert(!camera_discovery_start("printer.local",7125,"secret"));
 worker(NULL);assert(!camera_discovery_busy()&&!s_api_key[0]);
 moonraker_webcam_t webcam;bool found=false;size_t count=0;
 assert(camera_discovery_take_result(&webcam,&found,&count)&&found&&count==1&&writes==1);
 assert(!camera_discovery_take_result(NULL,NULL,NULL));
 immediate=true;
 for(unsigned i=0;i<3;i++){
  assert(camera_discovery_start("printer.local",7125,"secret")&&!camera_discovery_busy());
  assert(camera_discovery_take_result(NULL,NULL,NULL));
 }
 immediate=false;
 assert(camera_discovery_start("printer.local",7125,"secret"));worker(NULL);
 generation++;unsigned old=writes;
 assert(!camera_discovery_take_result(NULL,NULL,NULL)&&writes==old);
 immediate=true;assert(camera_discovery_start("printer.local",7125,"secret"));
 assert(camera_discovery_take_result(NULL,NULL,NULL));
 puts("PASS: discovery fast completion, retry, result ownership, stale generation and restart");
}
'''
with tempfile.TemporaryDirectory(prefix="camera-discovery-") as directory:
    tmp = Path(directory)
    for name, body in headers.items():
        path = tmp/name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(body.replace("\\n", "\n"))
    (tmp/"discovery_under_test.c").write_bytes(args.source.read_bytes())
    (tmp/"test.c").write_text(fixture)
    exe = tmp/"test"
    subprocess.run(["cc", "-std=c11", "-O1", "-g", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=address,undefined", "-fno-pie", "-no-pie",
                    "-I", str(tmp), "-I", str(root/"main"), str(tmp/"test.c"), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True, env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0"})
