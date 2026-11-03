#!/usr/bin/env python3
"""Fault-inject actual backup promotion against real host files."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / 'main/settings_backup.c').read_text()
start = source.index('static bool promote_backup_file(')
function = source[start:source.index('\n}\n', start) + 3]
preamble = r'''
#include <assert.h>
#include <stdbool.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static const char *current="backup", *temp="temporary", *old="previous";
static bool fail_promote, fail_restore, fail_rotate, fail_clear;
static int rename_fault(const char *from,const char *to) {
 if ((fail_promote&&!strcmp(from,temp)) ||
     (fail_restore&&!strcmp(from,old)) ||
     (fail_rotate&&!strcmp(from,current))) {errno=EIO;return -1;}
 return rename(from,to);
}
static int unlink_fault(const char *path) {
 if(fail_clear&&!strcmp(path,old)){errno=EIO;return -1;}
 return unlink(path);
}
static void set_status(char *out,size_t size,const char *text){snprintf(out,size,"%s",text);}
#define rename rename_fault
#define unlink unlink_fault
'''
checks = r'''
#undef rename
#undef unlink
static void put(const char *p,const char *data){FILE *f=fopen(p,"wb");assert(f);assert(fputs(data,f)>=0);assert(!fclose(f));}
static void expect(const char *p,const char *data){char buf[32]={0};FILE *f=fopen(p,"rb");assert(f);assert(fread(buf,1,sizeof(buf)-1,f)==strlen(data));assert(!fclose(f));assert(!strcmp(buf,data));}
static void reset(void){fail_promote=fail_restore=fail_rotate=fail_clear=false;unlink(current);unlink(temp);unlink(old);}
static bool save(char *status){return promote_backup_file(temp,current,old,status,256);}
int main(int argc,char **argv){
 assert(argc==2&&!chdir(argv[1]));char status[256];
 reset();put(temp,"new");assert(save(status));expect(current,"new");
 put(temp,"next");assert(save(status));expect(current,"next");assert(access(old,F_OK));
 reset();put(current,"good");put(temp,"new");fail_promote=true;
 assert(!save(status));expect(current,"good");assert(strstr(status,"was restored"));
 reset();put(current,"good");put(temp,"new");fail_promote=fail_restore=true;
 assert(!save(status));expect(old,"good");assert(strstr(status,"recovery failed"));assert(access(current,F_OK));
 /* A retry must not unlink the only good copy after rollback failed. */
 put(temp,"retry");assert(!save(status));expect(old,"good");assert(strstr(status,"recovery copy was preserved"));
 fail_restore=false;put(temp,"retry");assert(!save(status));expect(current,"good");
 fail_promote=false;put(temp,"final");assert(save(status));expect(current,"final");
 reset();put(old,"interrupted-good");put(temp,"new");fail_promote=true;
 assert(!save(status));expect(current,"interrupted-good");
 reset();put(temp,"new");fail_promote=true;assert(!save(status));assert(strstr(status,"no previous"));
 reset();put(current,"good");put(temp,"new");fail_rotate=true;
 assert(!save(status));expect(current,"good");
 reset();put(current,"good");put(old,"older");put(temp,"new");fail_clear=true;
 assert(!save(status));expect(current,"good");expect(old,"older");
 reset();assert(!mkdir(current,0700));put(temp,"new");
 /* An inaccessible/unusable destination must not claim a successful restore. */
 fail_promote=true;assert(!save(status));assert(!rmdir(current));
 reset();puts("PASS: backup promotion, rollback failure, interrupted recovery, repeated failure and retry preserve the known-good copy");
}
'''
with tempfile.TemporaryDirectory(prefix='backup-rotation-') as directory:
    p = Path(directory)
    (p / 'test.c').write_text(preamble + function + checks)
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-fno-pie', '-no-pie',
                    str(p / 'test.c'), '-o', str(p / 'test')], check=True)
    subprocess.run([str(p / 'test'), str(p)], check=True,
                   env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0'})
