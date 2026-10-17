#!/usr/bin/env python3
"""Exercise real theme persistence/dispatcher defaults against a transactional NVS fixture."""
from pathlib import Path
import argparse
import subprocess
import tempfile
root=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--lvgl-dir',type=Path)
a=p.parse_args()
candidates=[a.lvgl_dir] if a.lvgl_dir else list((root/'managed_components').glob('*lvgl*'))
lvgl=next((q for q in candidates if q and (q/'src/core/lv_obj.c').is_file()),None)
if lvgl is None:p.error('LVGL sources not found; supply --lvgl-dir')
with tempfile.TemporaryDirectory(prefix='theme-default-') as d:
 tmp=Path(d)
 (tmp/'esp_err.h').write_text('#pragma once\ntypedef int esp_err_t;\n#define ESP_OK 0\n#define ESP_FAIL -1\n#define ESP_ERR_NVS_NOT_FOUND 2\nconst char *esp_err_to_name(esp_err_t);\n')
 (tmp/'esp_log.h').write_text('#pragma once\n#include <stdio.h>\n#define ESP_LOGI(t,...) do{(void)(t);if(0)printf(__VA_ARGS__);}while(0)\n#define ESP_LOGW ESP_LOGI\n#define ESP_LOGE ESP_LOGI\n')
 (tmp/'nvs.h').write_text('#pragma once\n#include <stdint.h>\n#include <stddef.h>\n#include "esp_err.h"\ntypedef unsigned nvs_handle_t;\n#define NVS_READONLY 0\n#define NVS_READWRITE 1\nesp_err_t nvs_open(const char*,int,nvs_handle_t*);\nesp_err_t nvs_get_u8(nvs_handle_t,const char*,uint8_t*);\nesp_err_t nvs_get_str(nvs_handle_t,const char*,char*,size_t*);\nesp_err_t nvs_set_u8(nvs_handle_t,const char*,uint8_t);\nesp_err_t nvs_set_str(nvs_handle_t,const char*,const char*);\nesp_err_t nvs_erase_key(nvs_handle_t,const char*);\nesp_err_t nvs_commit(nvs_handle_t);\nvoid nvs_close(nvs_handle_t);\n')
 (tmp/'compat.h').write_text('#include <stddef.h>\nsize_t strlcpy(char*,const char*,size_t);\n')
 flags=['-include',str(tmp/'compat.h'),'-DLV_CONF_SKIP','-DLV_KCONFIG_IGNORE']+[f'-DLV_FONT_MONTSERRAT_{n}=1' for n in range(12,34,2)]+['-DLV_FONT_MONTSERRAT_48=1']
 exe=tmp/'theme_default'
 subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror','-ffunction-sections','-fdata-sections',*flags,'-I',str(tmp),'-I',str(lvgl.resolve()),'-I',str(root/'main'),str(root/'tools/audit/theme_default_test.c'),str(root/'main/theme_manager.c'),str(root/'main/ui_theme.c'),'-Wl,--gc-sections','-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
