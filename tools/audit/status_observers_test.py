#!/usr/bin/env python3
"""Build real LVGL host checks for status-banner string observer ownership. Supply --lvgl-dir if unmanaged."""
from pathlib import Path
import argparse
import concurrent.futures
import subprocess
import tempfile
root=Path(__file__).resolve().parents[2]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument("--lvgl-dir",type=Path)
parser.add_argument("--lvgl-lib",type=Path,help="Optional host LVGL archive built with these font/stdlib flags")
args=parser.parse_args()
candidates=[args.lvgl_dir] if args.lvgl_dir else list((root/"managed_components").glob("*lvgl*"))
lvgl=next((p for p in candidates if p and (p/"src/core/lv_obj.c").is_file()),None)
if lvgl is None:parser.error("LVGL sources not found; supply --lvgl-dir")
lvgl=lvgl.resolve()
flags=["-DLV_CONF_SKIP","-DLV_KCONFIG_IGNORE","-DLV_USE_STDLIB_MALLOC=1"]+[f"-DLV_FONT_MONTSERRAT_{n}=1" for n in range(12,34,2)]+["-DLV_FONT_MONTSERRAT_48=1"]
with tempfile.TemporaryDirectory(prefix="status-observers-") as directory:
    tmp=Path(directory)
    (tmp/"bsp").mkdir();(tmp/"freertos").mkdir()
    stubs={
        "esp_heap_caps.h":"#pragma once\n#include <stddef.h>\n#include <stdint.h>\n#define MALLOC_CAP_INTERNAL 1\n#define MALLOC_CAP_8BIT 2\n#define MALLOC_CAP_SPIRAM 4\nvoid *heap_caps_malloc(size_t,uint32_t);\nvoid *heap_caps_calloc(size_t,size_t,uint32_t);\nvoid heap_caps_free(void*);\n",
        "esp_err.h":"#pragma once\ntypedef int esp_err_t;\n#define ESP_OK 0\n#define ESP_FAIL -1\n",
        "esp_http_client.h":"#pragma once\ntypedef void *esp_http_client_handle_t;\ntypedef struct { int event_id; } esp_http_client_event_t;\n",
        "esp_log.h":"#pragma once\n#include <stdio.h>\n#define ESP_LOGI(t,...) do { (void)(t); if(0) printf(__VA_ARGS__); } while(0)\n#define ESP_LOGW ESP_LOGI\n#define ESP_LOGE ESP_LOGI\n",
        "bsp/esp-bsp.h":"#pragma once\n#include <stdbool.h>\nbool bsp_display_lock(unsigned);\nvoid bsp_display_unlock(void);\n",
        "freertos/FreeRTOS.h":"#pragma once\n#include <stddef.h>\ntypedef int BaseType_t;\n#define portMAX_DELAY 0xffffffffU\n#define pdTRUE 1\n#define pdPASS 1\n#define pdMS_TO_TICKS(ms) (ms)\nsize_t strlcpy(char*,const char*,size_t);\n",
        "freertos/queue.h":"#pragma once\n#include <stddef.h>\ntypedef void *QueueHandle_t;\nQueueHandle_t xQueueCreate(unsigned,size_t);\nint xQueueReceive(QueueHandle_t,void*,unsigned);\nint xQueueSend(QueueHandle_t,const void*,unsigned);\nint xQueueOverwrite(QueueHandle_t,const void*);\nint xQueueReset(QueueHandle_t);\n",
        "freertos/semphr.h":"#pragma once\ntypedef void *SemaphoreHandle_t;\nSemaphoreHandle_t xSemaphoreCreateMutex(void);\nint xSemaphoreTake(SemaphoreHandle_t,unsigned);\nint xSemaphoreGive(SemaphoreHandle_t);\n",
        "freertos/task.h":"#pragma once\nvoid vTaskDelay(unsigned);\nint xTaskCreatePinnedToCore(void (*)(void*),const char*,unsigned,void*,unsigned,void*,int);\n"
    }
    for name,text in stubs.items():(tmp/name).write_text(text)
    library=args.lvgl_lib
    if library is None:
        def compile_source(p):
            obj=tmp/(p.relative_to(lvgl).as_posix().replace("/","_")+".o")
            subprocess.run(["cc","-std=c11","-O1","-ffunction-sections","-fdata-sections",*flags,"-I",str(lvgl),"-c",str(p),"-o",str(obj)],check=True,capture_output=True)
            return str(obj)
        with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
            objects=list(pool.map(compile_source,(lvgl/"src").rglob("*.c")))
        library=tmp/"liblvgl.a"
        subprocess.run(["ar","rcs",str(library),*objects],check=True)
    sources=["ui_widgets.c","ui_theme.c","ui_theme_a.c","ui_theme_b.c","ui_theme_c.c","ui_font_fallback.c","ui_text.c"]
    executable=tmp/"status_observers"
    subprocess.run(["cc","-std=c11","-O2","-Wall","-Wextra","-Werror","-Wrestrict","-ffunction-sections","-fdata-sections",*flags,
                    "-I",str(tmp),"-I",str(lvgl),"-I",str(lvgl/"src"),"-I",str(root/"main"),
                    str(root/"tools/audit/status_observers_test.c"),*[str(root/"main"/p) for p in sources],
                    str(library),"-Wl,--gc-sections","-lm","-o",str(executable)],check=True)
    subprocess.run([str(executable)],check=True)
