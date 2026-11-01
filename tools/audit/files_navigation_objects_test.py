#!/usr/bin/env python3
"""Actual live-file completion bridge, Dashboard route, and demo object publication."""
from pathlib import Path
import os,re,subprocess,tempfile
root=Path(__file__).resolve().parents[2]
main=(root/'main/main.c').read_text();moon=(root/'main/moonraker.c').read_text()
def function(text,name):
 match=re.search(r'^(?:static )?(?:void|bool) '+name+r'\([^;]*?\)\n\{.*?^\}',text,re.M|re.S)
 assert match,name
 return match[0]
with tempfile.TemporaryDirectory(prefix='files-nav-objects-') as folder:
 t=Path(folder);(t/'lvgl.h').write_text('#pragma once\n')
 (t/'live_start.inc').write_text(function(main,'printer_file_detail_start_bridge'))
 dashboard=re.search(r'    case UI_SHELL_PAGE_DASHBOARD:(.*?)\n    case UI_SHELL_PAGE_PRINTER:',main,re.S);assert dashboard
 (t/'dashboard_route.inc').write_text('void ui_shell_page_action(ui_shell_page_t page){switch(page){case UI_SHELL_PAGE_DASHBOARD:'+dashboard[1]+'default:assert(0);}}\n')
 (t/'demo_publish.inc').write_text('\n'.join(function(moon,name) for name in ('moonraker_state_publish_demo','moonraker_exclude_objects_available','moonraker_exclude_state_snapshot')))
 exe=t/'test'
 subprocess.run(['cc','-std=c11','-O1','-Wall','-Wextra','-Werror','-DPRINTERHMI_DEMO_RUNTIME=1','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie','-I',str(t),'-I',str(root/'main'),'-I',str(root/'tools/theme-maker/linux/native/shims'),str(root/'tools/audit/files_navigation_objects_test.c'),str(root/'main/printer_simulation.c'),'-lm','-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1'))
print('PASS: actual normal-firmware start success/failure paths, Dashboard selection, and locked demo object publication')
