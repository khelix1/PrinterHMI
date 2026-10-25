#!/usr/bin/env python3
from pathlib import Path
import subprocess,tempfile,os
root=Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='gcode-core-') as d:
 exe=Path(d)/'test'
 subprocess.run(['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I',str(root/'main'),str(root/'main/gcode_toolpath.c'),str(root/'tools/audit/gcode_toolpath_test.c'),'-lm','-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True,env={**os.environ,"ASAN_OPTIONS":"detect_leaks=0"})

with tempfile.TemporaryDirectory(prefix='gcode-motion-') as d:
 exe=Path(d)/'test'
 subprocess.run(['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I',str(root/'main'),str(root/'main/gcode_toolpath.c'),str(root/'tools/audit/gcode_motion_test.c'),'-lm','-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True,env={**os.environ,"ASAN_OPTIONS":"detect_leaks=0"})

with tempfile.TemporaryDirectory(prefix='gcode-retention-') as d:
 exe=Path(d)/'test'
 subprocess.run(['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I',str(root/'main'),str(root/'main/gcode_toolpath.c'),str(root/'tools/audit/gcode_retention_test.c'),'-lm','-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True,env={**os.environ,"ASAN_OPTIONS":"detect_leaks=0"})

with tempfile.TemporaryDirectory(prefix='gcode-solid-') as d:
 exe=Path(d)/'test'
 subprocess.run(['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I',str(root/'main'),str(root/'main/gcode_toolpath.c'),str(root/'tools/audit/gcode_solid_test.c'),'-lm','-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True,env={**os.environ,"ASAN_OPTIONS":"detect_leaks=0"})
