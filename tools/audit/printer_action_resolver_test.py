#!/usr/bin/env python3
"""Exercise actual macro construction and action resolution at command limits."""
from pathlib import Path
import argparse
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--source', type=Path, default=root / 'main/printer_action_resolver.c')
args = parser.parse_args()
fixture = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "macro_controller.h"
#include "printer_action_resolver.h"
static const char *names[]={"HOME_MACHINE", "QGL", "CUSTOM"};
static const char *only_macro;
static size_t catalog_count(void){return only_macro?1:3;}
void macro_controller_status(macro_controller_status_t *out){memset(out,0,sizeof(*out));out->count=catalog_count();}
bool macro_controller_get(size_t i,char *out,size_t size){if(i>=catalog_count())return false;snprintf(out,size,"%s",only_macro?only_macro:names[i]);return true;}
int main(void){
 printer_action_resolution_t out;
 char macro[64],keys[6][32],values[6][65],command[MACRO_COMMAND_MAX];
 memset(macro,'M',63);macro[63]=0;
 macro_parameter_value_t params[6];
 for(unsigned i=0;i<6;i++){
  memset(keys[i],'P',31);keys[i][30]='A'+i;keys[i][31]=0;
  memset(values[i],'1',64);values[i][i==5?58:64]=0;
  params[i]=(macro_parameter_value_t){keys[i],values[i]};
 }
 const char *error=NULL;
 assert(macro_parameter_build_command(macro,params,6,command,sizeof(command),&error));
 assert(!error&&strlen(command)==639);
 assert(printer_action_resolver_resolve(command,&out));
 assert(!strcmp(command,out.command)&&!out.macro_used&&out.confirmation==PRINTER_ACTION_CONFIRM_NONE);
 char oversized[641];memset(oversized,'G',640);oversized[640]=0;
 memset(&out,0x55,sizeof(out));assert(!printer_action_resolver_resolve(oversized,&out));
 assert(!out.command[0]&&!out.macro_used&&out.confirmation==PRINTER_ACTION_CONFIRM_NONE);
 assert(!printer_action_resolver_resolve(NULL,&out)&&!out.command[0]);
 assert(!printer_action_resolver_resolve("",&out)&&!out.command[0]);
 assert(!printer_action_resolver_resolve("G28",NULL));
 assert(printer_action_resolver_resolve("quad_gantry_level",&out));
 assert(!strcmp(out.command,"QGL")&&out.macro_used&&out.confirmation==PRINTER_ACTION_CONFIRM_MOTION);
 assert(printer_action_resolver_resolve("G28\nQUAD_GANTRY_LEVEL",&out));assert(!strcmp(out.command,"G28\nQGL")&&out.macro_used);
 assert(printer_action_resolver_resolve("HOME_ALL",&out));assert(!strcmp(out.command,"G28")&&!out.macro_used);
 assert(printer_action_resolver_resolve("custom",&out));assert(!strcmp(out.command,"CUSTOM")&&out.confirmation==PRINTER_ACTION_CONFIRM_UNKNOWN_MACRO);
 assert(printer_action_resolver_resolve("CUSTOM X=123",&out));assert(!strcmp(out.command,"CUSTOM X=123")&&!out.macro_used);

 /* Every calibration alias retains explicit homing; unprefixed actions stay unprefixed. */
 const char *canonical[]={"Z_TILT_ADJUST","QUAD_GANTRY_LEVEL","SCREWS_TILT_CALCULATE",
  "BED_MESH_CALIBRATE","PROBE_CALIBRATE","SHAPER_CALIBRATE"};
 const char *aliases[][4]={
  {"Z_TILT","Z_TILT_CALIBRATE","CALIBRATE_Z_TILT",NULL},
  {"QGL","GANTRY_LEVEL","QUAD_GANTRY_LEVEL",NULL},
  {"SCREWS_TILT","LEVEL_SCREWS","SCREWS_TILT_CALCULATE",NULL},
  {"BED_MESH_CALIBRATE","CALIBRATE_BED",NULL,NULL},
  {"PROBE_CALIBRATE","Z_OFFSET_CALIBRATE","CALIBRATE_Z_OFFSET",NULL},
  {"SHAPER_CALIBRATE","INPUT_SHAPER_CALIBRATE",NULL,NULL}};
 for(size_t i=0;i<sizeof(canonical)/sizeof(*canonical);i++){
  for(size_t j=0;j<4&&aliases[i][j];j++){
   char requested[128],expected[128];only_macro=aliases[i][j];
   snprintf(requested,sizeof(requested),"G28\n%s",canonical[i]);
   snprintf(expected,sizeof(expected),"G28\n%s",only_macro);
   assert(printer_action_resolver_resolve(requested,&out));
   assert(!strcmp(out.command,expected)&&out.macro_used&&out.confirmation==PRINTER_ACTION_CONFIRM_MOTION);
   assert(printer_action_resolver_resolve(canonical[i],&out));
   assert(!strcmp(out.command,only_macro)&&out.macro_used);
  }
  only_macro="UNRELATED";char requested[128];snprintf(requested,sizeof(requested),"G28\n%s",canonical[i]);
  assert(printer_action_resolver_resolve(requested,&out));assert(!strcmp(out.command,requested)&&!out.macro_used);
 }
 only_macro="QGL";
 assert(printer_action_resolver_resolve("G28 X\nQUAD_GANTRY_LEVEL",&out));
 assert(!strcmp(out.command,"G28 X\nQUAD_GANTRY_LEVEL")&&!out.macro_used);
 assert(printer_action_resolver_resolve("G28\nQUAD_GANTRY_LEVEL EXTRA=1",&out));
 assert(!strcmp(out.command,"G28\nQUAD_GANTRY_LEVEL EXTRA=1")&&!out.macro_used);
 only_macro="HOME_ALL";assert(printer_action_resolver_resolve("HOME_ALL",&out));
 assert(!strcmp(out.command,"HOME_ALL")&&out.macro_used);
 puts("PASS: explicit homing retained for all calibration aliases and unchanged fallbacks; real macro builder/resolver, 639-byte exact preservation, oversized command refusal with cleared output, alias selection, confirmations and standard fallbacks");
}
'''
with tempfile.TemporaryDirectory(prefix='action-command-bounds-') as directory:
    p = Path(directory)
    (p / 'test.c').write_text(fixture)
    subprocess.run(['cc', '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-no-pie',
                    '-I', str(root / 'main'), str(p / 'test.c'), str(args.source),
                    str(root / 'main/macro_parameter_utils.c'), '-o', str(p / 'test')], check=True)
    env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0')
    subprocess.run([str(p / 'test')], env=env, check=True)
