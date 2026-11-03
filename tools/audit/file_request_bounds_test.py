#!/usr/bin/env python3
"""Exercise actual metadata HTTP and print-start construction with exact-limit names."""
from pathlib import Path
import argparse
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--source', type=Path, default=root / 'main/moonraker.c')
args = parser.parse_args()
source = args.source.read_text()
def function(name):
    at = source.index(name + '(')
    start = source.rfind('\n', 0, at) + 1
    return source[start:source.index('\n}\n', at) + 3]
production = '\n'.join(function(n) for n in ['moonraker_http_get_raw', 'moonraker_http_get_text', 'moonraker_fetch_file_metadata', 'moonraker_start_print_file'])
fixture = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "cJSON.h"
#include "printer_files.h"
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_ARG -2
#define ESP_ERR_INVALID_SIZE -3
#define ESP_ERR_INVALID_STATE -4
#define HTTP_METHOD_GET 0
#define HTTP_METHOD_POST 1
typedef struct {char *buf;int len,cap;} http_capture_t;
typedef void *esp_http_client_handle_t;
typedef struct {const char *url,*cert_pem;int method,timeout_ms;void *event_handler,*user_data;bool disable_auto_redirect;} esp_http_client_config_t;
static void *moonraker_http_event_handler;
static char url[2048],wire[2048];static unsigned created,cleaned,begun,ended,performed;
static bool shared=true;static int status=200,perform_error;static void *capture;
static bool moonraker_transport_security_build_http_url_for_endpoint(const char *host,int port,const char *path,char *out,size_t size,const char **ca){*ca=NULL;int n=snprintf(out,size,"https://%s:%d%s",host,port,path);return n>0&&(size_t)n<size;}
static void *esp_http_client_init(const esp_http_client_config_t *c){assert(c->disable_auto_redirect);snprintf(url,sizeof(url),"%s",c->url);capture=c->user_data;created++;return (void*)1;}
static int esp_http_client_set_header(void *c,const char *key,const char *value){assert(c&&key&&value);return 0;}
static int esp_http_client_set_post_field(void *c,const char *data,int size){assert(c&&size>0&&(size_t)size<sizeof(wire));memcpy(wire,data,(size_t)size);wire[size]=0;return 0;}
static bool network_activity_controller_try_begin_shared(void){if(!shared)return false;begun++;return true;}
static void network_activity_controller_end_shared(void){ended++;}
static int esp_http_client_perform(void *c){assert(c);performed++;if(capture){http_capture_t *cap=capture;assert(cap->cap>=3);strcpy(cap->buf,"{}");cap->len=2;}return perform_error;}
static int esp_http_client_get_status_code(void *c){assert(c);return status;}
static int esp_http_client_cleanup(void *c){assert(c);cleaned++;return 0;}
'''
checks = r'''
int main(void){
 char host[64],encoded[478],body[64];memset(host,'h',63);host[63]=0;
 for(int i=0;i<159;i++){memcpy(encoded+3*i,"%FF",3);}encoded[477]=0;
 int code=0;esp_err_t error;
 assert(moonraker_fetch_file_metadata(host,65535,"key",encoded,body,sizeof(body),&code,&error));
 assert(strlen(url)>512&&strstr(url,encoded)&&code==200&&error==0&&!strcmp(body,"{}"));
 char name[160];memset(name,1,159);name[159]=0;
 assert(moonraker_start_print_file(host,65535,"key",name,&code,&error));
 cJSON *j=cJSON_Parse(wire);assert(j);const cJSON *file=cJSON_GetObjectItemCaseSensitive(j,"filename");assert(cJSON_IsString(file)&&strlen(file->valuestring)==159&&!memcmp(file->valuestring,name,159));cJSON_Delete(j);
 unsigned before=created;char oversized[161];memset(oversized,'x',160);oversized[160]=0;
 assert(!moonraker_start_print_file(host,7125,"key",oversized,&code,&error)&&created==before);
 assert(!moonraker_start_print_file(host,65536,"key","test",&code,&error)&&created==before);
 char long_encoded[601];memset(long_encoded,'x',600);long_encoded[600]=0;
 assert(!moonraker_fetch_file_metadata(host,7125,"key",long_encoded,body,sizeof(body),&code,&error)&&created==before&&error==ESP_ERR_INVALID_SIZE);
 shared=false;before=performed;assert(!moonraker_start_print_file(host,7125,"key","test",&code,&error)&&performed==before);
 assert(!moonraker_fetch_file_metadata(host,7125,"key",encoded,body,sizeof(body),&code,&error)&&performed==before);
 shared=true;status=401;assert(!moonraker_start_print_file(host,7125,"key","test",&code,&error));
 status=200;perform_error=-9;assert(!moonraker_fetch_file_metadata(host,7125,"key",encoded,body,sizeof(body),&code,&error));
 assert(created==cleaned&&begun==ended);
 puts("PASS: maximum encoded metadata names and JSON-escaped print filenames survive complete request construction; oversized, busy and failed paths clean up");
}
'''
with tempfile.TemporaryDirectory(prefix='file-requests-') as directory:
    p = Path(directory)
    (p / 'test.c').write_text(fixture + production + checks)
    cjson = root / 'tools/theme-maker/linux/native/vendor/cjson'
    subprocess.run(['cc', '-std=c11', '-D_POSIX_C_SOURCE=200809L', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-fno-pie', '-no-pie', '-I', str(root / 'main'), '-I', str(cjson),
                    str(p / 'test.c'), str(cjson / 'cJSON.c'), '-o', str(p / 'test')], check=True)
    subprocess.run([str(p / 'test')], check=True, env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0'})
