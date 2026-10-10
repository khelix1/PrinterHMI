#!/usr/bin/env python3
"""Build real LVGL host checks for Setup Center steps, inputs and lifecycle in every theme. Supply --lvgl-dir if unmanaged."""
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
    (tmp/"esp_heap_caps.h").write_text("#pragma once\n#include <stdlib.h>\n#define MALLOC_CAP_SPIRAM 1\n#define MALLOC_CAP_8BIT 2\n#define MALLOC_CAP_INTERNAL 4\n#define heap_caps_calloc(n,size,caps) calloc(n,size)\n#define heap_caps_malloc(size,caps) malloc(size)\n#define heap_caps_free(ptr) free(ptr)\n")
    (tmp/"esp_err.h").write_text("#pragma once\ntypedef int esp_err_t;\n#define ESP_OK 0\n#define ESP_FAIL -1\n")
    (tmp/"esp_http_client.h").write_text("#pragma once\ntypedef void *esp_http_client_handle_t;\ntypedef struct { int event_id; } esp_http_client_event_t;\n")
    (tmp/"esp_log.h").write_text("#pragma once\n#include <stdio.h>\n#define ESP_LOGI(t,...) do { (void)(t); if(0) printf(__VA_ARGS__); } while(0)\n#define ESP_LOGW ESP_LOGI\n#define ESP_LOGE ESP_LOGI\n")
    (tmp/"esp_wifi_types.h").write_text("#pragma once\n#include <stdint.h>\ntypedef struct { uint8_t ssid[33]; int8_t rssi; } wifi_ap_record_t;\n")
    (tmp/"esp_wifi.h").write_text("#pragma once\n#include \"esp_wifi_types.h\"\n#include \"esp_err.h\"\nesp_err_t esp_wifi_scan_start(void*,bool);\nesp_err_t esp_wifi_scan_get_ap_num(uint16_t*);\nesp_err_t esp_wifi_scan_get_ap_records(uint16_t*,wifi_ap_record_t*);\nesp_err_t esp_wifi_sta_get_ap_info(wifi_ap_record_t*);\n")
    (tmp/"esp_netif_ip_addr.h").write_text("#pragma once\n#include <stdint.h>\ntypedef struct { uint32_t addr; } esp_ip4_addr_t;\n")
    (tmp/"esp_netif.h").write_text("#pragma once\n#include \"esp_netif_ip_addr.h\"\n#include \"esp_err.h\"\ntypedef void esp_netif_t;\ntypedef struct { esp_ip4_addr_t ip; } esp_netif_ip_info_t;\nesp_netif_t *esp_netif_get_handle_from_ifkey(const char*);\nesp_err_t esp_netif_get_ip_info(esp_netif_t*,esp_netif_ip_info_t*);\n")
    (tmp/"freertos").mkdir()
    (tmp/"freertos/FreeRTOS.h").write_text("#pragma once\n#define pdPASS 1\n#define pdMS_TO_TICKS(n) (n)\n")
    (tmp/"freertos/task.h").write_text("#pragma once\nvoid vTaskDelay(unsigned);\nvoid vTaskDelete(void*);\nint xTaskCreate(void(*)(void*),const char*,unsigned,void*,unsigned,void*);\n")
    sources=["ui_popup.c","ui_button.c","ui_theme.c","ui_theme_a.c","ui_theme_b.c","ui_theme_c.c","ui_theme_studio.c","ui_studio_icons.c","assets/fonts/studio/inter_18.c","assets/fonts/studio/inter_20.c","assets/fonts/studio/inter_24.c","assets/fonts/studio/inter_28.c","assets/fonts/studio/inter_32.c","assets/fonts/studio/inter_48.c","assets/fonts/studio/inter_64.c","assets/fonts/studio/inter_96.c","ui_font_fallback.c","ui_text.c","ui_widgets.c","console_filter.c","macro_parameter_utils.c","ui_page_title.c","ui_page_layout_profile.c","ui_thumbnail.c","ui_status_banner.c"]
    for variant in ["SETUP"]:
        executable=tmp/variant.lower()
        subprocess.run(["cc","-std=c11","-D_POSIX_C_SOURCE=200809L","-O2","-Wall","-Wextra","-Werror","-Wrestrict","-ffunction-sections","-fdata-sections",*flags,"-DTEST_"+variant,
                        "-I",str(tmp),"-I",str(lvgl),"-I",str(lvgl/"src"),"-I",str(root/"main"),
                        str(root/"tools/audit/setup_center_layout_test.c"),str(lvgl/"src/font/lv_font_montserrat_12.c"),*[str(root/"main"/s) for s in sources],
                        str(library),"-Wl,--gc-sections","-lm","-o",str(executable)],check=True)
        subprocess.run([str(executable)],check=True)
