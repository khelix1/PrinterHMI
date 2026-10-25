#!/usr/bin/env python3
from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='automatic-volume-') as d:
 exe=Path(d)/'test'
 subprocess.run(['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I',str(root/'main'),*[str(root/'main'/s) for s in ['gcode_toolpath.c','gcode_volume.c','mesh_preview.c']],str(root/'tools/audit/gcode_volume_test.c'),'-lm','-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
