#!/usr/bin/env python3
"""Build real LVGL host checks for native calibration dialogs, command flow and pinned actions in every theme. Supply --lvgl-dir if unmanaged."""
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
    (tmp/"esp_heap_caps.h").write_text("#pragma once\n#include <stdlib.h>\n#include <stdbool.h>\n#define MALLOC_CAP_SPIRAM 1\n#define MALLOC_CAP_8BIT 2\n#define MALLOC_CAP_INTERNAL 4\n#define heap_caps_calloc(n,size,caps) calloc(n,size)\n#define heap_caps_free(p) free(p)\nstatic inline bool heap_caps_check_integrity(unsigned c,bool p){(void)c;(void)p;return true;}\n")
    (tmp/"esp_err.h").write_text("#pragma once\ntypedef int esp_err_t;\n#define ESP_OK 0\n#define ESP_FAIL -1\n")
    (tmp/"esp_http_client.h").write_text("#pragma once\ntypedef void *esp_http_client_handle_t;\ntypedef struct { int event_id; } esp_http_client_event_t;\n")
    (tmp/"esp_log.h").write_text("#pragma once\n#include <stdio.h>\n#define ESP_LOGI(t,...) do { (void)(t); if(0) printf(__VA_ARGS__); } while(0)\n#define ESP_LOGW ESP_LOGI\n#define ESP_LOGE ESP_LOGI\n")
    (tmp/"esp_timer.h").write_text("#pragma once\n#include <stdint.h>\nint64_t esp_timer_get_time(void);\n")
    (tmp/"freertos").mkdir()
    (tmp/"freertos/FreeRTOS.h").write_text("#pragma once\n#include <stdint.h>\n#define pdMS_TO_TICKS(ms) (ms)\n")
    (tmp/"freertos/task.h").write_text("#pragma once\n#include <stdint.h>\nvoid vTaskDelay(uint32_t);\n")
    (tmp/"esp_app_desc.h").write_text("#pragma once\ntypedef struct { char version[32],date[16],time[16]; } esp_app_desc_t;\nconst esp_app_desc_t *esp_app_get_description(void);\n")
    (tmp/"esp_ota_ops.h").write_text("#pragma once\ntypedef struct { char label[32]; } esp_partition_t;\nconst esp_partition_t *esp_ota_get_running_partition(void);\n")
    sources=["macro_parameter_utils.c","ui_popup.c","ui_button.c","ui_theme.c","ui_theme_a.c","ui_theme_b.c","ui_theme_c.c","ui_theme_studio.c","ui_studio_icons.c","assets/fonts/studio/inter_18.c","assets/fonts/studio/inter_20.c","assets/fonts/studio/inter_24.c","assets/fonts/studio/inter_28.c","assets/fonts/studio/inter_32.c","assets/fonts/studio/inter_48.c","assets/fonts/studio/inter_64.c","assets/fonts/studio/inter_96.c","ui_font_fallback.c","ui_text.c","ui_widgets.c","ui_page_layout_profile.c"]
    common=["cc","-std=c11","-D_POSIX_C_SOURCE=200809L","-O2","-Wall","-Wextra","-Werror","-Wrestrict","-ffunction-sections","-fdata-sections",*flags,"-I",str(tmp),"-I",str(lvgl),"-I",str(lvgl/"src"),"-I",str(root/"main")]
    def compile_ui(source):
        obj=tmp/(source.replace("/","_")+".o")
        subprocess.run([*common,"-c",str(root/"main"/source),"-o",str(obj)],check=True)
        return str(obj)
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
        objects=list(pool.map(compile_ui,sources))
    for variant in ["FEEDBACK","OTAFAILURE","PID","MANUAL","MOTION","PROBE","ENDSTOP","RESULTS","PA","CUSTOM","GEOMETRY"]:
        executable=tmp/variant.lower()
        subprocess.run([*common,"-DTEST_"+variant,'-DTEST_NAME="'+variant.lower()+'"',str(root/"tools/audit/calibration_dialog_layout_test.c"),*objects,str(library),"-Wl,--gc-sections","-lm","-o",str(executable)],check=True)
        subprocess.run([str(executable)],check=True)
    # Compile every changed calibration owner independently, including the
    # page's Probe/Z and Axis Twist confirmation constructors.
    for source in ["ui_calibration.c","ui_calibration_motion.c","ui_calibration_geometry.c","ui_calibration_pid.c","ui_calibration_pressure_advance.c","ui_calibration_manual_probe.c","ui_calibration_results.c","ui_calibration_custom.c","ui_probe_accuracy.c","ui_endstop_status.c","ui_motion_diagnostics.c"]:
        compile_ui(source)
    print("PASS: separate calibration translation units with warnings treated as errors")
