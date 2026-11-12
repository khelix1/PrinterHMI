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
#ifdef LEGACY_DISCOVERY_SOURCE
camera_discovery_status_t camera_discovery_status(void){return CAMERA_DISCOVERY_READY;}
#endif
static uint32_t generation=1;
static bool immediate,fail_create;
static unsigned writes,probes;
static int fail_save=-1;
static size_t discovered=1;
static moonraker_profile_t profiles[4];
static void(*worker)(void*);
uint32_t moonraker_config_generation(void){return generation;}
int moonraker_config_active_profile_index(void){return 0;}
const moonraker_profile_t *moonraker_config_profile(int i){return i>=0&&i<4?&profiles[i]:NULL;}
size_t strlcpy(char*d,const char*s,size_t n){size_t z=strlen(s);if(n)snprintf(d,n,"%s",s);return z;}
void vTaskDelete(void*p){assert(!p);}
size_t moonraker_probe_webcams_with_api_key(const char*h,int port,const char*key,moonraker_webcam_t*out,size_t cap){
 assert(!strcmp(h,"printer.local")&&port==7125&&!strcmp(key,"secret")&&cap==4);probes++;
 for(size_t i=0;i<discovered;i++){snprintf(out[i].name,sizeof(out[i].name),"Camera %u",(unsigned)i);strcpy(out[i].stream_url,"http://camera/stream");}return discovered;
}
bool camera_catalog_set(int profile,size_t index,const char*name,const char*url){
 assert(profile==1&&index<discovered&&name[0]&&!strcmp(url,"http://camera/stream"));
 assert(!camera_discovery_start_for_profile(1,"printer.local",7125,"secret"));
 writes++;return (int)index!=fail_save;
}
int xTaskCreatePinnedToCoreWithCaps(void(*fn)(void*),const char*n,unsigned stack,void*a,unsigned p,TaskHandle_t*t,int core,unsigned caps){
 (void)n;(void)stack;(void)a;(void)p;(void)core;(void)caps;
 if(fail_create)return 0;
 worker=fn;if(immediate)fn(NULL);if(t)*t=(void*)2;return pdPASS;
}
static bool start(void){return camera_discovery_start_for_profile(1,"printer.local",7125,"secret");}
int main(void){
 for(unsigned i=0;i<4;i++){profiles[i]=(moonraker_profile_t){.configured=true,.port=7125};strcpy(profiles[i].host,"printer.local");strcpy(profiles[i].api_key,"secret");}
 fail_create=true;assert(!start()&&!camera_discovery_busy());
 assert(!s_api_key[0]);fail_create=false;
 assert(start()&&camera_discovery_busy());assert(!start());
 worker(NULL);assert(!camera_discovery_busy()&&!s_api_key[0]);
 moonraker_webcam_t webcam;bool found=false;size_t count=0;
 assert(camera_discovery_take_result(&webcam,&found,&count)&&found&&count==1&&writes==1);
 assert(camera_discovery_status()==CAMERA_DISCOVERY_READY);
#ifndef LEGACY_DISCOVERY_SOURCE
 for(size_t i=0;i<sizeof(s_profile_owner);i++)assert(!((unsigned char*)&s_profile_owner)[i]);
#endif
 assert(!camera_discovery_take_result(NULL,NULL,NULL));
 immediate=true;
 for(unsigned i=0;i<3;i++){assert(start()&&!camera_discovery_busy());assert(camera_discovery_take_result(NULL,NULL,NULL));}
 /* This is an INACTIVE target: configuration generation intentionally does not change. */
 for(unsigned field=0;field<7;field++){
  moonraker_profile_t before=profiles[1];assert(start());
  if(field==0)profiles[1].port++;
  if(field==1)strcpy(profiles[1].host,"changed.local");
  if(field==2)strcpy(profiles[1].api_key,"changed-secret");
  if(field==3)profiles[1].configured=false;
  if(field==4)profiles[1].secure_transport=true;
  if(field==5)strcpy(profiles[1].camera_stream_url,"http://changed/stream");
  if(field==6)strcpy(profiles[1].name,"Renamed");
  unsigned old=writes;assert(!camera_discovery_take_result(NULL,NULL,NULL)&&writes==old);
  assert(camera_discovery_status()==CAMERA_DISCOVERY_STALE);profiles[1]=before;
 }
 immediate=false;assert(start());unsigned old_probes=probes;generation++;worker(NULL);
 assert(probes==old_probes&&!camera_discovery_take_result(NULL,NULL,NULL));
 assert(camera_discovery_status()==CAMERA_DISCOVERY_STALE);
 immediate=true;discovered=2;
 for(int failure=0;failure<2;failure++){
  fail_save=failure;assert(start());unsigned old=writes;
  assert(camera_discovery_take_result(&webcam,&found,&count)&&found&&count==2&&writes==old+2);
  assert(camera_discovery_status()==CAMERA_DISCOVERY_SAVE_FAILED);
 }
 fail_save=-1;assert(start());assert(camera_discovery_take_result(NULL,NULL,NULL));
 assert(camera_discovery_status()==CAMERA_DISCOVERY_READY);
 discovered=0;assert(start());assert(camera_discovery_take_result(&webcam,&found,&count)&&!found&&!count&&!webcam.name[0]);
 assert(camera_discovery_status()==CAMERA_DISCOVERY_READY);
 puts("PASS: inactive-profile edits cannot import stale cameras; queued-generation rejection, partial save failures/retry, empty results, secret cleanup, fast completion and result ownership");
}
'''
with tempfile.TemporaryDirectory(prefix="camera-discovery-") as directory:
    tmp = Path(directory)
    headers["moonraker_config_controller.h"]=(root/"main/moonraker_config_controller.h").read_text()
    headers["camera_discovery_controller.h"]=(root/"main/camera_discovery_controller.h").read_text().replace('#include "moonraker_probe.h"','typedef struct { char name[32],stream_url[192]; } moonraker_webcam_t;')
    for name, body in headers.items():
        path = tmp/name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(body.replace("\\n", "\n"))
    (tmp/"discovery_under_test.c").write_bytes(args.source.read_bytes())
    (tmp/"test.c").write_text(fixture)
    exe = tmp/"test"
    subprocess.run(["cc", "-std=c11", "-D_POSIX_C_SOURCE=200809L", *(["-DLEGACY_DISCOVERY_SOURCE"] if "static moonraker_profile_t s_profile_owner" not in args.source.read_text() else []), "-O1", "-g", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=address,undefined", "-fno-pie", "-no-pie",
                    "-I", str(tmp), "-I", str(root/"main"), str(tmp/"test.c"), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True, env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0"})
