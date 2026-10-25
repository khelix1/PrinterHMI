#!/usr/bin/env python3
from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='gcode-surface-cache-') as d:
 exe=Path(d)/'test'
 subprocess.run(['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer',f'-DGCODE_CACHE_DIRECTORY="{d}/cache"','-I',str(root/'main'),*[str(root/'main'/s) for s in ['gcode_toolpath.c','gcode_surface.c','gcode_cache.c']],str(root/'tools/audit/gcode_surface_cache_test.c'),'-lm','-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
