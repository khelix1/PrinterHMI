#include "gcode_cache.h"
#include <dirent.h>
#include <errno.h>
#include <math.h>
#include <stddef.h>
#include <string.h>
#include <sys/stat.h>
#ifndef GCODE_CACHE_DIRECTORY
#define GCODE_CACHE_DIRECTORY "/sdcard/PrinterHMI/gcode-preview"
#define GCODE_CACHE_PARENT "/sdcard/PrinterHMI"
#endif
/* Fixed record encoding within one firmware session. Validate before reading
 * flags as bool or allowing any coordinate into a rasterizer. */
typedef struct {float xyz[6];float width,height;uint32_t sequence;uint16_t layer;uint8_t tool,flags;uint32_t checksum;} record_t;
typedef struct {char magic[8];uint32_t count,size;} header_t;
_Static_assert(sizeof(record_t)==44,"cache record layout");
static uint32_t checksum(const record_t *r){const unsigned char *p=(const unsigned char *)r;uint32_t h=2166136261u;for(size_t i=0;i<offsetof(record_t,checksum);i++){h^=p[i];h*=16777619u;}return h;}
static void failed(struct gcode_cache *c){if(c->writer){fclose(c->writer);c->writer=NULL;}c->good=false;c->complete=false;remove(c->file);}
bool gcode_cache_begin(struct gcode_cache *c,uint32_t session){
    memset(c,0,sizeof(*c));c->refs=1;
#ifdef GCODE_CACHE_PARENT
    if(mkdir(GCODE_CACHE_PARENT,0775)&&errno!=EEXIST)return false;
#endif
    if(mkdir(GCODE_CACHE_DIRECTORY,0775)&&errno!=EEXIST)return false;
    /* Only the first successful cache opening can have crash leftovers; later
     * openings may coexist with a referenced prior preview. */
    static bool cleaned;
    if(!cleaned){
        DIR *dir=opendir(GCODE_CACHE_DIRECTORY);if(!dir)return false;
        struct dirent *e;
        while((e=readdir(dir))){
            if(strlen(e->d_name)!=20||strncmp(e->d_name,"preview_",8)||strcmp(e->d_name+16,".bin"))continue;
            bool hex=true;for(unsigned i=8;i<16;i++)if(!((e->d_name[i]>='0'&&e->d_name[i]<='9')||(e->d_name[i]>='a'&&e->d_name[i]<='f')))hex=false;
            if(hex){char path[128];int n=snprintf(path,sizeof(path),"%s/%s",GCODE_CACHE_DIRECTORY,e->d_name);if(n>0&&(size_t)n<sizeof(path))remove(path);}
        }
        closedir(dir);cleaned=true;
    }
    int n=snprintf(c->file,sizeof(c->file),"%s/preview_%08lx.bin",GCODE_CACHE_DIRECTORY,(unsigned long)session);
    if(n<0||(size_t)n>=sizeof(c->file))return false;
    c->writer=fopen(c->file,"wb");if(!c->writer)return false;
    setvbuf(c->writer,c->buffer,_IOFBF,sizeof(c->buffer));
    header_t header={"PHGC002",0,sizeof(record_t)};
    if(fwrite(&header,sizeof(header),1,c->writer)!=1){failed(c);return false;}
    c->good=true;return true;
}
void gcode_cache_feed(struct gcode_cache *c,const gcode_segment_t *s){
    if(!c||!c->good||!c->writer)return;
    if(s->sequence!=c->count||(size_t)c->count>=(GCODE_CACHE_BYTES_MAX-sizeof(header_t))/sizeof(record_t)||s->layer>=GCODE_TOOLPATH_LAYERS_MAX){failed(c);return;}
    if(s->extrusion){
        if(!c->layers){c->layers=1;c->start[0]=0;}
        if(s->layer>=c->layers){
            if(s->layer!=c->layers){failed(c);return;}
            uint32_t boundary=c->extrusion_seen?c->last_extrusion+1:0;
            c->end[c->layers-1]=boundary;c->start[c->layers++]=boundary;
        }
        c->last_extrusion=c->count;c->extrusion_seen=true;
    }
    record_t r={{s->a.x,s->a.y,s->a.z,s->b.x,s->b.y,s->b.z},s->width>0?s->width:.45f,s->height>0?s->height:.2f,s->sequence,s->layer,s->tool,s->extrusion?1:0,0};r.checksum=checksum(&r);
    if(fwrite(&r,sizeof(r),1,c->writer)!=1){failed(c);return;}c->count++;
}
bool gcode_cache_finish(struct gcode_cache *c){
    if(!c||!c->good||!c->writer)return false;
    if(c->layers)c->end[c->layers-1]=c->count;
    header_t header={"PHGC002",c->count,sizeof(record_t)};
    if(fflush(c->writer)||fseek(c->writer,0,SEEK_SET)||fwrite(&header,sizeof(header),1,c->writer)!=1||fflush(c->writer)){failed(c);return false;}
    int result=fclose(c->writer);c->writer=NULL;if(result){failed(c);return false;}
    c->complete=true;return true;
}
void gcode_cache_destroy(struct gcode_cache *c){if(!c)return;if(c->writer)fclose(c->writer);c->writer=NULL;if(c->file[0])remove(c->file);c->complete=c->good=false;}
FILE *gcode_cache_open_layer(const struct gcode_cache *c,unsigned layer,uint32_t *count){
    if(!c||!c->complete||layer>=c->layers||c->end[layer]<c->start[layer]||c->end[layer]>c->count)return NULL;
    FILE *f=fopen(c->file,"rb");if(!f)return NULL;
    header_t h;
    if(fread(&h,sizeof(h),1,f)!=1||memcmp(h.magic,"PHGC002",8)||h.size!=sizeof(record_t)||h.count!=c->count||fseek(f,(long)(sizeof(header_t)+(size_t)c->start[layer]*sizeof(record_t)),SEEK_SET)){fclose(f);return NULL;}
    *count=c->end[layer]-c->start[layer];return f;
}
bool gcode_cache_read(FILE *f,uint32_t sequence,unsigned layer,gcode_segment_t *s){
    record_t r;if(fread(&r,sizeof(r),1,f)!=1||r.checksum!=checksum(&r)||r.sequence!=sequence||r.tool>=GCODE_TOOLPATH_TOOLS_MAX||r.flags>1)return false;
    for(unsigned i=0;i<6;i++)if(!isfinite(r.xyz[i])||fabsf(r.xyz[i])>1000000)return false;
    if(!isfinite(r.width)||!isfinite(r.height)||r.width<.05f||r.width>10||r.height<.01f||r.height>5)return false;
    *s=(gcode_segment_t){.a={r.xyz[0],r.xyz[1],r.xyz[2]},.b={r.xyz[3],r.xyz[4],r.xyz[5]},.sequence=r.sequence,.layer=layer,.tool=r.tool,.extrusion=r.flags==1,.width=r.width,.height=r.height};return true;
}
