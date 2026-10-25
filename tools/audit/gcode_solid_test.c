#include "gcode_toolpath.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
enum {W=160,H=120,N=W*H};
static uint16_t pixels[N+2],thin[N],reference[N];
static float depth[N+2];
static gcode_toolpath_t m;
static gcode_segment_t segments[3];
static unsigned colored(const uint16_t *p){unsigned count=0;for(unsigned i=0;i<N;i++)if(p[i]!=0x0863)count++;return count;}
int main(void){
 gcode_view_t v;gcode_view_reset(&v);v.yaw=0;v.pitch=0;
 m.segments=segments;m.count=2;m.min=(gcode_point_t){-1,-1,-1};m.max=(gcode_point_t){1,1,1};
 /* Same screen path, orange bead nearer camera than cyan bead. The image
  * must be independent of file order, including rounded end caps. */
 segments[0]=(gcode_segment_t){.a={-.8f,-.5f,0},.b={.8f,-.5f,0},.extrusion=true};
 segments[1]=(gcode_segment_t){.a={-.8f,.5f,0},.b={.8f,.5f,0},.extrusion=true,.tool=1,.layer=1};
 pixels[0]=0x1234;pixels[N+1]=0xabcd;depth[0]=123456;depth[N+1]=654321;
 gcode_toolpath_render(&m,&v,thin,W,H);
 gcode_toolpath_render_solid(&m,&v,pixels+1,depth+1,W,H);
 assert(colored(pixels+1)>colored(thin)*2);memcpy(reference,pixels+1,sizeof(reference));
 gcode_segment_t temp=segments[0];segments[0]=segments[1];segments[1]=temp;
 gcode_toolpath_render_solid(&m,&v,pixels+1,depth+1,W,H);assert(!memcmp(reference,pixels+1,sizeof(reference)));
 size_t cursor=gcode_toolpath_render_solid_step(&m,&v,pixels+1,depth+1,W,H,0,1);assert(cursor==1);
 cursor=gcode_toolpath_render_solid_step(&m,&v,pixels+1,depth+1,W,H,cursor,1);assert(!cursor&&!memcmp(reference,pixels+1,sizeof(reference)));
 assert(pixels[0]==0x1234&&pixels[N+1]==0xabcd&&depth[0]==123456&&depth[N+1]==654321);
 /* Filtering before the depth test exposes the selected back layer. */
 v.layer=0;gcode_toolpath_render_solid(&m,&v,pixels+1,depth+1,W,H);assert(memcmp(reference,pixels+1,sizeof(reference))&&colored(pixels+1));
 v.cumulative=true;v.layer=1;gcode_toolpath_render_solid(&m,&v,pixels+1,depth+1,W,H);assert(!memcmp(reference,pixels+1,sizeof(reference)));
 segments[0].extrusion=segments[1].extrusion=false;v.layer=-1;v.travel=false;
 gcode_toolpath_render_solid(&m,&v,pixels+1,depth+1,W,H);assert(!colored(pixels+1));
 v.travel=true;gcode_toolpath_render_solid(&m,&v,pixels+1,depth+1,W,H);assert(colored(pixels+1));
 /* End-on and diagonal paths, tiny dimensions, clipping, all orbit presets. */
 segments[0]=(gcode_segment_t){.a={-1,-1,-1},.b={1,1,1},.extrusion=true};
 segments[1]=(gcode_segment_t){.a={0,-1,0},.b={0,1,0},.extrusion=true,.tool=2};
 for(unsigned i=0;i<100;i++){v.yaw=(float)i*.1f;v.pitch=(float)i*.02f;v.zoom=i%2?4:.25f;
  gcode_toolpath_render_solid(&m,&v,pixels+1,depth+1,W,H);assert(colored(pixels+1));
  assert(pixels[0]==0x1234&&pixels[N+1]==0xabcd&&depth[0]==123456&&depth[N+1]==654321);
 }
 gcode_toolpath_render_solid(&m,&v,pixels+1,depth+1,1,1);
 gcode_toolpath_render_solid(NULL,&v,pixels+1,depth+1,W,H);assert(!colored(pixels+1));
 gcode_toolpath_render_solid(&m,NULL,pixels+1,depth+1,W,H);assert(!colored(pixels+1));
 puts("PASS: solid thickness/shading, order-independent near/far occlusion, layer/cumulative/travel filtering, end-on/diagonal beads, clipping and guarded pixel/depth buffers");
}
