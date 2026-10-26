#!/usr/bin/env python3
"""Run production thumbnail manager ownership and worker state checks with sanitizers."""
from pathlib import Path
import argparse
import os
import subprocess
import tempfile
root=Path(__file__).resolve().parents[2]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--lvgl-dir',type=Path)
args=parser.parse_args()
candidates=[args.lvgl_dir] if args.lvgl_dir else list((root/'managed_components').glob('*lvgl*'))
lvgl=next((p.resolve() for p in candidates if p and (p/'src/core/lv_obj.c').is_file()),None)
if lvgl is None:parser.error('LVGL sources required; supply --lvgl-dir')
with tempfile.TemporaryDirectory(prefix='thumbnail-manager-') as folder:
    tmp=Path(folder);(tmp/'freertos').mkdir()
    headers={
        'esp_heap_caps.h':'#pragma once\n#include <stddef.h>\n#include <stdint.h>\n#define MALLOC_CAP_SPIRAM 1\n#define MALLOC_CAP_8BIT 2\n#define MALLOC_CAP_INTERNAL 4\nvoid *heap_caps_malloc(size_t,uint32_t);\nvoid *heap_caps_calloc(size_t,size_t,uint32_t);\nvoid heap_caps_free(void *);\n',
        'esp_err.h':'#pragma once\ntypedef int esp_err_t;\n',
        'esp_http_client.h':'#pragma once\ntypedef void *esp_http_client_handle_t;\ntypedef struct { int event_id; } esp_http_client_event_t;\n',
        'esp_log.h':'#pragma once\n#include <stdio.h>\n#define ESP_LOGW(tag,...) do{(void)tag;if(0)printf(__VA_ARGS__);}while(0)\n#define ESP_LOGI ESP_LOGW\n',
        'freertos/FreeRTOS.h':'#pragma once\n#include <stddef.h>\ntypedef int portMUX_TYPE;\ntypedef int BaseType_t;\n#define portMUX_INITIALIZER_UNLOCKED 0\n#define portENTER_CRITICAL(p) ((void)(p))\n#define portEXIT_CRITICAL(p) ((void)(p))\n#define pdTRUE 1\n#define pdPASS 1\n#define pdMS_TO_TICKS(n) (n)\nsize_t strlcpy(char *,const char *,size_t);\n',
        'freertos/semphr.h':'#pragma once\ntypedef void *SemaphoreHandle_t;\nSemaphoreHandle_t xSemaphoreCreateMutex(void);\nint xSemaphoreTake(SemaphoreHandle_t,unsigned);\nint xSemaphoreGive(SemaphoreHandle_t);\nvoid vSemaphoreDelete(SemaphoreHandle_t);\n',
        'freertos/task.h':'#pragma once\nvoid vTaskDelay(unsigned);\nvoid vTaskDelete(void *);\nint xTaskCreatePinnedToCore(void (*)(void *),const char *,unsigned,void *,unsigned,void *,int);\n',
    }
    for name,text in headers.items():(tmp/name).write_text(text)
    executable=tmp/'test'
    subprocess.run(['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer',
        '-DLV_CONF_SKIP','-DLV_KCONFIG_IGNORE','-I',str(tmp),'-I',str(lvgl),'-I',str(root/'main'),str(root/'tools/audit/thumbnail_manager_test.c'),'-o',str(executable)],check=True)
    subprocess.run([str(executable)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
