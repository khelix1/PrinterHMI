#!/usr/bin/env python3
"""Fault-inject production OTA catalog HTTP capture and worker cleanup."""
from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[2]
source=(root/'main/ota_release_catalog.c').read_text()
def function(name):
 at=source.index(name+'(');return source[source.rfind('\n',0,at)+1:source.index('\n}\n',at)+3]
production=function('release_http_event')+'\n'+function('release_catalog_task')
fixture=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ota_release_catalog.h"
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_STATE -2
#define HTTP_EVENT_ON_DATA 1
#define ESP_LOGI(t,...) do{(void)(t);if(0)printf(__VA_ARGS__);}while(0)
#define RELEASE_API_URL "https://api.github.com/repos/khelix1/PrinterHMI/releases?per_page=30"
#define RELEASE_RESPONSE_MAX (256U*1024U)
typedef int esp_err_t;
typedef void *esp_http_client_handle_t;
typedef struct {void *user_data;int event_id;const char *data;int data_len;} esp_http_client_event_t;
typedef struct {const char *url;int timeout_ms;bool keep_alive_enable;esp_err_t (*event_handler)(esp_http_client_event_t*);void *user_data;int (*crt_bundle_attach)(void*);} esp_http_client_config_t;
typedef struct {ota_release_entry_t entries[OTA_RELEASE_CATALOG_MAX];size_t count;} release_catalog_t;
typedef struct {char *data;size_t length,capacity;bool overflow;} release_capture_t;
static const char TAG[]="fixture";
static release_catalog_t *s_catalog;
static char error_storage[128];static char *s_error=error_storage;
static ota_release_catalog_state_t s_state;
static bool s_task_running;
static unsigned live,performed,cleaned,begun,ended,parsed,header_calls,fail_header;
static bool fail_init,exclusive,busy;
static int http_status=200;
static const char *reply="[]";static size_t reply_size;
static esp_http_client_config_t request;
static void *psram_calloc(size_t n,size_t z){void *p=calloc(n,z);if(p)live++;return p;}
static void tracked_free(void *p){if(p){assert(live);live--;}free(p);}
#define free tracked_free
static void catalog_fail(const char *s){s_state=OTA_RELEASE_CATALOG_ERROR;s_task_running=false;snprintf(s_error,sizeof(error_storage),"%s",s);}
static bool lock_catalog(void){return true;}
static void unlock_catalog(void){}
static bool parse_releases(const char *s,release_catalog_t *r){parsed++;assert(!strcmp(s,"[]"));r->count=0;return true;}
static const char *esp_err_to_name(int error){(void)error;return "fixture";}
static int esp_crt_bundle_attach(void *p){(void)p;return 0;}
static void *esp_http_client_init(const esp_http_client_config_t *c){assert(!strcmp(c->url,RELEASE_API_URL)&&c->crt_bundle_attach==esp_crt_bundle_attach);request=*c;header_calls=0;return fail_init?NULL:(void*)1;}
static int esp_http_client_set_header(void *c,const char *k,const char *v){assert(c&&k&&v);return ++header_calls==fail_header?ESP_FAIL:ESP_OK;}
static bool network_activity_controller_exclusive_requested(void){return exclusive;}
static bool network_activity_controller_acquire_shared(unsigned ms){assert(ms==5000);if(busy)return false;begun++;return true;}
static void network_activity_controller_end_shared(void){ended++;}
static int esp_http_client_perform(void *c){assert(c);performed++;size_t n=reply_size?reply_size:strlen(reply);for(size_t at=0;at<n;){size_t part=n-at;if(part>17)part=17;esp_http_client_event_t e={request.user_data,HTTP_EVENT_ON_DATA,reply+at,(int)part};assert(request.event_handler(&e)==ESP_OK);at+=part;}return ESP_OK;}
static int esp_http_client_get_status_code(void *c){assert(c);return http_status;}
static int esp_http_client_cleanup(void *c){assert(c);cleaned++;return 0;}
static void vTaskDeleteWithCaps(void *p){assert(!p);}
/* PRODUCTION */
int main(void){
 s_task_running=true;release_catalog_task(NULL);assert(s_state==OTA_RELEASE_CATALOG_READY&&!s_task_running&&live==1);
 release_catalog_t *kept=s_catalog;
 for(fail_header=1;fail_header<=3;fail_header++){
  unsigned sent=performed,owned=begun,freed=cleaned;s_task_running=true;release_catalog_task(NULL);
  assert(s_state==OTA_RELEASE_CATALOG_ERROR&&!s_task_running&&live==1&&s_catalog==kept);
  assert(performed==sent&&begun==owned&&cleaned==freed+1);
 }
 fail_header=0;unsigned parse_count=parsed;
 reply="[]\0junk";reply_size=7;s_task_running=true;release_catalog_task(NULL);
 assert(s_state==OTA_RELEASE_CATALOG_ERROR&&!s_task_running&&live==1&&parsed==parse_count);
 reply_size=0;reply="[]";fail_init=true;release_catalog_task(NULL);assert(live==1&&s_catalog==kept);fail_init=false;
 exclusive=true;unsigned sent=performed;release_catalog_task(NULL);assert(live==1&&performed==sent);exclusive=false;
 busy=true;release_catalog_task(NULL);assert(live==1&&performed==sent);busy=false;
 http_status=401;release_catalog_task(NULL);assert(live==1&&s_state==OTA_RELEASE_CATALOG_ERROR);http_status=200;
 s_task_running=true;release_catalog_task(NULL);assert(s_state==OTA_RELEASE_CATALOG_READY&&!s_task_running&&live==1&&begun==ended);
 free(s_catalog);assert(!live);
 puts("PASS: production OTA catalog header/NUL failure refusal, client/lane/buffer cleanup, retained previous catalog and successful retry");
}
'''.replace('/* PRODUCTION */',production)
with tempfile.TemporaryDirectory(prefix='ota-catalog-worker-') as directory:
 p=Path(directory);(p/'test.c').write_text(fixture)
 subprocess.run(['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-no-pie','-I',str(root/'main'),str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
