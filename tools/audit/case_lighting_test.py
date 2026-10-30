#!/usr/bin/env python3
"""Actual lighting model with cJSON and sanitizer-backed SDK lock fixtures."""
from pathlib import Path
import argparse,subprocess,tempfile
root=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--cjson-dir',type=Path,required=True);a=p.parse_args()
with tempfile.TemporaryDirectory(prefix='case-lighting-') as folder:
 t=Path(folder);(t/'freertos').mkdir();(t/'freertos/FreeRTOS.h').write_text('#pragma once\ntypedef int portMUX_TYPE;\n#define portMUX_INITIALIZER_UNLOCKED 0\n#define portENTER_CRITICAL(p) ((void)(p))\n#define portEXIT_CRITICAL(p) ((void)(p))\n')
 exe=t/'lighting'
 subprocess.run(['cc','-std=c11','-O1','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie','-I',str(t),'-I',str(root/'main'),'-I',str(a.cjson_dir),str(root/'tools/audit/case_lighting_test.c'),str(root/'main/case_lighting.c'),str(a.cjson_dir/'cJSON.c'),'-lm','-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)

 ws=(root/'main/moonraker_live_websocket.c').read_text()
 start=ws.index('bool moonraker_live_websocket_set_case_led(')
 (t/'lighting_transport.inc').write_text(ws[start:ws.index('\n}\n',start)+3])
 subprocess.run(['cc','-std=c11','-O1','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie','-I',str(t),'-I',str(root/'main'),'-I',str(a.cjson_dir),str(root/'tools/audit/case_lighting_transport_test.c'),str(root/'main/case_lighting.c'),str(a.cjson_dir/'cJSON.c'),'-lm','-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
