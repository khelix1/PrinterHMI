#!/usr/bin/env python3
from pathlib import Path
import importlib.util,struct,subprocess,tempfile,os,hashlib
root=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('mesh_converter',root/'tools/mesh-preview/convert_stl.py');mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
blob,info=mod.encode(mod.demo());assert blob==(root/'tools/mesh-preview/demo.phm').read_bytes() and info['cell_mm']==0 and info['triangles']==6144
small,small_info=mod.encode(mod.demo(),1000);assert small_info['triangles']<=1000 and small_info['cell_mm']>0
try:mod.encode(mod.demo(),1000,False)
except ValueError:pass
else:raise AssertionError('ignored --no-simplify budget')
with tempfile.TemporaryDirectory(prefix='mesh-preview-') as d:
 d=Path(d)
 vertices=(0.,0.,0., 10.,0.,0., 0.,10.,0.)
 binary=d/'solid-binary.stl';binary.write_bytes(b'solid binary header'.ljust(80,b'\0')+struct.pack('<I12fH',1,0.,0.,1.,*vertices,0))
 before=hashlib.sha256(binary.read_bytes()).digest();assert list(mod.read_stl(binary))==list(vertices)
 ascii=d/'ascii.stl';ascii.write_text('solid demo\nfacet normal 0 0 1\nouter loop\nvertex 0 0 0\nvertex 10 0 0\nvertex 0 10 0\nendloop\nendfacet\nendsolid demo\n');assert list(mod.read_stl(ascii))==list(vertices)
 output=d/'example.phm';subprocess.run(['python3',str(root/'tools/mesh-preview/convert_stl.py'),str(binary),str(output)],check=True);assert hashlib.sha256(binary.read_bytes()).digest()==before
 for contents in [b'solid incomplete\nfacet normal 0 0 1\nouter loop\nvertex 0 0 0\n',binary.read_bytes()[:-1],b'solid x\n'+b'x'*2000]:
  bad=d/'bad.stl';bad.write_bytes(contents)
  try:mod.read_stl(bad)
  except (ValueError,UnicodeError,struct.error):pass
  else:raise AssertionError('accepted malformed STL')
 try:mod.write_output(output,b'bad')
 except ValueError:pass
 else:raise AssertionError('overwrote output without --force')
 exe=d/'test';subprocess.run(['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I',str(root/'main'),str(root/'main/mesh_preview.c'),str(root/'main/gcode_toolpath.c'),str(root/'tools/audit/mesh_preview_test.c'),'-lm','-o',str(exe)],check=True)
 subprocess.run([str(exe),str(root/'tools/mesh-preview/demo.phm')],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
print('PASS: standalone binary/ASCII STL conversion, binary solid-header detection, simplification budget, deterministic demo, malformed input and source/output preservation')
