#!/usr/bin/env python3
from pathlib import Path
import ast,os,subprocess,tempfile,shutil
root=Path(__file__).resolve().parents[2]
tree=ast.parse((root/'tools/audit/gcode_viewer_loader_test.py').read_text())
headers=next(ast.literal_eval(n.value) for n in tree.body if isinstance(n,ast.Assign) and n.targets[0].id=='headers')
with tempfile.TemporaryDirectory(prefix='mesh-loader-') as d:
 tmp=Path(d);sd=tmp/'meshes';sd.mkdir();shutil.copyfile(root/'tools/mesh-preview/demo.phm',sd/'demo.phm')
 for name,value in headers.items():
  p=tmp/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text(value)
 exe=tmp/'test';subprocess.run(['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',f'-DMESH_PREVIEW_DIRECTORY="{sd}"','-I',str(tmp),'-I',str(root/'main'),str(root/'main/mesh_preview.c'),str(root/'main/gcode_toolpath.c'),str(root/'main/mesh_preview_loader.c'),str(root/'tools/audit/mesh_preview_loader_test.c'),'-lm','-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
