#include "gcode_surface.h"
#include "gcode_cache.h"
#include <assert.h>
#include <float.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
static gcode_toolpath_t model;
static gcode_parser_t parser;
static gcode_segment_t samples[32];
static struct gcode_surface surface;
static struct gcode_cache cache;
static gcode_panel_t panels[4096];
static uint32_t slots[GCODE_SURFACE_SLOTS];
static void capture(void *context,const gcode_segment_t *s){(void)context;gcode_surface_feed(&surface,s);gcode_cache_feed(&cache,s);}
static void feed(const char *s){assert(gcode_parser_feed(&parser,s,strlen(s)));}
static void begin(unsigned session){gcode_parser_init(&parser,&model,samples,32);gcode_surface_init(&surface,panels,4096,slots);model.surface=&surface;assert(gcode_cache_begin(&cache,session));parser.segment_sink=capture;}
static void finish(void){gcode_parser_finish(&parser);gcode_surface_finish(&surface);assert(gcode_cache_finish(&cache));}
static unsigned colored(const uint16_t *p,unsigned n){unsigned result=0;for(unsigned i=0;i<n;i++)if(p[i]!=0x0863)result++;return result;}
int main(void){
 begin(1);feed("G90\nM83\nG1 X0 Y0 Z.2\n");
 /* 400,000 individual extrusions -> four wall panels. All 400 source paths
  * for a late layer remain on SD despite a 32-path RAM sample buffer. */
 for(unsigned layer=0;layer<1000;layer++){
  char text[100];snprintf(text,sizeof(text),"G0 X0 Y0 Z%.2f\n",.2+layer*.2);feed(text);
  for(unsigned side=0;side<4;side++)for(unsigned step=1;step<=100;step++){
   unsigned x=side==0?step:side==1?100:side==2?100-step:0;
   unsigned y=side==0?0:side==1?step:side==2?100:100-step;
   snprintf(text,sizeof(text),"G1 X%u Y%u E.1\n",x,y);feed(text);
  }
 }
 finish();assert(gcode_surface_usable(&surface)&&surface.count==4&&surface.inputs==400000&&surface.merged==399996);
 assert(cache.complete&&cache.count==model.input_segments&&model.sample_stride>1&&cache.layers==1000);
 uint32_t count;FILE *f=gcode_cache_open_layer(&cache,900,&count);assert(f&&count==401);unsigned extrusions=0;gcode_segment_t segment;
 for(uint32_t i=0;i<count;i++){assert(gcode_cache_read(f,cache.start[900]+i,900,&segment));assert(segment.layer==900);if(segment.extrusion)extrusions++;}
 fclose(f);assert(extrusions==400&&segment.b.z>180);
 enum{W=192,H=144,N=W*H};static uint16_t image[N+2],reference[N];static float depth[N+2];
 image[0]=0x1234;image[N+1]=0xabcd;depth[0]=123456;depth[N+1]=654321;
 gcode_view_t view;gcode_view_reset(&view);assert(!gcode_surface_render_step(&model,&view,image+1,depth+1,W,H,0,4096));assert(colored(image+1,N)>1000);memcpy(reference,image+1,sizeof(reference));
 size_t cursor=0;unsigned steps=0;do{cursor=gcode_surface_render_step(&model,&view,image+1,depth+1,W,H,cursor,1);steps++;}while(cursor);assert(steps==4&&!memcmp(reference,image+1,sizeof(reference)));
 assert(image[0]==0x1234&&image[N+1]==0xabcd&&depth[0]==123456&&depth[N+1]==654321);
 view.yaw=0;view.pitch=1.5707963f;gcode_surface_render_step(&model,&view,image+1,depth+1,W,H,0,4096);assert(image[1+(H/2)*W+W/2]==0x0863);gcode_view_reset(&view);
 view.layer=400;view.cumulative=true;gcode_surface_render_step(&model,&view,image+1,depth+1,W,H,0,4096);assert(memcmp(reference,image+1,sizeof(reference)));
 /* Per-record checksum detects card corruption before coordinates reach UI. */
 f=fopen(cache.file,"r+b");assert(f);assert(!fseek(f,16+(long)cache.start[900]*GCODE_CACHE_RECORD_BYTES,SEEK_SET));int byte=fgetc(f);assert(byte!=EOF);assert(!fseek(f,-1,SEEK_CUR));fputc(byte^1,f);fclose(f);
 f=gcode_cache_open_layer(&cache,900,&count);assert(f&&!gcode_cache_read(f,cache.start[900],900,&segment));fclose(f);
 char filename[128];strcpy(filename,cache.file);gcode_cache_destroy(&cache);assert(!fopen(filename,"rb"));
 /* Pending transition travels belong to the approached layer, even when they
  * precede its first extrusion; trailing travel remains on the last layer. */
 begin(2);feed("M83\nG1 X0 Y0 Z.2\nG1 X1 E1\nG0 X2\nG0 X3 Z.4\nG1 X4 E1\nG0 X5\n");finish();
 f=gcode_cache_open_layer(&cache,0,&count);assert(f&&count==1);fclose(f);
 f=gcode_cache_open_layer(&cache,1,&count);assert(f&&count==4);for(uint32_t i=0;i<count;i++)assert(gcode_cache_read(f,cache.start[1]+i,1,&segment)&&segment.layer==1);fclose(f);gcode_cache_destroy(&cache);
 /* Capacity exhaustion never silently publishes an incomplete surface. */
 gcode_surface_init(&surface,panels,1,slots);gcode_segment_t s={.a={0,0,.2f},.b={1,0,.2f},.extrusion=true};gcode_surface_feed(&surface,&s);s.a.y=s.b.y=2;gcode_surface_feed(&surface,&s);gcode_surface_finish(&surface);assert(surface.overflow&&!gcode_surface_usable(&surface));
 gcode_surface_init(&surface,panels,4096,slots);s.b.z=.4f;gcode_surface_feed(&surface,&s);gcode_surface_finish(&surface);assert(surface.nonplanar&&!gcode_surface_usable(&surface));
 begin(3);cache.count=(GCODE_CACHE_BYTES_MAX-16)/GCODE_CACHE_RECORD_BYTES;s.sequence=cache.count;gcode_cache_feed(&cache,&s);assert(!cache.good&&!gcode_cache_finish(&cache));gcode_cache_destroy(&cache);
 puts("PASS: 400,000 extrusions merged to 4 panels; 1,000 indexed SD layers; exact late paths and travel ownership; incremental surface raster/clipping/guards; corruption detection, cleanup, SD cap and explicit surface fallbacks");
}
