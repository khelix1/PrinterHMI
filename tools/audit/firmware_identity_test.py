#!/usr/bin/env python3
"""Compile channel/build identity and compare rules; verify release CMake wiring."""
from pathlib import Path
import os,subprocess,tempfile,shutil
root=Path(__file__).resolve().parents[2]
source=r'''#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "firmware_identity.c"
static esp_app_desc_t app={.version="6.5.6"};
const esp_app_desc_t *esp_app_get_description(void){return &app;}
int main(void){
 firmware_identity_t i;firmware_identity_get(&i);assert(i.channel==PRINTERHMI_FIRMWARE_CHANNEL&&!strcmp(i.version,"6.5.6")&&!strcmp(i.revision,"abcdef123456"));
 char text[160];firmware_identity_format(text,sizeof(text));assert(strstr(text,firmware_identity_channel_name(i.channel)));
 i.channel=FIRMWARE_CHANNEL_STABLE;strcpy(i.version,"6.5.6");
 assert(firmware_identity_compare(&i,FIRMWARE_CHANNEL_STABLE,"v6.5.6")==0);
 assert(firmware_identity_compare(&i,FIRMWARE_CHANNEL_STABLE,"v6.5.7")==1);
 assert(firmware_identity_compare(&i,FIRMWARE_CHANNEL_STABLE,"v6.5.5")==-1);
 assert(firmware_identity_compare(&i,FIRMWARE_CHANNEL_NIGHTLY,"nightly-2026-10-11-abcdef")==3);
 i.channel=FIRMWARE_CHANNEL_CUSTOM;
 assert(firmware_identity_compare(&i,FIRMWARE_CHANNEL_STABLE,"v6.5.6")==3);
 assert(firmware_identity_compare(&i,FIRMWARE_CHANNEL_STABLE,"v6.5.7")==1);
 assert(firmware_identity_compare(&i,FIRMWARE_CHANNEL_STABLE,"v6.5.5")==-1);
 strcpy(i.version,"unknown");assert(firmware_identity_compare(&i,FIRMWARE_CHANNEL_STABLE,"v6.5.6")==3);
 i.channel=FIRMWARE_CHANNEL_NIGHTLY;strcpy(i.version,"nightly-2026-10-11-abcdef");
 assert(firmware_identity_compare(&i,FIRMWARE_CHANNEL_NIGHTLY,"nightly-2026-10-11-abcdef")==0);
 assert(firmware_identity_compare(&i,FIRMWARE_CHANNEL_NIGHTLY,"nightly-2026-10-12-123456")==1);
 assert(firmware_identity_compare(&i,FIRMWARE_CHANNEL_NIGHTLY,"nightly-2026-10-10-123456")==-1);
 assert(firmware_identity_compare(&i,FIRMWARE_CHANNEL_NIGHTLY,"nightly-2026-10-11-123456")==2);
 assert(firmware_identity_compare(&i,FIRMWARE_CHANNEL_STABLE,"v6.5.6")==3);
 puts("PASS: embedded channel/revision and Stable/Nightly/Custom installed/update/older/channel-switch/same-day rules");}
'''
with tempfile.TemporaryDirectory(prefix='firmware-identity-') as t:
 p=Path(t);(p/'esp_app_desc.h').write_text('#pragma once\ntypedef struct {char version[32];} esp_app_desc_t;\nconst esp_app_desc_t *esp_app_get_description(void);\n');(p/'test.c').write_text(source)
 for channel in range(3):
  subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror','-Wrestrict',f'-DPRINTERHMI_FIRMWARE_CHANNEL={channel}','-DPRINTERHMI_SOURCE_REVISION="abcdef123456"','-I',str(p),'-I',str(root/'main'),str(p/'test.c'),'-o',str(p/'test')],check=True)
  subprocess.run([str(p/'test')],check=True)
 (p/'dummy.c').write_text('int placeholder;\n')
 (p/'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.16)\nproject(identity C)\nfunction(idf_component_register)\n add_library(identity STATIC dummy.c)\n set(COMPONENT_LIB identity PARENT_SCOPE)\nendfunction()\ninclude("'+(root/'main/CMakeLists.txt').as_posix()+'")\nget_target_property(definitions identity COMPILE_DEFINITIONS)\nfile(WRITE "${CMAKE_BINARY_DIR}/identity.txt" "${definitions}")\n')
 for n,(version,env_channel,expected) in enumerate([('6.5.6','',0),('6.5.6','stable',1),('nightly-2026-10-11-abcdef','',2),('6.5.6','custom',0)]):
  env=os.environ.copy();env['PRINTERHMI_BUILD_CHANNEL']=env_channel
  if not shutil.which('cmake'):continue
  result=subprocess.run(['cmake','-S',str(p),'-B',str(p/f'build{n}'),f'-DPRINTERHMI_VERSION={version}'],env=env,capture_output=True,text=True)
  if result.returncode:raise RuntimeError(result.stdout+result.stderr)
  assert f'PRINTERHMI_FIRMWARE_CHANNEL={expected}' in (p/f'build{n}/identity.txt').read_text()
 assert 'PRINTERHMI_BUILD_CHANNEL=stable "$repo_dir/tools/build_v6_stack.sh"' in (root/'tools/release_stable.sh').read_text()
print('PASS: release entry point' + (' and CMake channel configurations' if shutil.which('cmake') else '; CMake execution skipped (cmake unavailable)'))
