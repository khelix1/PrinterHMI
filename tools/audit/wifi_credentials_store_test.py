#!/usr/bin/env python3
"""Exercise the actual Wi-Fi NVS boundary and fixed-width station builder."""
from pathlib import Path
import os
import argparse
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--source', type=Path, default=root / 'main/wifi_credentials_store.c')
args = parser.parse_args()
headers = {
    'nvs.h': '''#pragma once
#include <stddef.h>
typedef int nvs_handle_t;
#define NVS_READONLY 0
#define NVS_READWRITE 1
int nvs_open(const char *,int,nvs_handle_t *);
int nvs_get_str(nvs_handle_t,const char *,char *,size_t *);
int nvs_set_str(nvs_handle_t,const char *,const char *);
int nvs_commit(nvs_handle_t);
void nvs_close(nvs_handle_t);
''',
    'esp_err.h': '#pragma once\n#define ESP_OK 0\ntypedef int esp_err_t;\nconst char *esp_err_to_name(int);\n',
    'esp_log.h': '#pragma once\n#define ESP_LOGW(tag,...) ((void)(tag))\n#define ESP_LOGI(tag,...) ((void)(tag))\n#define ESP_LOGE(tag,...) ((void)(tag))\n',
    'esp_wifi.h': '#pragma once\ntypedef struct {struct {unsigned char ssid[32],password[64];} sta;} wifi_config_t;\n',
}
fixture = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "wifi_credentials_store.h"
#include "wifi_station_config.h"
#include "nvs.h"
static int open_error,ssid_error,pass_error,set_error,commit_error;
static unsigned opens,closes,writes,commits;
static const char *stored_ssid="network", *stored_pass="secret";
const char *esp_err_to_name(int e){(void)e;return "injected";}
int nvs_open(const char *name,int mode,nvs_handle_t *h){assert(!strcmp(name,"netcfg"));assert(mode==0||mode==1);opens++;*h=42;return open_error;}
void nvs_close(nvs_handle_t h){assert(h==42);closes++;}
int nvs_get_str(nvs_handle_t h,const char *key,char *out,size_t *size){
 assert(h==42);bool ssid=!strcmp(key,"ssid");assert(ssid||!strcmp(key,"pass"));
 int error=ssid?ssid_error:pass_error;
 if(error){memset(out,'x',*size);return error;} /* Failure must clear even a touched buffer. */
 const char *text=ssid?stored_ssid:stored_pass;size_t n=strlen(text)+1;
 if(n>*size){return 2;}memcpy(out,text,n);*size=n;return 0;
}
int nvs_set_str(nvs_handle_t h,const char *key,const char *value){assert(h==42&&value);assert(!strcmp(key,"ssid")||!strcmp(key,"pass"));writes++;return set_error;}
int nvs_commit(nvs_handle_t h){assert(h==42);commits++;return commit_error;}
static void reset(void){open_error=ssid_error=pass_error=set_error=commit_error=0;opens=closes=writes=commits=0;stored_ssid="network";stored_pass="secret";}
static void zero(const char *p,size_t n){for(size_t i=0;i<n;i++)assert(!p[i]);}
int main(void){
 char ssid[33],pass[65];wifi_config_t config;
 reset();memset(ssid,'s',sizeof(ssid));memset(pass,'p',sizeof(pass));open_error=1;
 assert(!wifi_credentials_store_load(ssid,sizeof(ssid),pass,sizeof(pass)));zero(ssid,sizeof(ssid));zero(pass,sizeof(pass));assert(!closes);
 reset();ssid_error=1;assert(!wifi_credentials_store_load(ssid,sizeof(ssid),pass,sizeof(pass)));zero(ssid,sizeof(ssid));zero(pass,sizeof(pass));assert(closes==1);
 reset();pass_error=1;assert(!wifi_credentials_store_load(ssid,sizeof(ssid),pass,sizeof(pass)));zero(ssid,sizeof(ssid));zero(pass,sizeof(pass));assert(closes==1);
 reset();assert(!wifi_credentials_store_load(NULL,0,pass,sizeof(pass)));assert(!opens);zero(pass,sizeof(pass));
 reset();assert(wifi_credentials_store_load(ssid,sizeof(ssid),pass,sizeof(pass)));assert(!strcmp(ssid,"network")&&!strcmp(pass,"secret"));
 stored_pass="";assert(wifi_credentials_store_load(ssid,sizeof(ssid),pass,sizeof(pass))&&!pass[0]);
 stored_ssid="";assert(!wifi_credentials_store_load(ssid,sizeof(ssid),pass,sizeof(pass)));zero(ssid,sizeof(ssid));zero(pass,sizeof(pass));
 char max_ssid[33],max_pass[65],long_ssid[34],long_pass[66];
 memset(max_ssid,'s',32);max_ssid[32]=0;memset(max_pass,'a',64);max_pass[64]=0;
 memset(long_ssid,'s',33);long_ssid[33]=0;memset(long_pass,'p',65);long_pass[65]=0;
 reset();stored_ssid=max_ssid;stored_pass=max_pass;
 assert(wifi_credentials_store_load(ssid,sizeof(ssid),pass,sizeof(pass)));
 assert(wifi_station_config_build(&config,ssid,pass));assert(!memcmp(config.sta.ssid,max_ssid,32)&&!memcmp(config.sta.password,max_pass,64));
 assert(wifi_credentials_store_save(max_ssid,max_pass)&&writes==2&&commits==1&&closes==2);
 unsigned before=opens;
 assert(!wifi_credentials_store_save(long_ssid,max_pass));assert(!wifi_credentials_store_save(max_ssid,long_pass));assert(!wifi_credentials_store_save(NULL,NULL));assert(!wifi_credentials_store_save("",NULL));assert(opens==before);
 assert(!wifi_station_config_build(&config,long_ssid,max_pass));zero((char*)&config,sizeof(config));
 assert(!wifi_station_config_build(&config,max_ssid,long_pass));zero((char*)&config,sizeof(config));
 assert(!wifi_station_config_build(&config,NULL,NULL));assert(!wifi_station_config_build(NULL,max_ssid,max_pass));
 assert(wifi_station_config_build(&config,"short",NULL));assert(!strcmp((char*)config.sta.ssid,"short"));zero((char*)config.sta.password,sizeof(config.sta.password));
 reset();open_error=1;assert(!wifi_credentials_store_save("network","secret")&&!writes&&!closes);
 reset();set_error=1;assert(!wifi_credentials_store_save("network","secret")&&writes==1&&!commits&&closes==1);
 reset();commit_error=1;assert(!wifi_credentials_store_save("network","secret")&&writes==2&&commits==1&&closes==1);
 reset();assert(wifi_credentials_store_save("network",NULL));
 puts("PASS: WiFi read failures clear both outputs; explicit open-network passwords, full-width credentials, bounds and storage failures checked");
}
'''
with tempfile.TemporaryDirectory(prefix='wifi-store-') as directory:
    p = Path(directory)
    for name, body in headers.items():
        (p / name).write_text(body)
    (p / 'test.c').write_text(fixture)
    subprocess.run(['cc', '-std=c11', '-D_POSIX_C_SOURCE=200809L', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-fno-pie', '-no-pie',
                    '-I', str(p), '-I', str(root / 'main'), str(p / 'test.c'),
                    str(args.source), '-o', str(p / 'test')], check=True)
    subprocess.run([str(p / 'test')], check=True,
                   env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0'})
