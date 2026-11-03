#!/usr/bin/env python3
"""Exercise the real camera catalog with bounded fields and NVS failures."""
from pathlib import Path
import argparse
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--source', type=Path, default=root / 'main/camera_catalog_controller.c')
args = parser.parse_args()
headers = {
    'nvs.h': '''#pragma once
#include <stddef.h>
#include <stdint.h>
typedef int nvs_handle_t;
#define NVS_READONLY 0
#define NVS_READWRITE 1
int nvs_open(const char *,int,nvs_handle_t *);
int nvs_get_str(nvs_handle_t,const char *,char *,size_t *);
int nvs_get_u8(nvs_handle_t,const char *,uint8_t *);
int nvs_set_str(nvs_handle_t,const char *,const char *);
int nvs_set_u8(nvs_handle_t,const char *,uint8_t);
int nvs_erase_key(nvs_handle_t,const char *);
int nvs_commit(nvs_handle_t);
void nvs_close(nvs_handle_t);
size_t strlcpy(char *,const char *,size_t);
''',
    'esp_err.h': '#pragma once\ntypedef int esp_err_t;\n#define ESP_OK 0\n#define ESP_ERR_NVS_NOT_FOUND -1\n',
}
fixture = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "camera_catalog_controller.h"
#include "moonraker_config_controller.h"
#include "nvs.h"
#include "esp_err.h"
static char primary[192],url[256],name[128];
static int open_error,read_error,write_error,commit_error;
static unsigned opens,closes,writes,commits,primary_writes;
size_t strlcpy(char *out,const char *in,size_t size){size_t n=strlen(in);if(size)snprintf(out,size,"%s",in);return n;}
size_t moonraker_config_profile_capacity(void){return 4;}
const char *moonraker_config_camera_stream_url(int profile){assert(profile>=0&&profile<4);return primary;}
bool moonraker_config_set_camera_stream_url(int profile,const char *text){assert(profile>=0&&profile<4);primary_writes++;strlcpy(primary,text,sizeof(primary));return true;}
int nvs_open(const char *space,int mode,nvs_handle_t *h){assert(!strcmp(space,"netcfg")&&(mode==0||mode==1));opens++;*h=1;return open_error;}
void nvs_close(nvs_handle_t h){assert(h==1);closes++;}
int nvs_get_str(nvs_handle_t h,const char *key,char *out,size_t *size){
 assert(h==1);if(read_error){memset(out,'x',*size);return read_error;}
 const char *text=strstr(key,"_n")?name:url;size_t n=strlen(text)+1;
 if(n>*size)return -8;
 memcpy(out,text,n);*size=n;return ESP_OK;
}
int nvs_get_u8(nvs_handle_t h,const char *key,uint8_t *out){assert(h==1);(void)key;*out=0;return ESP_ERR_NVS_NOT_FOUND;}
int nvs_set_str(nvs_handle_t h,const char *key,const char *text){assert(h==1);writes++;if(write_error)return write_error;strlcpy(strstr(key,"_n")?name:url,text,strstr(key,"_n")?sizeof(name):sizeof(url));return ESP_OK;}
int nvs_set_u8(nvs_handle_t h,const char *key,uint8_t value){assert(h==1);(void)key;(void)value;writes++;return write_error;}
int nvs_erase_key(nvs_handle_t h,const char *key){assert(h==1);writes++;if(write_error)return write_error;(strstr(key,"_n")?name:url)[0]=0;return ESP_OK;}
int nvs_commit(nvs_handle_t h){assert(h==1);commits++;return commit_error;}
static void reset(void){primary[0]=url[0]=name[0]=0;open_error=read_error=write_error=commit_error=0;opens=closes=writes=commits=primary_writes=0;}
static void empty(const camera_catalog_entry_t *e){const unsigned char *p=(const unsigned char*)e;for(size_t i=0;i<sizeof(*e);i++)assert(!p[i]);}
int main(void){
 camera_catalog_entry_t entry;char max_url[192],long_url[193],max_name[32],long_name[33];
 memset(max_url,'u',191);memcpy(max_url,"http://",7);max_url[191]=0;
 memcpy(long_url,max_url,191);long_url[191]='u';long_url[192]=0;
 memset(max_name,'n',31);max_name[31]=0;memset(long_name,'n',32);long_name[32]=0;
 reset();assert(!camera_catalog_set(0,1,max_name,long_url)&&!writes&&!opens);
 assert(!camera_catalog_set(0,0,max_name,long_url)&&!primary_writes&&!opens);
 assert(!camera_catalog_set(0,1,long_name,max_url)&&!writes&&!opens);
 assert(!camera_catalog_set(0,1,"camera","rtsp://unsupported")&&!opens);
 assert(camera_catalog_set(0,1,max_name,max_url)&&camera_catalog_get(0,1,&entry));
 assert(entry.configured&&!strcmp(entry.name,max_name)&&!strcmp(entry.stream_url,max_url));assert(opens==closes);
 assert(camera_catalog_set(0,0,max_name,max_url)&&primary_writes==1&&camera_catalog_get(0,0,&entry));assert(!strcmp(entry.stream_url,max_url));
 reset();memset(&entry,1,sizeof(entry));assert(!camera_catalog_get(-1,1,&entry));empty(&entry);
 memset(&entry,1,sizeof(entry));assert(!camera_catalog_get(0,4,&entry));empty(&entry);
 open_error=-9;assert(!camera_catalog_get(0,1,&entry));empty(&entry);
 open_error=ESP_ERR_NVS_NOT_FOUND;assert(camera_catalog_get(0,1,&entry));empty(&entry);
 open_error=0;read_error=-9;assert(!camera_catalog_get(0,1,&entry));empty(&entry);assert(opens==closes+2);
 read_error=ESP_ERR_NVS_NOT_FOUND;assert(camera_catalog_get(0,1,&entry));empty(&entry);
 read_error=0;strcpy(url,"rtsp://bad");assert(!camera_catalog_get(0,1,&entry));empty(&entry);
 url[0]=0;assert(camera_catalog_get(0,1,&entry));empty(&entry);
 reset();write_error=-9;assert(!camera_catalog_set(0,1,"camera","http://camera")&&!commits&&opens==closes);
 reset();commit_error=-9;assert(!camera_catalog_set(0,1,"camera","http://camera")&&commits==1&&opens==closes);
 reset();assert(camera_catalog_clear(0,1)&&camera_catalog_get(0,1,&entry));empty(&entry);
 puts("PASS: camera catalog exact-limit round trips, oversized rejection before primary/NVS writes, cleared outputs and distinct missing/error reads");
}
'''
with tempfile.TemporaryDirectory(prefix='camera-catalog-') as directory:
    p = Path(directory)
    for name, content in headers.items():
        (p / name).write_text(content)
    (p / 'test.c').write_text(fixture)
    subprocess.run(['cc', '-std=c11', '-D_POSIX_C_SOURCE=200809L', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-fno-pie', '-no-pie',
                    '-I', str(p), '-I', str(root / 'main'), str(p / 'test.c'), str(args.source),
                    '-o', str(p / 'test')], check=True)
    subprocess.run([str(p / 'test')], check=True,
                   env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0'})
