#!/usr/bin/env python3
"""Run real-LVGL graphics-observer checks with deterministic ESP stubs."""
import argparse
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--lvgl-dir', type=Path, required=True)
p.add_argument('--lvgl-lib', type=Path, required=True)
a = p.parse_args()
flags = ['-DLV_CONF_SKIP', '-DLV_KCONFIG_IGNORE', '-DLV_USE_STDLIB_MALLOC=1']
flags += [f'-DLV_FONT_MONTSERRAT_{n}=1' for n in range(12,34,2)] + ['-DLV_FONT_MONTSERRAT_48=1']
with tempfile.TemporaryDirectory(prefix='graphics-diag-') as directory:
    tmp = Path(directory)
    (tmp/'sdkconfig.h').write_text('#define CONFIG_LV_DRAW_BUF_ALIGN 4\n#define CONFIG_CACHE_L2_CACHE_LINE_SIZE 64\n')
    (tmp/'esp_timer.h').write_text('#pragma once\nlong long esp_timer_get_time(void);\n')
    (tmp/'esp_heap_caps.h').write_text('''#pragma once
#include <stddef.h>
#define MALLOC_CAP_INTERNAL 1
#define MALLOC_CAP_SPIRAM 2
#define MALLOC_CAP_8BIT 4
static inline size_t heap_caps_get_free_size(unsigned caps) { (void)caps; return 2048; }
static inline size_t heap_caps_get_largest_free_block(unsigned caps) { (void)caps; return 1024; }
''')
    (tmp/'esp_log.h').write_text('#pragma once\nvoid audit_log(const char *, const char *, ...) __attribute__((format(printf,2,3)));\n#define ESP_LOGI audit_log\n')
    common = ['cc','-std=c11','-O2','-Wall','-Wextra','-Werror',*flags,
              '-I',str(tmp),'-I',str(a.lvgl_dir),'-I',str(root/'main')]
    for enabled in [False,True]:
        mode = ['-DCONFIG_PRINTERHMI_GRAPHICS_DIAGNOSTICS=1'] if enabled else []
        exe = tmp/('graphics_test_on' if enabled else 'graphics_test_off')
        subprocess.run([*common,*mode,str(root/'tools/audit/graphics_diagnostics_test.c'),str(a.lvgl_lib),'-lm','-o',str(exe)],check=True)
        subprocess.run([str(exe)],check=True)
        subprocess.run([*common,*mode,'-c',str(root/'main/ui_graphics_diagnostics.c'),'-o',str(tmp/('observer_on.o' if enabled else 'observer_off.o'))],check=True)
    print('PASS: both observer modes compile standalone with warnings as errors')
