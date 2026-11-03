#!/usr/bin/env python3
"""Exercise actual SD preview metadata readers with malformed documents."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / 'main/printer_preview_store.c').read_text()
def function(name):
    start = source.index('static bool ' + name + '(')
    return source[start:source.index('\n}\n', start) + 3]
production = function('read_metadata_line') + '\n' + function('read_preview_metadata')
fixture = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
'''
checks = r'''
static bool parse(const char *data,size_t size,int *port){
 FILE *f=tmpfile();assert(f);assert(fwrite(data,1,size,f)==size);rewind(f);
 char host[65],file[161];bool ok=read_preview_metadata(f,host,sizeof(host),port,file,sizeof(file));
 assert(!fclose(f));return ok;
}
static void invalid(const char *data){int port=123;assert(!parse(data,strlen(data),&port)&&port==123);}
int main(void){
 const char *valid="PRINTERHMI_PREVIEW_V1\nprinter.local\n7125\npart.gcode\n";int port=0;
 assert(parse(valid,strlen(valid),&port)&&port==7125);
 const char *crlf="PRINTERHMI_PREVIEW_V1\r\nprinter.local\r\n65535\r\npart.gcode\r\n";
 assert(parse(crlf,strlen(crlf),&port)&&port==65535);
 const char *ports[]={"0","65536","-1","+7125"," 7125","7125x","7125 ","","999999999999999999999999999999"};
 char data[512];
 for(unsigned i=0;i<sizeof(ports)/sizeof(ports[0]);i++){
  snprintf(data,sizeof(data),"PRINTERHMI_PREVIEW_V1\nprinter.local\n%s\npart.gcode\n",ports[i]);invalid(data);
 }
 invalid("PRINTERHMI_PREVIEW_V2\nprinter.local\n7125\npart.gcode\n");
 invalid("PRINTERHMI_PREVIEW_V1\nprinter.local\n7125\npart.gcode");
 invalid("PRINTERHMI_PREVIEW_V1\nprinter.local\n7125\npart.gcode\nextra\n");
 invalid("PRINTERHMI_PREVIEW_V1\n\n7125\npart.gcode\n");
 invalid("PRINTERHMI_PREVIEW_V1\nprinter.local\n7125\n\n");
 char binary[]="PRINTERHMI_PREVIEW_V1\nprinter.local\n7125\npart.gcode\0hidden\n";
 assert(!parse(binary,sizeof(binary)-1,&port));
 char host[65],file[161];memset(host,'h',63);host[63]=0;memset(file,'f',159);file[159]=0;
 snprintf(data,sizeof(data),"PRINTERHMI_PREVIEW_V1\n%s\n1\n%s\n",host,file);
 assert(parse(data,strlen(data),&port)&&port==1);
 host[63]='h';host[64]=0;snprintf(data,sizeof(data),"PRINTERHMI_PREVIEW_V1\n%s\n1\n%s\n",host,file);invalid(data);
 host[63]=0;file[159]='f';file[160]=0;snprintf(data,sizeof(data),"PRINTERHMI_PREVIEW_V1\n%s\n1\n%s\n",host,file);invalid(data);
 puts("PASS: preview metadata strict ports, full field widths, CRLF, truncation, embedded NUL and trailing-data rejection");
}
'''
with tempfile.TemporaryDirectory(prefix='preview-metadata-') as directory:
    p = Path(directory)
    (p / 'test.c').write_text(fixture + production + checks)
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-fno-pie', '-no-pie',
                    str(p / 'test.c'), '-o', str(p / 'test')], check=True)
    subprocess.run([str(p / 'test')], check=True,
                   env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0'})
