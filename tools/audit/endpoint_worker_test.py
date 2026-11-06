#!/usr/bin/env python3
"""Exercise actual endpoint verification with fast completion and exact inputs."""
from pathlib import Path
import argparse
import os
import subprocess
import tempfile
root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--source", type=Path, default=root/"main/moonraker_endpoint_test.c")
args = parser.parse_args()
headers = {
    "freertos/FreeRTOS.h": """#pragma once
#include <stddef.h>
#define pdPASS 1
size_t strlcpy(char*,const char*,size_t);
""",
    "freertos/task.h": """#pragma once
#include "FreeRTOS.h"
typedef void *TaskHandle_t;
void vTaskDelete(void*);
int xTaskCreate(void(*)(void*),const char*,unsigned,void*,unsigned,TaskHandle_t*);
""",
}
fixture = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "endpoint_under_test.c"
static bool immediate,fail_create,probe_ok=true;
static unsigned creates;
static void(*worker)(void*);
static const char *expect_host="printer.local",*expect_key="secret";
size_t strlcpy(char*d,const char*s,size_t n){size_t z=strlen(s);if(n)snprintf(d,n,"%s",s);return z;}
bool moonraker_probe_endpoint_with_api_key(const char*h,int p,const char*k,moonraker_probe_result_t*out){
 assert(!strcmp(h,expect_host)&&p==7125&&!strcmp(k,expect_key));
 /* Worker owns its request until publication. */
 assert(!moonraker_endpoint_test_start("other.local",7125,"other"));
 out->reachable=probe_ok;out->klippy_ready=probe_ok;strcpy(out->identity,"Fixture");return probe_ok;
}
void vTaskDelete(void*p){assert(!p);}
int xTaskCreate(void(*fn)(void*),const char*n,unsigned stack,void*a,unsigned priority,TaskHandle_t*t){
 (void)n;(void)a;(void)priority;assert(stack==4096);creates++;
 if(fail_create)return 0;
 worker=fn;if(immediate)fn(NULL);if(t)*t=(void*)1;return pdPASS;
}
int main(void){
 char host[MOONRAKER_CONFIG_HOST_LENGTH+1],key[MOONRAKER_CONFIG_API_KEY_LENGTH+1];
 memset(host,'h',sizeof(host)-1);host[sizeof(host)-1]=0;
 memset(key,'k',sizeof(key)-1);key[sizeof(key)-1]=0;
 assert(!moonraker_endpoint_test_start(host,7125,"secret")&&!creates);
 assert(!moonraker_endpoint_test_start("printer.local",7125,key)&&!creates);
 assert(!moonraker_endpoint_test_start("printer.local",0,"secret")&&!creates);
 fail_create=true;assert(!moonraker_endpoint_test_start("printer.local",7125,"secret"));
 assert(!moonraker_endpoint_test_busy()&&!s_api_key[0]);fail_create=false;
 assert(moonraker_endpoint_test_start("printer.local",7125,"secret")&&moonraker_endpoint_test_busy());
 assert(!moonraker_endpoint_test_take_result(NULL,NULL));
 worker(NULL);assert(!moonraker_endpoint_test_busy()&&!s_api_key[0]);
 moonraker_probe_result_t result;bool verified=false;
 assert(moonraker_endpoint_test_take_result(&result,&verified)&&verified&&result.klippy_ready&&!strcmp(result.identity,"Fixture"));
 assert(!moonraker_endpoint_test_take_result(NULL,NULL));
 immediate=true;
 for(unsigned i=0;i<3;i++){
  probe_ok=i!=1;
  assert(moonraker_endpoint_test_start("printer.local",7125,"secret")&&!moonraker_endpoint_test_busy());
  assert(moonraker_endpoint_test_take_result(&result,&verified)&&verified==probe_ok);
 }
 /* Largest accepted values arrive at the probe without silent truncation. */
 host[sizeof(host)-2]=0;key[sizeof(key)-2]=0;expect_host=host;expect_key=key;
 assert(moonraker_endpoint_test_start(host,7125,key));
 assert(moonraker_endpoint_test_take_result(NULL,NULL)&&!s_api_key[0]);
 puts("PASS: endpoint bounds, exact requests, creation failure/retry, fast completion, one-shot results and cleared API keys");
}
'''
with tempfile.TemporaryDirectory(prefix="endpoint-worker-") as directory:
    tmp = Path(directory)
    for name, content in headers.items():
        path = tmp/name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content)
    (tmp/"endpoint_under_test.c").write_bytes(args.source.read_bytes())
    (tmp/"test.c").write_text(fixture)
    executable = tmp/"test"
    subprocess.run(["cc", "-std=c11", "-D_POSIX_C_SOURCE=200809L", "-O1", "-g",
                    "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined",
                    "-fno-pie", "-no-pie", "-I", str(tmp), "-I", str(root/"main"),
                    str(tmp/"test.c"), "-o", str(executable)], check=True)
    subprocess.run([str(executable)], check=True,
                   env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0"})
