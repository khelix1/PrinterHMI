#!/usr/bin/env python3
"""Compile the actual profile controller against fault-injected typed NVS calls."""
from pathlib import Path
import argparse
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--source', type=Path, default=root / 'main/moonraker_config_controller.c')
args = parser.parse_args()
headers = {
    'nvs.h': '''#pragma once
#include <stddef.h>
#include <stdint.h>
typedef int nvs_handle_t;
#define NVS_READWRITE 1
int nvs_open(const char *,int,nvs_handle_t *);
int nvs_get_str(nvs_handle_t,const char *,char *,size_t *);
int nvs_get_u8(nvs_handle_t,const char *,uint8_t *);
int nvs_get_i32(nvs_handle_t,const char *,int32_t *);
int nvs_set_str(nvs_handle_t,const char *,const char *);
int nvs_set_u8(nvs_handle_t,const char *,uint8_t);
int nvs_set_i32(nvs_handle_t,const char *,int32_t);
int nvs_erase_key(nvs_handle_t,const char *);
int nvs_commit(nvs_handle_t);
void nvs_close(nvs_handle_t);
''',
    'esp_err.h': '''#pragma once
#define ESP_OK 0
#define ESP_ERR_NVS_NOT_FOUND -1
typedef int esp_err_t;
const char *esp_err_to_name(int);
''',
    'esp_log.h': '''#pragma once
#include <stddef.h>
size_t strlcpy(char *,const char *,size_t);
#define ESP_LOGI(tag,...) ((void)(tag))
#define ESP_LOGW(tag,...) ((void)(tag))
#define ESP_LOGE(tag,...) ((void)(tag))
''',
}
with tempfile.TemporaryDirectory(prefix='profile-storage-') as directory:
    p = Path(directory)
    for name, content in headers.items():
        (p / name).write_text(content)
    subprocess.run(['cc', '-std=c11', '-D_POSIX_C_SOURCE=200809L', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-fno-pie', '-no-pie',
                    '-I', str(p), '-I', str(root / 'main'),
                    str(root / 'tools/audit/profile_storage_test.c'), str(args.source),
                    '-o', str(p / 'test')], check=True)
    subprocess.run([str(p / 'test')], check=True,
                   env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0'})
