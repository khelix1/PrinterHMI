#!/usr/bin/env python3
"""Round-trip production macro formatting through Klipper-style shlex parsing."""
import argparse
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
root=Path(__file__).resolve().parents[2]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--source',type=Path,default=root/'main/macro_parameter_utils.c')
args=parser.parse_args()
fixture=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "macro_parameter_utils.h"
int main(void){
 const char *samples[]={"part#2","#ffee00","hello world","plain","file#2 path"};
 for(unsigned i=0;i<sizeof(samples)/sizeof(samples[0]);i++){
  macro_parameter_value_t values[]={{"NOTE",samples[i]},{"TARGET","210"}};
  char command[MACRO_COMMAND_MAX];const char *error=NULL;
  assert(macro_parameter_build_command("TEST_MACRO",values,2,command,sizeof(command),&error)&&!error);
  puts(command);
  size_t length=strlen(command);
  for(size_t size=1;size<=length;size++){
   char short_command[MACRO_COMMAND_MAX];memset(short_command,'x',sizeof(short_command));
   assert(!macro_parameter_build_command("TEST_MACRO",values,2,short_command,size,&error));
   assert(!short_command[0]&&error);
  }
  assert(macro_parameter_build_command("TEST_MACRO",values,2,command,length+1,&error));
 }
 const char *invalid[]={"quote\"","semicolon;","apostrophe'","backslash\\","line\nbreak"};
 for(unsigned i=0;i<sizeof(invalid)/sizeof(invalid[0]);i++){
  macro_parameter_value_t value={"NOTE",invalid[i]};char command[128];const char *error=NULL;
  assert(!macro_parameter_build_command("TEST_MACRO",&value,1,command,sizeof(command),&error)&&!command[0]&&error);
 }
 macro_parameter_value_t duplicate[]={{"NOTE","x"},{"note","y"}};
 char command[128];const char *error=NULL;
 assert(!macro_parameter_build_command("TEST_MACRO",duplicate,2,command,sizeof(command),&error)&&!command[0]);
}
'''
with tempfile.TemporaryDirectory(prefix='macro-roundtrip-') as directory:
    tmp=Path(directory);(tmp/'test.c').write_text(fixture)
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
                    '-fno-pie','-no-pie','-I',str(root/'main'),str(tmp/'test.c'),str(args.source),'-o',str(tmp/'test')],check=True)
    result=subprocess.run([str(tmp/'test')],check=True,text=True,capture_output=True,
                          env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
    samples=['part#2','#ffee00','hello world','plain','file#2 path']
    commands=result.stdout.splitlines();assert len(commands)==len(samples)
    for command,value in zip(commands,samples):
        lexer=shlex.shlex(command,posix=True);lexer.whitespace_split=True;lexer.commenters='#;'
        tokens=list(lexer)
        assert tokens==['TEST_MACRO','NOTE='+value,'TARGET=210'],(command,tokens)
    print('PASS: macro hash/space values survive comment-aware parsing with later parameters intact; unsafe values, duplicates and short buffers are rejected')
