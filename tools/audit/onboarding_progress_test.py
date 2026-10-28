#!/usr/bin/env python3
"""Exercise actual Setup progress persistence with transactional NVS faults."""
from pathlib import Path
import os
import subprocess
import tempfile
root=Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='onboarding-progress-') as directory:
    tmp=Path(directory)
    (tmp/'esp_err.h').write_text('#pragma once\ntypedef int esp_err_t;\n#define ESP_OK 0\n#define ESP_FAIL -1\n#define ESP_ERR_NVS_NOT_FOUND -2\n')
    (tmp/'nvs.h').write_text('#pragma once\n#include <stdint.h>\n#include "esp_err.h"\ntypedef unsigned nvs_handle_t;\n#define NVS_READONLY 0\n#define NVS_READWRITE 1\nesp_err_t nvs_open(const char*,int,nvs_handle_t*);\nesp_err_t nvs_get_u8(nvs_handle_t,const char*,uint8_t*);\nesp_err_t nvs_set_u8(nvs_handle_t,const char*,uint8_t);\nesp_err_t nvs_commit(nvs_handle_t);\nvoid nvs_close(nvs_handle_t);\n')
    executable=tmp/'progress'
    subprocess.run(['cc','-std=c11','-O1','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie','-I',str(tmp),'-I',str(root/'main'),str(root/'tools/audit/onboarding_progress_test.c'),'-o',str(executable)],check=True)
    env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0')
    subprocess.run([str(executable)],check=True,env=env)
