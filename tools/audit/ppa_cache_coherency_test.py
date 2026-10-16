#!/usr/bin/env python3
"""Compile patched pinned PPA source with real LVGL and ESP cache/DMA stubs."""
from pathlib import Path
import argparse
import hashlib
import importlib.util
import shutil
import subprocess
import tempfile
root=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--lvgl-dir',type=Path,required=True);p.add_argument('--lvgl-lib',type=Path,required=True);a=p.parse_args()
spec=importlib.util.spec_from_file_location('cache_patch',root/'tools/apply_lvgl95_ppa_cache.py');patch=importlib.util.module_from_spec(spec);spec.loader.exec_module(patch)
with tempfile.TemporaryDirectory(prefix='ppa-cache-') as directory:
 tmp=Path(directory);component=tmp/'lvgl'
 for f in patch.FILES:
  dst=component/f;dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(a.lvgl_dir/f,dst)
 # Every file is checked before mutation, even if the first one is valid.
 last=component/list(patch.FILES)[-1];old=last.read_bytes();last.write_bytes(old+b'\n/* local change */\n');before={f:(component/f).read_bytes() for f in patch.FILES}
 try:patch.apply(component)
 except RuntimeError:pass
 else:raise AssertionError('Expected mismatched dependency refusal')
 assert before=={f:(component/f).read_bytes() for f in patch.FILES};last.write_bytes(old)
 patch.apply(component);patch.apply(component)
 for f,(_,digest,_) in patch.FILES.items():assert hashlib.sha256((component/f).read_bytes()).hexdigest()==digest
 (tmp/'driver').mkdir();(tmp/'hal').mkdir()
 (tmp/'sdkconfig.h').write_text('#define CONFIG_LV_DRAW_BUF_ALIGN 64\n#define CONFIG_CACHE_L2_CACHE_LINE_SIZE 64\n#define CONFIG_SOC_PPA_SUPPORTED 1\n')
 (tmp/'esp_err.h').write_text('#pragma once\ntypedef int esp_err_t;\n#define ESP_OK 0\n')
 (tmp/'esp_heap_caps.h').write_text('#pragma once\n')
 (tmp/'esp_log.h').write_text('#pragma once\n')
 (tmp/'hal/color_hal.h').write_text('#pragma once\n')
 (tmp/'esp_cache.h').write_text('''#pragma once
#include <stddef.h>
#include "esp_err.h"
#define ESP_CACHE_MSYNC_FLAG_DIR_C2M 1
#define ESP_CACHE_MSYNC_FLAG_DIR_M2C 2
#define ESP_CACHE_MSYNC_FLAG_TYPE_DATA 4
size_t esp_cache_get_line_size_by_addr(const void *);
esp_err_t esp_cache_msync(void *,size_t,int);
''')
 (tmp/'driver/ppa.h').write_text('''#pragma once
#include "esp_err.h"
typedef void *ppa_client_handle_t;
typedef int ppa_srm_color_mode_t;
typedef int ppa_fill_color_mode_t;
typedef int ppa_blend_color_mode_t;
typedef struct { int oper_type; int max_pending_trans_num; int data_burst_length; } ppa_client_config_t;
enum { PPA_SRM_COLOR_MODE_RGB565,PPA_SRM_COLOR_MODE_RGB888,PPA_SRM_COLOR_MODE_ARGB8888, PPA_OPERATION_SRM,PPA_OPERATION_FILL,PPA_OPERATION_BLEND,
PPA_DATA_BURST_LENGTH_128,PPA_DATA_BURST_LENGTH_64,PPA_DATA_BURST_LENGTH_32,PPA_DATA_BURST_LENGTH_16,PPA_DATA_BURST_LENGTH_8,
PPA_FILL_COLOR_MODE_RGB565,PPA_FILL_COLOR_MODE_RGB888,PPA_FILL_COLOR_MODE_ARGB8888,
PPA_BLEND_COLOR_MODE_RGB565,PPA_BLEND_COLOR_MODE_RGB888,PPA_BLEND_COLOR_MODE_ARGB8888 };
esp_err_t ppa_register_client(const ppa_client_config_t *,ppa_client_handle_t *);
esp_err_t ppa_unregister_client(ppa_client_handle_t);
''')
 flags=['-DLV_CONF_SKIP','-DLV_KCONFIG_IGNORE','-DLV_USE_STDLIB_MALLOC=1','-DLV_USE_PPA=1','-DLV_USE_PPA_IMG=0','-DLV_PPA_BURST_LENGTH=128','-DLV_DRAW_BUF_ALIGN=64']
 flags += [f'-DLV_FONT_MONTSERRAT_{n}=1' for n in range(12,34,2)]+['-DLV_FONT_MONTSERRAT_48=1']
 original=a.lvgl_dir/'src/draw/espressif/ppa';patched=component/'src/draw/espressif/ppa'
 exe=tmp/'cache_test'
 subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror',*flags,'-I',str(tmp),'-I',str(patched),'-I',str(original),'-I',str(a.lvgl_dir),str(root/'tools/audit/ppa_cache_coherency_test.c'),str(a.lvgl_lib),'-lm','-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
 # Prove the same handoff assertions reject the original pinned implementation.
 import resource
 resource.setrlimit(resource.RLIMIT_CORE,(0,0))
 baseline_dir=tmp/'baseline';baseline_dir.mkdir()
 for name in ['lv_draw_ppa.c','lv_draw_ppa_buf.c']:shutil.copy2(original/name,baseline_dir/name)
 baseline_fixture=tmp/'baseline_repro.c'
 text=(root/'tools/audit/ppa_cache_coherency_test.c').read_text().split(' /* Flush full rows,')[0]+' lv_deinit();return 0;\n}\n'
 baseline_fixture.write_text(text)
 baseline_exe=tmp/'baseline_repro'
 subprocess.run(['cc','-std=c11','-O2',*flags,'-I',str(tmp),'-I',str(baseline_dir),'-I',str(original),'-I',str(a.lvgl_dir),str(baseline_fixture),str(a.lvgl_lib),'-lm','-o',str(baseline_exe)],check=True)
 result=subprocess.run([str(baseline_exe)],cwd=tmp,capture_output=True,text=True)
 assert result.returncode!=0 and 'Assertion' in result.stderr
 print('PASS: original pinned PPA code fails the same cache handoff regression assertion')
 print('PASS: pinned cache patch apply/no-op/hash checks and all-files-before-mutation refusal')
