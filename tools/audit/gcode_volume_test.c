#include "gcode_volume.h"
#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
static gcode_toolpath_t model,replay;
static gcode_parser_t parser,second;
static gcode_segment_t paths[8],one[1];
static gcode_volume_t *volume;
static bool cancelled;
static size_t allocations,attempts,fail_at;
static void *allocate(size_t n){if(++attempts==fail_at)return NULL;void *p=malloc(n);if(p)allocations++;return p;}
static void release(void *p){if(p){assert(allocations);allocations--;free(p);}}
static bool stop(void *p){(void)p;return cancelled;}
static void sink(void *p,const gcode_segment_t *s){assert(gcode_volume_feed(p,s));}
static void source(gcode_parser_t *p){
 const char *start="G90\nM83\n;WIDTH:0.6\n;HEIGHT:0.4\n";assert(gcode_parser_feed(p,start,strlen(start)));
 for(unsigned layer=0;layer<36;layer++){
  float z=3+.4f*layer,relative=z-10,r=sqrtf(fmaxf(0,64-relative*relative));
  for(float radius=25-r;radius<=25+r;radius+=.55f){
   char line[128];int n=snprintf(line,sizeof(line),"G0 X%.4f Y50 Z%.4f\n",50+radius,z);assert(gcode_parser_feed(p,line,n));
   for(unsigned i=1;i<=64;i++){double a=6.283185307179586*i/64;n=snprintf(line,sizeof(line),"G1 X%.4f Y%.4f E.2\n",50+radius*cos(a),50+radius*sin(a));assert(gcode_parser_feed(p,line,n));}
  }
 }
 gcode_parser_finish(p);
}
int main(void){
 gcode_parser_init(&parser,&model,paths,8);source(&parser);assert(model.sample_stride>1&&model.extrusion_seen&&model.dimension_tags==2&&model.max_width>.59f);
 volume=gcode_volume_create(&model,96,allocate,release,stop,NULL,NULL);assert(volume);gcode_parser_init(&second,&replay,one,1);second.segment_sink=sink;second.sink_context=volume;source(&second);
 mesh_preview_t *mesh=gcode_volume_finish(volume);assert(mesh&&mesh->cull_backfaces&&mesh->triangles&&mesh->triangles<=MESH_PREVIEW_TRIANGLES_MAX&&allocations==1&&mesh->cell_mm>0);
 for(unsigned i=0;i<mesh->vertices;i++){mesh_vertex_t p=mesh->vertex[i];assert(isfinite(p.x)&&isfinite(p.y)&&isfinite(p.z));float length=p.nx*p.nx+p.ny*p.ny+p.nz*p.nz;assert(isfinite(length)&&fabsf(length-1)<.001f);assert(p.x>=mesh->min[0]&&p.x<=mesh->max[0]&&p.y>=mesh->min[1]&&p.y<=mesh->max[1]&&p.z>=mesh->min[2]&&p.z<=mesh->max[2]);}
 for(unsigned i=0;i<mesh->triangles;i++){mesh_triangle_t t=mesh->triangle[i];assert(t.a<mesh->vertices&&t.b<mesh->vertices&&t.c<mesh->vertices);}
 enum{W=192,H=144,N=W*H};static uint16_t pixels[N+2],reference[N];static float depth[N+2];pixels[0]=0x1234;pixels[N+1]=0xabcd;depth[0]=1234;depth[N+1]=4321;gcode_view_t view;gcode_view_reset(&view);mesh_preview_render_step(mesh,&view,pixels+1,depth+1,W,H,0,SIZE_MAX);memcpy(reference,pixels+1,sizeof(reference));
 size_t cursor=0;do{cursor=mesh_preview_render_step(mesh,&view,pixels+1,depth+1,W,H,cursor,199);}while(cursor);assert(!memcmp(reference,pixels+1,sizeof(reference)));
 for(unsigned angle=0;angle<8;angle++){
  view.yaw=angle*.79f;view.pitch=angle%2?-.6f:.7f;mesh->cull_backfaces=false;mesh_preview_render_step(mesh,&view,pixels+1,depth+1,W,H,0,SIZE_MAX);memcpy(reference,pixels+1,sizeof(reference));mesh->cull_backfaces=true;mesh_preview_render_step(mesh,&view,pixels+1,depth+1,W,H,0,SIZE_MAX);
  unsigned mismatch=0;for(unsigned i=0;i<N;i++)if(reference[i]!=pixels[i+1])mismatch++;assert(mismatch<N/100); /* Shared-edge ties can differ; the exterior must remain closed. */
 }
 enum{CAP_WORDS=((W+31)/32)*H};static uint32_t cap[CAP_WORDS+2];cap[0]=0x12345678;cap[CAP_WORDS+1]=0x87654321;
 view.yaw=0;view.pitch=1.5707963f;float cut=(mesh->min[2]+mesh->max[2])*.5f;
 mesh_preview_render_section_step(mesh,&view,pixels+1,depth+1,W,H,0,SIZE_MAX,cut,cap+1);assert(pixels[1+H/2*W+W/2]==0x0863);assert(pixels[1+H/2*W+W/2+45]!=0x0863);assert((cap[1+(H/2)*((W+31)/32)+(W/2+45)/32]&(1u<<((W/2+45)%32)))!=0);memcpy(reference,pixels+1,sizeof(reference));cursor=0;do{cursor=mesh_preview_render_section_step(mesh,&view,pixels+1,depth+1,W,H,cursor,137,cut,cap+1);}while(cursor);assert(!memcmp(reference,pixels+1,sizeof(reference))&&cap[0]==0x12345678&&cap[CAP_WORDS+1]==0x87654321);
 /* Contact joining must fill only an inside boundary band, not the infill
  * interior or the hole. Exercise mask word shifts and incremental lifetime. */
 unsigned full_plane=0;for(unsigned i=0;i<N;i++)if(fabsf(depth[i+1])<.0001f)full_plane++;
 mesh_preview_render_section_step(mesh,&view,pixels+1,depth+1,W,H,0,SIZE_MAX,cut,NULL);memcpy(reference,pixels+1,sizeof(reference));
 mesh_preview_render_joined_section_step(mesh,&view,pixels+1,depth+1,W,H,0,SIZE_MAX,-FLT_MAX,cut,NULL);assert(!memcmp(reference,pixels+1,sizeof(reference)));
 mesh_preview_render_joined_section_step(mesh,&view,pixels+1,depth+1,W,H,0,SIZE_MAX,-FLT_MAX,cut,cap+1);unsigned joined_plane=0,changed_join=0;
 for(int y=0;y<H;y++)for(int x=0;x<W;x++){unsigned i=y*W+x;if(fabsf(depth[i+1])<.0001f)joined_plane++;if(pixels[i+1]!=reference[i]){changed_join++;assert(cap[1+y*((W+31)/32)+x/32]&(1u<<(x%32)));}}
 assert(joined_plane>0&&joined_plane<full_plane&&changed_join>0&&pixels[1+H/2*W+W/2]==0x0863);memcpy(reference,pixels+1,sizeof(reference));cursor=0;
 do{cursor=mesh_preview_render_joined_section_step(mesh,&view,pixels+1,depth+1,W,H,cursor,137,-FLT_MAX,cut,cap+1);}while(cursor);assert(!memcmp(reference,pixels+1,sizeof(reference))&&cap[0]==0x12345678&&cap[CAP_WORDS+1]==0x87654321);
 view.pan_x=.2f;view.pan_y=-.1f;mesh_preview_render_section_step(mesh,&view,pixels+1,depth+1,W,H,0,SIZE_MAX,cut,cap+1);int shifted_x=(int)lroundf(W*.7f),shifted_y=(int)lroundf(H*.4f);assert(pixels[1+shifted_y*W+shifted_x]==0x0863&&pixels[1+shifted_y*W+shifted_x+45]!=0x0863);view.pan_x=view.pan_y=0;
 view.pitch=0;mesh_preview_render_section_step(mesh,&view,pixels+1,depth+1,W,H,0,SIZE_MAX,cut,cap+1);memcpy(reference,pixels+1,sizeof(reference));mesh_preview_render_slab_step(mesh,&view,pixels+1,depth+1,W,H,0,SIZE_MAX,cut-2,cut,cap+1);assert(memcmp(reference,pixels+1,sizeof(reference)));memcpy(reference,pixels+1,sizeof(reference));cursor=0;do{cursor=mesh_preview_render_slab_step(mesh,&view,pixels+1,depth+1,W,H,cursor,137,cut-2,cut,cap+1);}while(cursor);assert(!memcmp(reference,pixels+1,sizeof(reference)));
 view.pitch=1.5707963f;mesh_preview_render_slab_step(mesh,&view,pixels+1,depth+1,W,H,0,SIZE_MAX,cut-.4f,cut,cap+1);assert(pixels[1+H/2*W+W/2]==0x0863&&pixels[1+H/2*W+W/2+45]!=0x0863);
 view.yaw=0;view.pitch=1.5707963f;mesh_preview_render_step(mesh,&view,pixels+1,depth+1,W,H,0,SIZE_MAX);assert(pixels[1+H/2*W+W/2]==0x0863);assert(pixels[0]==0x1234&&pixels[N+1]==0xabcd&&depth[0]==1234&&depth[N+1]==4321);
 printf("PASS: automatic ring from %lu source paths despite 8 retained samples -> %lu triangles at %.3f mm; hole/depth/raster guards, outward-face culling equivalence, section/slab caps/holes/stencil guards and complete ownership\n",(unsigned long)model.input_segments,(unsigned long)mesh->triangles,(double)mesh->cell_mm);release(mesh);assert(!allocations);
 volume=gcode_volume_create(&model,64,allocate,release,stop,NULL,NULL);assert(volume);cancelled=true;assert(!gcode_volume_finish(volume)&&!allocations);cancelled=false;
 for(size_t i=1;i<=3;i++){attempts=0;fail_at=i;assert(!gcode_volume_create(&model,64,allocate,release,stop,NULL,NULL)&&!allocations);}
 attempts=0;fail_at=4;volume=gcode_volume_create(&model,64,allocate,release,stop,NULL,NULL);assert(volume);gcode_parser_init(&second,&replay,one,1);second.segment_sink=sink;second.sink_context=volume;source(&second);mesh=gcode_volume_finish(volume);assert(mesh&&allocations==1);release(mesh);assert(!allocations);
 puts("PASS: finite unit normals, checked topology bounds, every create allocation failure and optional mesh compaction failure cleanup");
 return 0;
}
