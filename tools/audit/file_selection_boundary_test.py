#!/usr/bin/env python3
"""Exercise actual file-selection side effects at the session path boundary."""
from pathlib import Path
import argparse
import os
import subprocess
import tempfile
root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--source", type=Path, default=root/"main/printer_file_controller.c")
args = parser.parse_args()
headers = {
    "esp_err.h": "#pragma once\ntypedef int esp_err_t;\n#define ESP_FAIL -1\nconst char *esp_err_to_name(int);\n",
    "esp_log.h": '#pragma once\n#include <stdio.h>\n#define ESP_LOGI(tag,...) do{(void)(tag);if(0)printf(__VA_ARGS__);}while(0)\n#define ESP_LOGW ESP_LOGI\n',
    "moonraker.h": '#pragma once\n#include <stdbool.h>\nbool moonraker_start_print_file(const char*,int,const char*,const char*,int*,int*);\n',
    "thumbnail_manager.h": '#pragma once\n#include <stdbool.h>\nvoid thumbnail_manager_set_force_refresh(bool);\n',
    "thumbnail_session.h": '#pragma once\n#include <stddef.h>\nchar *thumbnail_session_selected_file(void);\nsize_t thumbnail_session_selected_file_size(void);\nvoid thumbnail_session_save_last_selected_file(void);\n',
    "ui_dashboard.h": '#pragma once\nchar *ui_dashboard_thumb_canvas_file(void);\n',
    "ui_printer.h": '#pragma once\nvoid ui_printer_preview_reset(void);\n',
}
fixture = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "printer_file_controller.h"
static char selected[160]="previous.gcode",canvas[160]="previous.gcode";
static unsigned refreshed,reset,saved,opened;
char *thumbnail_session_selected_file(void){return selected;}
size_t thumbnail_session_selected_file_size(void){return sizeof(selected);}
void thumbnail_manager_set_force_refresh(bool force){assert(force);refreshed++;}
char *ui_dashboard_thumb_canvas_file(void){return canvas;}
void ui_printer_preview_reset(void){reset++;}
void thumbnail_session_save_last_selected_file(void){saved++;}
static void popup(void){opened++;}
const char *esp_err_to_name(int e){(void)e;return "fixture";}
bool moonraker_start_print_file(const char*h,int p,const char*k,const char*f,int*c,int*e){(void)h;(void)p;(void)k;(void)f;(void)c;(void)e;return false;}
int main(void){
 char path[161];memset(path,'a',sizeof(path)-1);path[160]=0;
 assert(!printer_file_controller_select_file(NULL,popup));
 assert(!printer_file_controller_select_file("",popup));
 assert(!printer_file_controller_select_file(path,popup));
 assert(!strcmp(selected,"previous.gcode")&&!strcmp(canvas,"previous.gcode"));
 assert(!refreshed&&!reset&&!saved&&!opened);
 path[159]=0;
 assert(printer_file_controller_select_file(path,popup));
 assert(!strcmp(selected,path)&&!canvas[0]&&refreshed==1&&reset==1&&saved==1&&opened==1);
 /* Selecting the existing session buffer is supported without overlapping copy UB. */
 assert(printer_file_controller_select_file(selected,popup));
 assert(!strcmp(selected,path)&&refreshed==2&&reset==2&&saved==2&&opened==2);
 assert(printer_file_controller_select_file("folder/exact.gcode",NULL));
 assert(!strcmp(selected,"folder/exact.gcode")&&saved==3&&opened==2);
 puts("PASS: overlong/empty requests preserve selection, previews and persistence; exact-limit, nested and aliased paths select intact");
}
'''
with tempfile.TemporaryDirectory(prefix="file-selection-") as directory:
    tmp = Path(directory)
    for name, content in headers.items():
        (tmp/name).write_text(content)
    (tmp/"controller.c").write_bytes(args.source.read_bytes())
    (tmp/"test.c").write_text(fixture)
    exe = tmp/"test"
    subprocess.run(["cc", "-std=c11", "-D_POSIX_C_SOURCE=200809L", "-O1", "-g",
                    "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined",
                    "-fno-pie", "-no-pie", "-I", str(tmp), "-I", str(root/"main"),
                    str(tmp/"test.c"), str(tmp/"controller.c"), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True,
                   env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0"})
