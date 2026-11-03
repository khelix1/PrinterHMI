#!/usr/bin/env python3
"""Test actual device/motion/endstop controllers under malformed and stale updates."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
cjson = root / 'tools/theme-maker/linux/native/vendor/cjson'
headers = {
    'esp_heap_caps.h': '#pragma once\n#include <stdlib.h>\n#define MALLOC_CAP_SPIRAM 1\n#define MALLOC_CAP_INTERNAL 2\n#define MALLOC_CAP_8BIT 4\n#define heap_caps_calloc(n,s,c) calloc(n,s)\n',
    'esp_log.h': '#pragma once\n#include <stdio.h>\n#define ESP_LOGI(tag,...) do{(void)(tag);if(0)printf(__VA_ARGS__);}while(0)\n#define ESP_LOGW ESP_LOGI\n#define ESP_LOGE ESP_LOGI\n',
    'esp_timer.h': '#pragma once\n#include <stdint.h>\nint64_t esp_timer_get_time(void);\n',
    'freertos/FreeRTOS.h': '#pragma once\ntypedef int portMUX_TYPE;\n#define portMUX_INITIALIZER_UNLOCKED 0\nvoid test_enter(void);\nvoid test_exit(void);\n#define portENTER_CRITICAL(lock) ((void)(lock),test_enter())\n#define portEXIT_CRITICAL(lock) ((void)(lock),test_exit())\n',
}
fixture = r'''
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "cJSON.h"
#include "device_catalog_controller.h"
#include "motion_diagnostics_controller.h"
#include "endstop_status_controller.h"
static unsigned depth,swap_after_unlock;
static cJSON *replacement;
static int64_t now;
uint32_t moonraker_config_generation(void){return 9;}
int64_t esp_timer_get_time(void){return now;}
void test_enter(void){assert(!depth);depth++;}
void test_exit(void){assert(depth==1);depth=0;if(swap_after_unlock&&!--swap_after_unlock)device_catalog_controller_update_from_objects(replacement);}
static cJSON *parse(const char *text){cJSON *r=cJSON_Parse(text);assert(r);return r;}
static cJSON *response(double id){cJSON *r=cJSON_CreateObject();assert(r);cJSON *n=cJSON_AddNumberToObject(r,"id",0);assert(n);n->valuedouble=id;cJSON *result=cJSON_AddObjectToObject(r,"result");assert(result&&cJSON_AddStringToObject(result,"x","TRIGGERED"));return r;}
int main(void){
 assert(device_catalog_controller_init()&&motion_diagnostics_controller_init());
 replacement=parse("[\"temperature_sensor chamber\"]");device_catalog_controller_update_from_objects(replacement);
 cJSON *status=parse("{\"temperature_sensor chamber\":{\"temperature\":42.5}}");
 assert(device_catalog_controller_merge_status(status));device_descriptor_t device;assert(device_catalog_controller_get(0,&device)&&device.live_value_valid&&strstr(device.live_value,"42.5"));
 device_catalog_controller_update_from_objects(replacement);swap_after_unlock=2;
 assert(!device_catalog_controller_merge_status(status));assert(!swap_after_unlock);
 assert(device_catalog_controller_get(0,&device)&&!device.live_value_valid); /* Rebuilt catalog did not inherit old data. */
 assert(device_catalog_controller_merge_status(status));cJSON_Delete(status);
 status=parse("{\"temperature_sensor chamber\":{\"temperature\":1e999}}");
 assert(!device_catalog_controller_merge_status(status));assert(device_catalog_controller_get(0,&device)&&!strstr(device.live_value,"inf"));cJSON_Delete(status);
 cJSON *objects=parse("[\"tmc2209 stepper_x\"]");motion_diagnostics_controller_update_objects(objects);cJSON_Delete(objects);
 status=parse("{\"tmc2209 stepper_x\":{\"drv_status\":{\"ot\":1e999}}}");
 assert(motion_diagnostics_controller_merge_status(status));cJSON_Delete(status);
 motion_diagnostics_snapshot_t motion;motion_diagnostics_controller_snapshot(&motion);assert(motion.driver_count==1&&motion.drivers[0].status_valid&&!motion.drivers[0].fault);
 endstop_status_snapshot_t stops;uint32_t id=endstop_status_controller_begin(9);
 double invalid[]={id+0.5,NAN,INFINITY,-INFINITY,(double)ENDSTOP_REQUEST_FIRST-1,2147483648.0};
 for(unsigned i=0;i<sizeof(invalid)/sizeof(invalid[0]);i++){
  cJSON *r=response(invalid[i]);assert(!endstop_status_controller_merge(r,9));cJSON_Delete(r);
  endstop_status_controller_snapshot(&stops);assert(stops.waiting&&!stops.valid&&stops.request_id==id);
 }
 cJSON *r=response(id);assert(endstop_status_controller_merge(r,10));endstop_status_controller_snapshot(&stops);assert(stops.waiting&&!stops.valid);
 assert(endstop_status_controller_merge(r,9));endstop_status_controller_snapshot(&stops);assert(!stops.waiting&&stops.valid&&stops.count==1&&stops.items[0].triggered);
 uint32_t next=endstop_status_controller_begin(9);assert(next!=id&&endstop_status_controller_merge(r,9));cJSON_Delete(r);endstop_status_controller_snapshot(&stops);assert(stops.waiting&&stops.request_id==next);
 now=5000000;endstop_status_controller_snapshot(&stops);assert(!stops.waiting&&!stops.valid&&strstr(stops.error,"timed out"));
 cJSON_Delete(replacement);assert(!depth);
 puts("PASS: device catalog replacement race, non-finite readings/driver flags, fractional/non-finite endstop IDs, stale owners/responses and timeout handling");
}
'''
with tempfile.TemporaryDirectory(prefix='device-live-') as directory:
    p = Path(directory)
    for name, content in headers.items():
        path = p / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content)
    (p / 'test.c').write_text(fixture)
    subprocess.run(['cc', '-std=c11', '-D_POSIX_C_SOURCE=200809L', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined,float-cast-overflow', '-fno-pie', '-no-pie',
                    '-I', str(p), '-I', str(root / 'main'), '-I', str(cjson), str(p / 'test.c'),
                    *[str(root / 'main' / name) for name in ['device_catalog_controller.c', 'motion_diagnostics_controller.c', 'endstop_status_controller.c']],
                    str(cjson / 'cJSON.c'), '-lm', '-o', str(p / 'test')], check=True)
    subprocess.run([str(p / 'test')], check=True,
                   env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0'})
