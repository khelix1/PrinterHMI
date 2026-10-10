#!/usr/bin/env python3
"""Strict host compilation of the independently composed STUDIO pages."""
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
with tempfile.TemporaryDirectory(prefix="lvgl95-ui-") as directory:
    tmp=Path(directory)
    (tmp/"esp_heap_caps.h").write_text("#pragma once\n#include <stdlib.h>\n#define MALLOC_CAP_SPIRAM 1\n#define MALLOC_CAP_8BIT 2\n#define heap_caps_malloc(size,caps) malloc(size)\n#define heap_caps_free(ptr) free(ptr)\n")
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
    (tmp/"esp_heap_caps.h").write_text("#pragma once\n#include <stdlib.h>\n#define MALLOC_CAP_SPIRAM 1\n#define MALLOC_CAP_8BIT 2\n#define MALLOC_CAP_INTERNAL 4\n#define heap_caps_calloc(n,size,caps) calloc(n,size)\n#define heap_caps_free(p) free(p)\n#define heap_caps_malloc(size,caps) malloc(size)\n#define heap_caps_check_integrity(caps,print_errors) true\n")
    (tmp/"esp_err.h").write_text("#pragma once\ntypedef int esp_err_t;\n#define ESP_OK 0\n#define ESP_FAIL -1\n")
    (tmp/"esp_http_client.h").write_text("#pragma once\ntypedef void *esp_http_client_handle_t;\ntypedef struct { int event_id; } esp_http_client_event_t;\n")
    (tmp/"esp_log.h").write_text("#pragma once\n#include <stdio.h>\n#define ESP_LOGI(t,...) do { (void)(t); if(0) printf(__VA_ARGS__); } while(0)\n#define ESP_LOGW ESP_LOGI\n#define ESP_LOGE ESP_LOGI\n")
    (tmp/"esp_system.h").write_text("#pragma once\nvoid esp_restart(void);\n")
    (tmp/"nvs_flash.h").write_text("#pragma once\n#include \"esp_err.h\"\nesp_err_t nvs_flash_erase(void);\n")
    (tmp/"freertos").mkdir()
    (tmp/"freertos/FreeRTOS.h").write_text("#pragma once\n#include <stdint.h>\n#define pdMS_TO_TICKS(ms) (ms)\n")
    (tmp/"freertos/task.h").write_text("#pragma once\nvoid vTaskDelay(uint32_t ticks);\n")
    (tmp/"host_compat.h").write_text("#include <stddef.h>\nsize_t strlcpy(char *,const char *,size_t);\n")
    (tmp/"cJSON.h").write_text("#pragma once\ntypedef struct cJSON cJSON;\n")
    (tmp/"bsp").mkdir()
    (tmp/"bsp/display.h").write_text('#pragma once\n#include "esp_err.h"\nesp_err_t bsp_display_brightness_set(int value);\nesp_err_t bsp_display_backlight_off(void);\n')
    (tmp/"nvs.h").write_text('#pragma once\n#include "esp_err.h"\n#include <stdint.h>\ntypedef unsigned nvs_handle_t;\n#define NVS_READWRITE 1\n#define NVS_READONLY 0\n#define ESP_ERR_NVS_NOT_FOUND -2\nesp_err_t nvs_open(const char *,int,nvs_handle_t *);\nesp_err_t nvs_set_u8(nvs_handle_t,const char *,uint8_t);\nesp_err_t nvs_get_u8(nvs_handle_t,const char *,uint8_t *);\nesp_err_t nvs_commit(nvs_handle_t);\nvoid nvs_close(nvs_handle_t);\n')
    (tmp/"esp_app_desc.h").write_text('#pragma once\ntypedef struct { char version[32]; char idf_ver[32]; char date[16]; char time[16]; } esp_app_desc_t;\nconst esp_app_desc_t *esp_app_get_description(void);\n')
    with (tmp/"esp_err.h").open("a") as f:f.write('const char *esp_err_to_name(esp_err_t);\n')
    for source in ["ui_calibration.c","ui_calibration_layout.c","ui_devices.c","ui_devices_catalog_view.c","ui_bed_mesh.c","ui_printer_profiles.c","ui_macros.c","ui_settings.c","ui_printer_layout.c","ui_printer_actions.c","ui_printer_info_cards.c","ui_printer_live_status.c","ui_drybox_page.c","ui_settings_components.c","ui_console.c","ui_files.c"]:
        subprocess.run(["cc","-std=c11","-D_POSIX_C_SOURCE=200809L","-Wall","-Wextra","-Werror","-fsyntax-only","-include",str(tmp/"host_compat.h"),*flags,"-I",str(tmp),"-I",str(lvgl),"-I",str(lvgl/"src"),"-I",str(root/"main"),str(root/"main"/source)],check=True)
        print("PASS:",source)
