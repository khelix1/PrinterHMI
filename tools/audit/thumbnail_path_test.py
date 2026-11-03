#!/usr/bin/env python3
"""Check root/folder thumbnail resolution before URL encoding."""
from pathlib import Path
import os
import subprocess
import tempfile
root = Path(__file__).resolve().parents[2]
fixture = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "thumbnail_path.h"
int main(void){
 char out[256];
 assert(thumbnail_path_resolve("part.gcode",".thumbs/part.png",out,sizeof(out))&&!strcmp(out,".thumbs/part.png"));
 assert(thumbnail_path_resolve("folder/sub/part.gcode",".thumbs/part.png",out,sizeof(out))&&!strcmp(out,"folder/sub/.thumbs/part.png"));
 assert(thumbnail_path_resolve("with space/é/part.gcode",".thumbs/part #%.png",out,sizeof(out))&&!strcmp(out,"with space/é/.thumbs/part #%.png"));
 strcpy(out,".thumbs/part.png");assert(thumbnail_path_resolve("folder/part.gcode",out,out,sizeof(out))&&!strcmp(out,"folder/.thumbs/part.png"));
 char exact[12];assert(thumbnail_path_resolve("f/a",".thumbs/x",exact,12)&&!strcmp(exact,"f/.thumbs/x"));
 assert(!thumbnail_path_resolve("f/a",".thumbs/x",exact,11)&&!exact[0]);
 assert(!thumbnail_path_resolve("f/a","../outside",out,sizeof(out))&&!out[0]);
 assert(!thumbnail_path_resolve("f/a",".thumbs/../../outside",out,sizeof(out))&&!out[0]);
 assert(!thumbnail_path_resolve("f/a","/absolute",out,sizeof(out))&&!out[0]);
 assert(!thumbnail_path_resolve(NULL,"x",out,sizeof(out))&&!out[0]);
 puts("PASS: root/nested/Unicode/escaped thumbnail paths, in-place resolution, exact capacity and unsafe/oversized rejection");
}
'''
with tempfile.TemporaryDirectory(prefix='thumbnail-path-') as directory:
    p=Path(directory);(p/'test.c').write_text(fixture)
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie','-I',str(root/'main'),str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
