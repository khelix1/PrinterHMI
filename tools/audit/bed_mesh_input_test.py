#!/usr/bin/env python3
"""Real Bed Mesh JSON boundary: reject nonfinite/unrepresentable values before cache publication."""
from pathlib import Path
import argparse,os,subprocess,tempfile
root=Path(__file__).resolve().parents[2]
parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--cjson-dir',type=Path,required=True);parser.add_argument('--source',type=Path,default=root/'main/bed_mesh_controller.c');args=parser.parse_args()
code=r'''
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "mesh_under_test.c"
void *xSemaphoreCreateMutex(void){return (void*)1;}
int xSemaphoreTake(void *m,unsigned ticks){assert(m==(void*)1);(void)ticks;return 1;}
int xSemaphoreGive(void *m){assert(m==(void*)1);return 1;}
static bool merge(const char *text){cJSON *j=cJSON_Parse(text);assert(j);bool ok=bed_mesh_controller_merge_status(j);cJSON_Delete(j);return ok;}
int main(void){
 bed_mesh_controller_init();
 assert(merge("{\"bed_mesh\":{\"mesh_matrix\":[[0,0.1],[-0.1,0]],\"mesh_min\":[10,20],\"mesh_max\":[200,210]}}"));
 float values[4];bed_mesh_snapshot_t before,after;assert(bed_mesh_controller_snapshot(&before,values,4,NULL,0));assert(before.valid);
 const char *invalid[]={"1e309","-1e309","1e300","-1e300"};
 for(unsigned i=0;i<4;i++){
  char text[256];snprintf(text,sizeof(text),"{\"bed_mesh\":{\"mesh_matrix\":[[0,%s],[0,0]],\"mesh_min\":[1,2]}}",invalid[i]);
  assert(!merge(text));assert(bed_mesh_controller_snapshot(&after,values,4,NULL,0));
  assert(after.valid&&after.minimum==before.minimum&&after.maximum==before.maximum&&after.mesh_min_x==10&&values[1]==.1f);
 }
 /* Invalid coordinates never poison an otherwise valid cached view. */
 assert(merge("{\"bed_mesh\":{\"mesh_matrix\":[[0,0],[0,0]],\"mesh_min\":[1e309,20],\"mesh_max\":[200,1e300]}}"));
 assert(bed_mesh_controller_snapshot(&after,values,4,NULL,0));assert(after.valid&&after.mesh_min_x==10&&after.mesh_max_y==210);
 assert(merge("{\"bed_mesh\":{\"profile_name\":\"active\"}}"));
 assert(bed_mesh_controller_snapshot(&after,values,4,NULL,0)&&after.valid);
 assert(merge("{\"bed_mesh\":{\"mesh_matrix\":[]}}"));
 assert(!bed_mesh_controller_snapshot(&after,values,4,NULL,0)||!after.valid);
 free(s.values);free(s.profile_names);
 puts("PASS: real Bed Mesh JSON rejects nonfinite/unrepresentable heights, preserves last valid mesh and safe coordinates, retains partial/clear semantics");
}
'''
with tempfile.TemporaryDirectory(prefix='bed-mesh-input-') as folder:
 p=Path(folder);(p/'freertos').mkdir()
 (p/'esp_heap_caps.h').write_text('#pragma once\n#include <stdlib.h>\n#define MALLOC_CAP_SPIRAM 1\n#define MALLOC_CAP_8BIT 2\n#define heap_caps_calloc(n,s,c) calloc(n,s)\n#define heap_caps_free free\n')
 (p/'freertos/FreeRTOS.h').write_text('#pragma once\n#define portMAX_DELAY 0xffffffffU\n#define pdTRUE 1\n')
 (p/'freertos/semphr.h').write_text('#pragma once\ntypedef void *SemaphoreHandle_t;\nvoid *xSemaphoreCreateMutex(void);\nint xSemaphoreTake(void*,unsigned);\nint xSemaphoreGive(void*);\n')
 (p/'mesh_under_test.c').write_bytes(args.source.read_bytes());(p/'test.c').write_text(code);exe=p/'test'
 subprocess.run(['cc','-D_POSIX_C_SOURCE=200809L','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie','-I',str(p),'-I',str(root/'main'),'-I',str(args.cjson_dir),str(p/'test.c'),str(args.cjson_dir/'cJSON.c'),'-lm','-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
