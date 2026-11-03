#!/usr/bin/env python3
"""Exercise the full preview-store module with concurrent-publish/lock fixtures."""
from pathlib import Path
import argparse
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--source', type=Path, default=root / 'main/printer_preview_store.c')
args = parser.parse_args()
headers = {
    'lvgl.h': '#pragma once\ntypedef struct {int unused;} lv_image_dsc_t;\n',
    'bsp/esp-bsp.h': '#pragma once\n#include <stdbool.h>\nbool bsp_display_lock(unsigned);\nvoid bsp_display_unlock(void);\n',
    'bsp/display.h': '#pragma once\n',
    'esp_heap_caps.h': '#pragma once\nvoid heap_caps_free(void *);\n',
    'esp_log.h': '#pragma once\n#define ESP_LOGI(tag,...) ((void)(tag))\n',
    'compat.h': '#include <stddef.h>\nsize_t strlcpy(char *,const char *,size_t);\n',
}
fixture = r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "printer_preview_store.h"
#include "printer_preview_cache.h"
#include "moonraker_config_controller.h"
static moonraker_profile_t profile;
static bool locked, ready;
static unsigned lock_calls,fail_lock_at,publishes,loads,allocations,frees;
static int load_hook;
static uint32_t generation=1;
static lv_image_dsc_t image;
static char published[160];
size_t strlcpy(char *out,const char *in,size_t size){size_t n=strlen(in);if(size)snprintf(out,size,"%s",in);return n;}
bool bsp_display_lock(unsigned timeout){assert(!locked&&(timeout==50||timeout==1000));lock_calls++;if(lock_calls==fail_lock_at)return false;locked=true;return true;}
void bsp_display_unlock(void){assert(locked);locked=false;}
const moonraker_profile_t *moonraker_config_profile(int index){return index==0?&profile:NULL;}
int moonraker_config_active_profile_index(void){return 0;}
uint32_t moonraker_config_generation(void){return generation;}
const lv_image_dsc_t *printer_preview_cache_image(int index,const char **file,uint32_t *revision){assert(locked&&index==0);(void)file;(void)revision;return ready?&image:NULL;}
bool printer_preview_cache_publish_png(int index,const char *host,int port,const char *file,const uint8_t *png,size_t size,int w,int h){assert(locked&&index==0&&!strcmp(host,profile.host)&&port==profile.port&&png&&size==8&&w==286&&h==215);publishes++;ready=true;snprintf(published,sizeof(published),"%s",file);return true;}
void heap_caps_free(void *p){if(p)frees++;free(p);}
bool thumbnail_manager_store_cache_file(const char *path,const uint8_t *png,size_t size){(void)path;(void)png;(void)size;assert(!"Unexpected save reached I/O");return false;}
bool thumbnail_manager_load_cache_file(const char *path,uint8_t **out,size_t *size){
 assert(!locked&&!strcmp(path,"previews/profile_0.png"));loads++;*out=malloc(8);assert(*out);allocations++;*size=8;
 if(load_hook==1)ready=true; /* The live worker publishes during SD I/O. */
 if(load_hook==2){generation++;strcpy(profile.host,"new-printer.local");}
 return true;
}
static void reset(void){assert(!locked&&allocations==frees);profile=(moonraker_profile_t){.configured=true,.port=7125};strcpy(profile.host,"printer.local");ready=false;lock_calls=fail_lock_at=publishes=loads=allocations=frees=0;load_hook=0;published[0]=0;generation++;printer_preview_store_reset_restore();}
static void metadata(const char *file){FILE *f=fopen("previews/profile_0.meta","wb");assert(f);fprintf(f,"PRINTERHMI_PREVIEW_V1\nprinter.local\n7125\n%s\n",file);assert(!fclose(f));}
static void retry_cycle(void){for(int i=0;i<4;i++)printer_preview_store_restore_one(true);}
int main(int argc,char **argv){
 assert(argc==2&&!chdir(argv[1])&&!mkdir("previews",0700));metadata("old.gcode");
 reset();printer_preview_store_restore_one(true);assert(publishes==1&&loads==1&&allocations==frees&&!strcmp(published,"old.gcode"));
 reset();ready=true;printer_preview_store_restore_one(true);assert(!loads&&!publishes);
 reset();load_hook=1;printer_preview_store_restore_one(true);assert(loads==1&&!publishes&&allocations==frees);
 reset();fail_lock_at=1;printer_preview_store_restore_one(true);assert(!loads&&!publishes);
 fail_lock_at=0;retry_cycle();assert(loads==1&&publishes==1);
 reset();fail_lock_at=2;printer_preview_store_restore_one(true);assert(loads==1&&!publishes&&allocations==frees);
 fail_lock_at=0;retry_cycle();assert(loads==2&&publishes==1&&allocations==frees);
 reset();load_hook=2;printer_preview_store_restore_one(true);assert(!publishes&&allocations==frees);
 reset();metadata("part.gcode\nextra");printer_preview_store_restore_one(true);assert(!loads&&!publishes);
 metadata("old.gcode");reset();
 uint8_t signature[]={137,'P','N','G',13,10,26,10};
 assert(!printer_preview_store_store_png(0,"printer.local",7125,"bad\nfile",signature,8));
 assert(!printer_preview_store_store_png(0,"printer.local",7125,"bad\rfile",signature,8));
 char long_file[161];memset(long_file,'f',160);long_file[160]=0;
 assert(!printer_preview_store_store_png(0,"printer.local",7125,long_file,signature,8));
 assert(allocations==frees&&!locked);
 puts("PASS: full preview store locks cache reads, retains newer live images, rejects stale profile loads, retries busy locks and frees buffers on every tested path");
}
'''
with tempfile.TemporaryDirectory(prefix='preview-restore-') as directory:
    p = Path(directory)
    for name, content in headers.items():
        path = p / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content)
    source = args.source.read_text().replace('#define STORE_DIR "/sdcard/hmi/profile_previews"', '#define STORE_DIR "previews"')
    (p / 'store.c').write_text(source)
    (p / 'test.c').write_text(fixture)
    subprocess.run(['cc', '-std=c11', '-D_POSIX_C_SOURCE=200809L', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-fno-pie', '-no-pie',
                    '-include', str(p / 'compat.h'), '-I', str(p), '-I', str(root / 'main'),
                    str(p / 'test.c'), str(p / 'store.c'), '-o', str(p / 'test')], check=True)
    subprocess.run([str(p / 'test'), str(p)], check=True,
                   env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0'})
