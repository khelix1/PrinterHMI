#include "files_row_rgb_cache.h"
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#ifndef FILES_ROW_RGB_ROOT
#define FILES_ROW_RGB_ROOT "/sdcard/hmi/rows"
#endif
#define HEADER_BYTES 40u
static uint32_t checksum(const void *data,size_t size){
    const unsigned char *p=data;uint32_t hash=2166136261u;
    while(size--){hash^=*p++;hash*=16777619u;}return hash;
}
static void put32(unsigned char *out,uint32_t value){for(unsigned i=0;i<4;i++)out[i]=(unsigned char)(value>>(i*8));}
static uint32_t get32(const unsigned char *in){uint32_t value=0;for(unsigned i=0;i<4;i++)value|=(uint32_t)in[i]<<(i*8);return value;}
uint64_t files_row_rgb_key(const char *host,int port,const char *file,double size,double modified){
    if(!host||!host[0]||!file||!file[0]||port<1||port>65535||!isfinite(size)||!isfinite(modified)||size<=0||modified<=0)return 0;
    char identity[512];int n=snprintf(identity,sizeof(identity),"%s\n%d\n%s\n%.17g\n%.17g",host,port,file,size,modified);
    if(n<1||(size_t)n>=sizeof(identity))return 0;
    uint64_t hash=UINT64_C(14695981039346656037);
    for(int i=0;i<n;i++){hash^=(unsigned char)identity[i];hash*=UINT64_C(1099511628211);}return hash?hash:1;
}
static bool cache_path(uint64_t key,char *path,size_t capacity){
    int n=snprintf(path,capacity,FILES_ROW_RGB_ROOT "/%02x.rgb",(unsigned)(key%FILES_ROW_RGB_BUCKETS));
    return key&&n>0&&(size_t)n<capacity;
}
bool files_row_rgb_load(uint64_t key,uint16_t *pixels,size_t capacity,int *width,int *height){
    if(width)*width=0;
    if(height)*height=0;
    char path[256];if(!pixels||!width||!height||!cache_path(key,path,sizeof(path)))return false;
    FILE *f=fopen(path,"rb");if(!f)return false;
    unsigned char header[HEADER_BYTES];bool valid=fread(header,1,sizeof(header),f)==sizeof(header)&&!memcmp(header,"PHROW001",8);
    unsigned w=valid?get32(header+16):0,h=valid?get32(header+20):0;
    uint64_t stored=valid?((uint64_t)get32(header+12)<<32)|get32(header+8):0;
    size_t count=(size_t)w*h;
    valid=valid&&stored==key&&w&&h&&w<=FILES_ROW_RGB_EDGE&&h<=FILES_ROW_RGB_EDGE&&count<=capacity&&
        get32(header+24)==count*sizeof(*pixels)&&get32(header+32)==checksum(header,32)&&get32(header+36)==0;
    if(valid)valid=fread(pixels,sizeof(*pixels),count,f)==count&&getc(f)==EOF&&!ferror(f)&&get32(header+28)==checksum(pixels,count*sizeof(*pixels));
    fclose(f);if(!valid)return false;
    *width=(int)w;*height=(int)h;return true;
}
static bool replace_file(const char *temporary,const char *path){
    if(rename(temporary,path)==0)return true;
    if(errno!=EEXIST)return false;
    char backup[288];int n=snprintf(backup,sizeof(backup),"%s.bak",path);
    if(n<1||(size_t)n>=sizeof(backup))return false;
    if(remove(backup)!=0&&errno!=ENOENT)return false;
    if(rename(path,backup)!=0)return false;
    if(rename(temporary,path)!=0){(void)rename(backup,path);return false;}
    (void)remove(backup);return true;
}
bool files_row_rgb_store(uint64_t key,const uint16_t *pixels,int width,int height){
    char path[256],temporary[288];
    if(!pixels||width<1||height<1||width>(int)FILES_ROW_RGB_EDGE||height>(int)FILES_ROW_RGB_EDGE||!cache_path(key,path,sizeof(path)))return false;
    int n=snprintf(temporary,sizeof(temporary),"%s.tmp",path);if(n<1||(size_t)n>=sizeof(temporary))return false;
    char parent[256];snprintf(parent,sizeof(parent),"%s",FILES_ROW_RGB_ROOT);char *slash=strrchr(parent,'/');
    if(slash&&slash!=parent){*slash=0;(void)mkdir(parent,0775);}
    (void)mkdir(FILES_ROW_RGB_ROOT,0775);
    size_t bytes=(size_t)width*height*sizeof(*pixels);unsigned char header[HEADER_BYTES]={0};memcpy(header,"PHROW001",8);
    put32(header+8,(uint32_t)key);put32(header+12,(uint32_t)(key>>32));put32(header+16,(uint32_t)width);put32(header+20,(uint32_t)height);
    put32(header+24,(uint32_t)bytes);put32(header+28,checksum(pixels,bytes));put32(header+32,checksum(header,32));
    FILE *f=fopen(temporary,"wb");if(!f)return false;
    bool valid=fwrite(header,1,sizeof(header),f)==sizeof(header)&&fwrite(pixels,1,bytes,f)==bytes;
    if(fclose(f)!=0)valid=false;
    if(valid)valid=replace_file(temporary,path);
    if(!valid)(void)remove(temporary);
    return valid;
}
