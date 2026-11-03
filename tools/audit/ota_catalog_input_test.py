#!/usr/bin/env python3
"""Exercise actual OTA release parsing with malformed documents, versions and sizes."""
from pathlib import Path
import argparse
import os
import subprocess
import tempfile
root=Path(__file__).resolve().parents[2]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--source',type=Path,default=root/'main/ota_release_catalog.c')
args=parser.parse_args();source=args.source.read_text()
def function(name):
    at=source.index(name+'(')
    return source[source.rfind('\n',0,at)+1:source.index('\n}\n',at)+3]
names=['copy_json_string','normalize_notes','valid_nightly_tag','parse_releases']
if 'static bool parse_semver(' in source:names.insert(0,'parse_semver')
production='\n'.join(function(n) for n in names)
fixture=r'''
#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "json_document_parse.h"
#include "ota_release_catalog.h"
#include "firmware_identity.h"
#include "esp_app_desc.h"
#define RELEASE_RESPONSE_MAX (256U*1024U)
typedef struct {ota_release_entry_t entries[OTA_RELEASE_CATALOG_MAX];size_t count;} release_catalog_t;
static const esp_app_desc_t app={.version="7.3.0"};
const esp_app_desc_t *esp_app_get_description(void){return &app;}
'''
checks=r'''
static void document(char *out,size_t size,const char *tag,const char *asset_size){
 snprintf(out,size,"[{\"tag_name\":\"%s\",\"assets\":[{\"name\":\"PrinterHMI-%s-ota.bin\",\"browser_download_url\":\"https://github.com/khelix1/PrinterHMI/releases/download/v7.4.0/file.bin\",\"size\":%s}]}]",tag,tag,asset_size);
}
int main(void){
 int major=-1,minor=-1,patch=-1;
 assert(firmware_identity_parse_stable_version("v7.3.1",&major,&minor,&patch)&&major==7&&minor==3&&patch==1);
 const char *bad_versions[]={"v+7.3.1","v 7.3.1","v7.-3.1","v7.3.1-dev","v2147483648.0.0","v999999999999999999999999999999999.0.0","v7.3.1junk"};
 for(unsigned i=0;i<sizeof(bad_versions)/sizeof(bad_versions[0]);i++)assert(!firmware_identity_parse_stable_version(bad_versions[i],&major,&minor,&patch));
 char json[2048];release_catalog_t catalog={0};document(json,sizeof(json),"v7.4.0","3456789");
 assert(parse_releases(json,&catalog)&&catalog.count==1&&catalog.entries[0].asset_size==3456789&&catalog.entries[0].relation==OTA_RELEASE_NEWER);
 const char *bad_sizes[]={"1e999","-1","0.5","9223372036854775808","null","\"123\""};
 for(unsigned i=0;i<sizeof(bad_sizes)/sizeof(bad_sizes[0]);i++){
  memset(&catalog,0,sizeof(catalog));document(json,sizeof(json),"v7.4.0",bad_sizes[i]);assert(parse_releases(json,&catalog)&&!catalog.count);
 }
 memset(&catalog,0,sizeof(catalog));document(json,sizeof(json),"v7.4.0","9223372036854774784");assert(parse_releases(json,&catalog)&&catalog.count==1&&catalog.entries[0].asset_size==INT64_MAX-1023);
 for(unsigned i=0;i<sizeof(bad_versions)/sizeof(bad_versions[0]);i++){
  memset(&catalog,0,sizeof(catalog));document(json,sizeof(json),bad_versions[i],"1");assert(parse_releases(json,&catalog)&&!catalog.count);
 }
 memset(&catalog,0,sizeof(catalog));document(json,sizeof(json),"v7.4.0","1");strcat(json,"{}");assert(!parse_releases(json,&catalog)&&!catalog.count);
 char deep[128];memset(deep,'[',17);memset(deep+17,']',17);deep[34]=0;assert(!parse_releases(deep,&catalog));
 assert(!parse_releases("{\"message\":\"API failure\"}",&catalog));
 const char *nightly="[{\"tag_name\":\"nightly-2026-11-02-abcdef123456\",\"prerelease\":true,\"body\":\"Firmware identity: nightly-2026-11-02-abcdef123456\",\"assets\":[{\"name\":\"PrinterHMI-nightly-2026-11-02-abcdef123456.bin\",\"browser_download_url\":\"https://github.com/khelix1/PrinterHMI/asset.bin\",\"size\":3000000}]}]";
 memset(&catalog,0,sizeof(catalog));assert(parse_releases(nightly,&catalog)&&catalog.count==1&&catalog.entries[0].channel==OTA_RELEASE_CHANNEL_NIGHTLY&&catalog.entries[0].relation==OTA_RELEASE_SWITCH_CHANNEL);
 puts("PASS: actual OTA catalog strict JSON, bounded shared versions, valid Stable/Nightly channels, invalid asset sizes and safe int64 boundary conversion");
}
'''
with tempfile.TemporaryDirectory(prefix='ota-catalog-') as directory:
    p=Path(directory);(p/'esp_app_desc.h').write_text('#pragma once\ntypedef struct {char version[32];} esp_app_desc_t;\nconst esp_app_desc_t *esp_app_get_description(void);\n')
    (p/'test.c').write_text(fixture+production+checks)
    cjson=root/'tools/theme-maker/linux/native/vendor/cjson'
    subprocess.run(['cc','-std=c11','-D_POSIX_C_SOURCE=200809L','-DPRINTERHMI_FIRMWARE_CHANNEL=1','-Wall','-Wextra','-Werror','-fsanitize=address,undefined,float-cast-overflow','-fno-pie','-no-pie','-I',str(p),'-I',str(root/'main'),'-I',str(cjson),str(p/'test.c'),str(root/'main/firmware_identity.c'),str(cjson/'cJSON.c'),'-lm','-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'})
