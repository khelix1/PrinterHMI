#!/usr/bin/env python3
"""Exercise the real file-detail worker and encoder with a deferred task adapter."""
from pathlib import Path
import argparse,os,re,subprocess,tempfile
root=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--lvgl-dir',type=Path,required=True);args=p.parse_args()
with tempfile.TemporaryDirectory() as folder:
 t=Path(folder);(t/'freertos').mkdir();(t/'bsp').mkdir()
 (t/'freertos/FreeRTOS.h').write_text('#pragma once\n#include <stddef.h>\ntypedef int portMUX_TYPE;\n#define portMUX_INITIALIZER_UNLOCKED 0\n#define taskENTER_CRITICAL(x) ((void)(x))\n#define taskEXIT_CRITICAL(x) ((void)(x))\n#define pdMS_TO_TICKS(x) (x)\n#define pdPASS 1\nsize_t strlcpy(char*,const char*,size_t);\n')
 (t/'freertos/task.h').write_text('#pragma once\nstatic inline void vTaskDelay(unsigned n){(void)n;}\nstatic inline void vTaskDelete(void*p){(void)p;}\nint xTaskCreatePinnedToCore(void(*)(void*),const char*,unsigned,void*,unsigned,void*,int);\n')
 (t/'bsp/esp-bsp.h').write_text('#pragma once\n#include <stdbool.h>\nbool bsp_display_lock(unsigned);\nvoid bsp_display_unlock(void);\n')
 source=(root/'main/thumbnail_manager.c').read_text();match=re.search(r'^void thumbnail_manager_url_encode\(.*?^\}',source,re.M|re.S);assert match
 (t/'encoder.inc').write_text(match.group(0))
 exe=t/'test';subprocess.run(['cc','-std=c11','-O1','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie','-DLV_CONF_SKIP','-DLV_KCONFIG_IGNORE','-I',str(t),'-I',str(root/'main'),'-I',str(root/'tools/theme-maker/linux/native/shims'),'-I',str(args.lvgl_dir),str(root/'tools/audit/file_detail_ownership_test.c'),'-lm','-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True,env={**os.environ,"ASAN_OPTIONS":os.environ.get("ASAN_OPTIONS","detect_leaks=0")})
