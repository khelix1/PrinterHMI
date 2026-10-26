#!/usr/bin/env python3
"""Exercise decoded row-cache parsing, identity and storage failures with sanitizers."""
from pathlib import Path
import os
import subprocess
import tempfile
root=Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='row-rgb-cache-') as folder:
    directory=Path(folder)
    executable=directory/'test'
    subprocess.run(['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer',
        '-DFILES_ROW_RGB_ROOT="'+str(directory/'sd'/'rows')+'"','-I',str(root/'main'),
        str(root/'tools/audit/files_row_rgb_cache_test.c'),'-lm','-o',str(executable)],check=True)
    subprocess.run([str(executable)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
