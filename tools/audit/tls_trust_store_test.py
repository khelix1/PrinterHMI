#!/usr/bin/env python3
"""Fault-inject the actual CA store and HTTPS/WSS policy, including read recovery."""
from pathlib import Path
import argparse
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--source', type=Path, default=root / 'main/moonraker_tls_trust_store.c')
args = parser.parse_args()
headers = {
    'esp_heap_caps.h': '''#pragma once
#include <stdlib.h>
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_INTERNAL 2
#define MALLOC_CAP_8BIT 4
void *test_calloc(size_t,size_t,int);
#define heap_caps_calloc test_calloc
''',
    'esp_log.h': '#pragma once\n#define ESP_LOGI(...) ((void)0)\n#define ESP_LOGW(...) ((void)0)\n',
    'nvs.h': '''#pragma once
#include <stddef.h>
typedef int nvs_handle_t;
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_ERR_NVS_NOT_FOUND -1
#define NVS_READONLY 0
#define NVS_READWRITE 1
int nvs_open(const char *,int,nvs_handle_t *);
int nvs_get_str(nvs_handle_t,const char *,char *,size_t *);
int nvs_set_str(nvs_handle_t,const char *,const char *);
int nvs_commit(nvs_handle_t);
void nvs_close(nvs_handle_t);
''',
    'compat.h': '''#include <string.h>
static inline size_t strlcpy(char *out,const char *value,size_t size){size_t n=strlen(value);if(size){size_t c=n<size-1?n:size-1;memcpy(out,value,c);out[c]=0;}return n;}
''',
}
fixture = r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "moonraker_tls_trust_store.c"
static moonraker_profile_t profiles[4];
const moonraker_profile_t *moonraker_config_profile(int i){return i>=0&&i<4?&profiles[i]:NULL;}
int moonraker_config_active_profile_index(void){return 0;}
static int open_error,read_error,set_error,commit_error,alloc_failures;
static unsigned opens,closes,reads,writes,commits;
static size_t returned_length;
static char stored[4][MOONRAKER_TLS_CA_PEM_MAX],pending[MOONRAKER_TLS_CA_PEM_MAX];
static int pending_index;
static bool missing_key;
static const char *old_ca="-----BEGIN CERTIFICATE-----\nold\n-----END CERTIFICATE-----\n";
static const char *new_ca="-----BEGIN CERTIFICATE-----\nnew\n-----END CERTIFICATE-----\n";
void *test_calloc(size_t n,size_t size,int caps){(void)caps;if(alloc_failures){alloc_failures--;return NULL;}return calloc(n,size);}
int nvs_open(const char *space,int mode,int *handle){assert(!strcmp(space,"moon_tls")&&(mode==0||mode==1));opens++;*handle=1;return open_error;}
static int index_for(const char *key){int i=-1;assert(sscanf(key,"ca_pem_%d",&i)==1&&i>=0&&i<4);return i;}
int nvs_get_str(int h,const char *key,char *out,size_t *length){
 assert(h==1&&*length==sizeof(stored[0]));reads++;
 memcpy(out,stored[index_for(key)],sizeof(stored[0]));
 *length=returned_length?returned_length:strlen(stored[index_for(key)])+1;
 return missing_key?ESP_ERR_NVS_NOT_FOUND:read_error;
}
int nvs_set_str(int h,const char *key,const char *pem){assert(h==1);writes++;pending_index=index_for(key);strlcpy(pending,pem,sizeof(pending));return set_error;}
int nvs_commit(int h){assert(h==1);commits++;if(!commit_error)strcpy(stored[pending_index],pending);return commit_error;}
void nvs_close(int h){assert(h==1);closes++;}
static void reset(void){
 for(unsigned i=0;i<4;i++){free(s_pem[i]);s_pem[i]=NULL;s_loaded[i]=false;}
 memset(stored,0,sizeof(stored));memset(profiles,0,sizeof(profiles));
 open_error=read_error=set_error=commit_error=alloc_failures=0;
 opens=closes=reads=writes=commits=0;returned_length=0;missing_key=false;
 for(unsigned i=0;i<4;i++)strcpy(stored[i],old_ca);
 profiles[0]=(moonraker_profile_t){.configured=true,.port=7126,.secure_transport=true};strcpy(profiles[0].host,"printer.local");
}
static void loaded(int i){const char *pem=moonraker_tls_trust_store_pem_for_profile(i);assert(pem&&!strcmp(pem,old_ca));}
static void file_bytes(const char *path,const char *data,size_t size){FILE *f=fopen(path,"wb");assert(f);assert(fwrite(data,1,size,f)==size);assert(!fclose(f));}
int main(int argc,char **argv){
 assert(argc==2);const char *file=argv[1];reset();
 open_error=-9;assert(!moonraker_tls_trust_store_pem_for_profile(0));
 open_error=0;loaded(0);assert(opens==2&&reads==1&&closes==1);
 unsigned before=opens;loaded(0);assert(opens==before);
 reset();read_error=-9;assert(!moonraker_tls_trust_store_pem_for_profile(0));
 assert(!s_loaded[0]&&!s_pem[0][0]);read_error=0;loaded(0);assert(opens==closes&&reads==2);
 reset();open_error=ESP_ERR_NVS_NOT_FOUND;assert(!moonraker_tls_trust_store_pem_for_profile(0));
 before=opens;assert(!moonraker_tls_trust_store_pem_for_profile(0)&&opens==before);
 open_error=0;assert(moonraker_tls_trust_store_set_pem_for_profile(0,new_ca));
 assert(!strcmp(moonraker_tls_trust_store_pem_for_profile(0),new_ca));
 reset();missing_key=true;assert(!moonraker_tls_trust_store_pem_for_profile(0));
 before=reads;assert(!moonraker_tls_trust_store_pem_for_profile(0)&&reads==before);
 reset();alloc_failures=2;assert(!moonraker_tls_trust_store_pem_for_profile(0)&&!opens);loaded(0);
 reset();alloc_failures=1;loaded(0); /* Internal-RAM fallback. */
 for(unsigned mode=0;mode<7;mode++){
  reset();
  if(mode==0)returned_length=sizeof(stored[0])+1;
  if(mode==1){memset(stored[0],'x',sizeof(stored[0]));returned_length=sizeof(stored[0]);}
  if(mode==2){returned_length=strlen(old_ca)+5;}
  if(mode==3)strcpy(stored[0],"not a PEM");
  if(mode==4)strcpy(stored[0],"-----BEGIN CERTIFICATE-----\nmissing end");
  if(mode==5)stored[0][0]=0;
  if(mode==6)returned_length=1;
  assert(!moonraker_tls_trust_store_pem_for_profile(0)&&!s_loaded[0]&&!s_pem[0][0]);
  strcpy(stored[0],old_ca);returned_length=0;loaded(0);
 }
 reset();read_error=-9;char url[256];const char *ca=(void*)1;
 assert(!moonraker_transport_security_build_http_url_for_endpoint("printer.local",7126,"/server/info",url,sizeof(url),&ca)&&!ca&&!url[0]);
 assert(!moonraker_transport_security_build_websocket_uri_for_endpoint("printer.local",7126,url,sizeof(url),&ca)&&!ca&&!url[0]);
 read_error=0;
 assert(moonraker_transport_security_build_http_url_for_endpoint("printer.local",7126,"/server/info",url,sizeof(url),&ca)&&ca&&!strcmp(ca,old_ca)&&!strcmp(url,"https://printer.local:7126/server/info"));
 assert(moonraker_transport_security_build_websocket_uri_for_endpoint("printer.local",7126,url,sizeof(url),&ca)&&ca&&!strcmp(url,"wss://printer.local:7126/websocket"));
 loaded(1);assert(s_pem[0]!=s_pem[1]);
 for(unsigned mode=0;mode<3;mode++){
  open_error=mode==0?-9:0;set_error=mode==1?-9:0;commit_error=mode==2?-9:0;
  assert(!moonraker_tls_trust_store_set_pem_for_profile(0,new_ca));loaded(0);
 }
 open_error=set_error=commit_error=0;
 before=writes;file_bytes(file,"invalid",7);
 assert(!moonraker_tls_trust_store_import_file_for_profile(0,file)&&writes==before);loaded(0);
 char bytes[MOONRAKER_TLS_CA_PEM_MAX];memset(bytes,'x',sizeof(bytes));
 file_bytes(file,bytes,sizeof(bytes));assert(!moonraker_tls_trust_store_import_file_for_profile(0,file));loaded(0);
 size_t n=strlen(new_ca);memcpy(bytes,new_ca,n+1);memcpy(bytes+n+1,"hidden",6);
 file_bytes(file,bytes,n+7);assert(!moonraker_tls_trust_store_import_file_for_profile(0,file));loaded(0);
 file_bytes(file,new_ca,n);commit_error=-9;assert(!moonraker_tls_trust_store_import_file_for_profile(0,file));loaded(0);
 commit_error=0;assert(moonraker_tls_trust_store_import_file_for_profile(0,file));
 assert(!strcmp(moonraker_tls_trust_store_pem_for_profile(0),new_ca));
 assert(moonraker_tls_trust_store_set_pem_for_profile(0,moonraker_tls_trust_store_pem_for_profile(0)));loaded(1);
 assert(!moonraker_tls_trust_store_pem_for_profile(-1)&&!moonraker_tls_trust_store_pem_for_profile(4));
 assert(!moonraker_tls_trust_store_set_pem_for_profile(4,old_ca));
 assert(!moonraker_tls_trust_store_import_file_for_profile(0,"/no/such/certificate"));
 /* Exact maximum PEM file remains supported; a further byte fails. */
 reset();memset(bytes,'x',sizeof(bytes));
 const char *begin="-----BEGIN CERTIFICATE-----\n",*end="\n-----END CERTIFICATE-----\n";
 memcpy(bytes,begin,strlen(begin));memcpy(bytes+4095-strlen(end),end,strlen(end));bytes[4095]=0;
 file_bytes(file,bytes,4095);assert(moonraker_tls_trust_store_import_file_for_profile(0,file));
 assert(strlen(moonraker_tls_trust_store_pem_for_profile(0))==4095);
 assert(!moonraker_tls_trust_store_set_pem_for_profile(0,NULL));
 bytes[4095]='x';file_bytes(file,bytes,4096);
 assert(!moonraker_tls_trust_store_import_file_for_profile(0,file));
 assert(strlen(moonraker_tls_trust_store_pem_for_profile(0))==4095);
 reset();puts("PASS: actual CA store transient read/allocation retry, absent storage caching, malformed strings, per-profile ownership, failed import preservation and HTTPS/WSS fail-closed recovery");
}
'''
with tempfile.TemporaryDirectory(prefix='tls-trust-store-') as directory:
    p = Path(directory)
    for name, text in headers.items():
        (p / name).write_text(text)
    (p / 'moonraker_tls_trust_store.c').write_bytes(args.source.read_bytes())
    (p / 'test.c').write_text(fixture)
    subprocess.run(['cc', '-std=c11', '-D_POSIX_C_SOURCE=200809L', '-O1', '-g',
                    '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined',
                    '-fno-omit-frame-pointer', '-no-pie', '-include', str(p / 'compat.h'),
                    '-I', str(p), '-I', str(root / 'main'), str(p / 'test.c'),
                    str(root / 'main/moonraker_transport_security_controller.c'), '-o', str(p / 'test')], check=True)
    subprocess.run([str(p / 'test'), str(p / 'ca.pem')], check=True,
                   env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0'})
