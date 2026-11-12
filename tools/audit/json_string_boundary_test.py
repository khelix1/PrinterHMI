#!/usr/bin/env python3
"""Shared bounded JSON parser: encoded NUL keys/values must not alias C strings."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[2]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--cjson-dir',type=Path,required=True)
parser.add_argument('--limits-header',type=Path,default=root/'main/json_document_limits.h')
args=parser.parse_args()
fixture=r'''
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "json_document_parse.h"
static cJSON *parse(const char *text){return json_document_parse_bounded(text,strlen(text),4096,16);}
int main(void){
 const char *bad[]={"\"\\u0000\"","{\"name\":\"case_led\\u0000extra\"}",
  "{\"case_led\\u0000extra\":1}","{\"name\":\"\\u0041\\u0000\\u0042\"}"};
 for(unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);i++)assert(!parse(bad[i]));
 const char *good[]={"{\"name\":\"\\u0041\\u0042\"}","{\"name\":\"\\\\u0000\"}",
  "{\"name\":\"\\\\u0000.txt\"}","{\"name\":\"\\u0001\\u00e9\"}"};
 for(unsigned i=0;i<sizeof(good)/sizeof(good[0]);i++){cJSON *j=parse(good[i]);assert(j);cJSON_Delete(j);}
 cJSON *literal=parse(good[1]);assert(!strcmp(cJSON_GetObjectItemCaseSensitive(literal,"name")->valuestring,"\\u0000"));cJSON_Delete(literal);
 /* Odd backslash parity decodes a NUL; even parity is literal text. */
 for(unsigned slashes=1;slashes<=8;slashes++){
  char json[64];unsigned length=0;json[length++]='"';
  for(unsigned i=0;i<slashes;i++)json[length++]='\\';
  memcpy(json+length,"u0000\"",6);length+=6;
  char *raw=malloc(length);assert(raw);memcpy(raw,json,length);
  cJSON *j=json_document_parse_bounded(raw,length,4096,16);free(raw);
  assert((j!=NULL)==(slashes%2==0));cJSON_Delete(j);
 }
 /* Exact-sized escaped input must not make the preflight read beyond its end. */
 const char *unfinished="\"\\u0000";
 for(size_t n=1;n<=strlen(unfinished);n++){
  char *raw=malloc(n);assert(raw);memcpy(raw,unfinished,n);
  assert(!json_document_parse_bounded(raw,n,4096,16));free(raw);
 }
 puts("PASS: bounded JSON rejects encoded NUL keys/values, retains literal backslashes/normal Unicode, and respects exact nonterminated lengths");
}
'''
with tempfile.TemporaryDirectory(prefix='json-string-boundary-') as directory:
    tmp=Path(directory)
    (tmp/'json_document_parse.h').write_bytes((root/'main/json_document_parse.h').read_bytes())
    (tmp/'json_document_limits.h').write_bytes(args.limits_header.read_bytes())
    (tmp/'test.c').write_text(fixture)
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
                    '-fno-pie','-no-pie','-I',str(tmp),'-I',str(args.cjson_dir),
                    str(tmp/'test.c'),str(args.cjson_dir/'cJSON.c'),'-o',str(tmp/'test')],check=True)
    subprocess.run([str(tmp/'test')],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
