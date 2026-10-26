#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static int fail_install,fail_fat_install,short_write,fail_close,fat_rename;
static int rename_fixture(const char *a,const char *b){
    if(fail_install&&strstr(a,".tmp")){fail_install=0;errno=EIO;return -1;}
    if(fat_rename&&access(b,F_OK)==0){errno=EEXIST;return -1;}
    if(fail_fat_install&&strstr(a,".tmp")){fail_fat_install=0;errno=EIO;return -1;}
    return rename(a,b);
}
static size_t write_fixture(const void *p,size_t a,size_t b,FILE *f){return fwrite(p,a,short_write?b/2:b,f);}
static int close_fixture(FILE *f){int r=fclose(f);return fail_close?EOF:r;}
#define rename rename_fixture
#define fwrite write_fixture
#define fclose close_fixture
#include "files_row_rgb_cache.c"
#undef rename
#undef fwrite
#undef fclose
int main(void){
    uint64_t key=files_row_rgb_key("printer.local",7125,"folder/job.gcode",10000,12345);
    assert(key&&key==files_row_rgb_key("printer.local",7125,"folder/job.gcode",10000,12345));
    assert(key!=files_row_rgb_key("other.local",7125,"folder/job.gcode",10000,12345));
    assert(key!=files_row_rgb_key("printer.local",7126,"folder/job.gcode",10000,12345));
    assert(key!=files_row_rgb_key("printer.local",7125,"other/job.gcode",10000,12345));
    assert(key!=files_row_rgb_key("printer.local",7125,"folder/job.gcode",10001,12345));
    assert(key!=files_row_rgb_key("printer.local",7125,"folder/job.gcode",10000,12346));
    assert(!files_row_rgb_key("printer.local",7125,"job",100,0)&&!files_row_rgb_key("printer.local",7125,"job",0,100));
    assert(!files_row_rgb_key("",7125,"job",100,100)&&!files_row_rgb_key("printer.local",0,"job",100,100));
    assert(!files_row_rgb_key("printer.local",7125,"job",INFINITY,100)&&!files_row_rgb_key("printer.local",7125,"job",100,NAN));
    enum{COUNT=96*96};uint16_t pixels[COUNT],readback[COUNT+2];for(unsigned i=0;i<COUNT;i++)pixels[i]=(uint16_t)(i*31);
    readback[0]=0x1234;readback[COUNT+1]=0xabcd;int w=1,h=1;
    assert(!files_row_rgb_load(key,readback+1,COUNT,&w,&h)&&!w&&!h);
    assert(files_row_rgb_store(key,pixels,96,55));assert(files_row_rgb_load(key,readback+1,COUNT,&w,&h)&&w==96&&h==55&&!memcmp(pixels,readback+1,96*55*2));
    assert(!files_row_rgb_load(key,readback+1,1,&w,&h)&&!w&&!h);
    uint64_t collision=key+FILES_ROW_RGB_BUCKETS;
    assert(!files_row_rgb_load(collision,readback+1,COUNT,&w,&h));
    assert(files_row_rgb_store(collision,pixels,55,96));assert(!files_row_rgb_load(key,readback+1,COUNT,&w,&h));
    assert(files_row_rgb_load(collision,readback+1,COUNT,&w,&h)&&w==55&&h==96);
    assert(files_row_rgb_store(key,pixels,96,55));char path[256];assert(cache_path(key,path,sizeof(path)));
    FILE *f=fopen(path,"rb");assert(f);unsigned char original[40+96*55*2];assert(fread(original,1,sizeof(original),f)==sizeof(original));fclose(f);
    /* Corruption of header, key, dimensions, length, payload and trailing data
     * must all miss safely and leave output descriptors unusable. */
    unsigned mutations[]={0,8,12,16,20,24,28,32,36,40,50,sizeof(original)-1};
    for(unsigned i=0;i<sizeof(mutations)/sizeof(*mutations);i++){
        unsigned char damaged[sizeof(original)];memcpy(damaged,original,sizeof(damaged));damaged[mutations[i]]^=0x80;
        f=fopen(path,"wb");assert(f);assert(fwrite(damaged,1,sizeof(damaged),f)==sizeof(damaged));fclose(f);
        assert(!files_row_rgb_load(key,readback+1,COUNT,&w,&h)&&!w&&!h);
    }
    for(size_t length=0;length<sizeof(original);length+=317){
        f=fopen(path,"wb");assert(f);assert(fwrite(original,1,length,f)==length);fclose(f);
        assert(!files_row_rgb_load(key,readback+1,COUNT,&w,&h)&&!w&&!h);
    }
    f=fopen(path,"wb");assert(f);assert(fwrite(original,1,sizeof(original),f)==sizeof(original));fputc(42,f);fclose(f);
    assert(!files_row_rgb_load(key,readback+1,COUNT,&w,&h));
    /* Valid checksums cannot make an oversized image publishable. */
    unsigned char oversized[40+97*2];memcpy(oversized,original,sizeof(oversized));
    put32(oversized+16,97);put32(oversized+20,1);put32(oversized+24,97*2);put32(oversized+28,checksum(oversized+40,97*2));put32(oversized+32,checksum(oversized,32));
    f=fopen(path,"wb");assert(f);assert(fwrite(oversized,1,sizeof(oversized),f)==sizeof(oversized));fclose(f);
    assert(!files_row_rgb_load(key,readback+1,COUNT,&w,&h));
    assert(files_row_rgb_store(key,pixels,96,55));
    short_write=1;assert(!files_row_rgb_store(key,pixels,55,96));short_write=0;
    assert(files_row_rgb_load(key,readback+1,COUNT,&w,&h)&&w==96&&h==55);
    fail_close=1;assert(!files_row_rgb_store(key,pixels,55,96));fail_close=0;
    assert(files_row_rgb_load(key,readback+1,COUNT,&w,&h)&&w==96&&h==55);
    fail_install=1;assert(!files_row_rgb_store(key,pixels,55,96));assert(files_row_rgb_load(key,readback+1,COUNT,&w,&h)&&w==96&&h==55);
    fat_rename=1;fail_fat_install=1;assert(!files_row_rgb_store(key,pixels,55,96));assert(files_row_rgb_load(key,readback+1,COUNT,&w,&h)&&w==96&&h==55);
    assert(files_row_rgb_store(key,pixels,55,96));fat_rename=0;
    assert(files_row_rgb_load(key,readback+1,COUNT,&w,&h)&&w==55&&h==96);
    assert(!files_row_rgb_store(0,pixels,96,55)&&!files_row_rgb_store(key,pixels,97,55));
    assert(readback[0]==0x1234&&readback[COUNT+1]==0xabcd);
    puts("PASS: decoded RGB revision/endpoint identity, fixed-bucket collision misses, packed portrait/wide output, checksums, truncation/trailing guards, bounded reads and atomic write/close/rename failure preservation");return 0;
}
