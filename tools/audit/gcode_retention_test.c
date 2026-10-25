#include "gcode_toolpath.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static gcode_toolpath_t m;
static gcode_parser_t p;
static gcode_segment_t samples[GCODE_TOOLPATH_SEGMENTS_MAX];
static void reset(size_t cap){gcode_parser_init(&p,&m,samples,cap);}
static void feed(const char *text){assert(gcode_parser_feed(&p,text,strlen(text)));}
int main(void){
 /* Reproduce 32,759 retained paths followed by an arc needing >9 slots. */
 reset(32768);feed("G90\nM83\nG1 X0 Y0 Z.2\n");
 for(unsigned i=0;i<32759;i++){char line[64];snprintf(line,sizeof(line),"G1 X%u E1\n",i%2?0:1);feed(line);}
 assert(m.count==32759);feed("G3 I-1 J0 E1\n");assert(m.arcs==1&&!m.bounded&&!m.stop_reason[0]);
 for(unsigned layer=1;layer<=900;layer++){
  char line[256];snprintf(line,sizeof(line),"G0 X10 Y10 Z%.2f\nG1 X20 E1\nG1 Y20 E1\nG1 X10 E1\nG1 Y10 E1\n",.2+layer*.2);feed(line);
 }
 gcode_parser_finish(&p);assert(!m.bounded&&!m.malformed&&!p.halted&&m.layer_count==901);
 assert(m.input_segments>32768&&m.count<=32768&&m.sample_stride>1);
 assert(m.last_segment.b.z>180&&m.last_segment.layer==900);
 for(unsigned i=0;i<m.layer_count;i++)assert(m.layer_anchor[i].extrusion&&m.layer_anchor[i].layer==i);
 for(size_t i=0;i<m.count;i++){assert(m.segments[i].sequence%m.sample_stride==0);if(i)assert(m.segments[i].sequence>m.segments[i-1].sequence);}
 /* The expanded buffer keeps all 200,000 paths without compaction. */
 reset(GCODE_TOOLPATH_SEGMENTS_MAX);feed("M83\nG1 X0 Y0 Z.2\n");
 for(unsigned i=0;i<200000;i++){char line[64];snprintf(line,sizeof(line),"G1 X%u E1\n",i%2?0:1);feed(line);}
 assert(m.input_segments==200000&&m.count==200000&&m.sample_stride==1);
 /* Compaction must not retag travel preceding a discarded extrusion. */
 reset(2);feed("M83\nG1 X0 Y0 Z.2\nG1 X1 E1\nG0 X2\nG1 X3 E1\nG1 X4 E1\nG0 X5\nG0 X6\nG1 X7 E1\nG1 X8 Z.4 E1\n");
 assert(m.input_segments==8&&m.count==2&&m.sample_stride==4&&m.layer_count==2);
 assert(m.segments[1].sequence==4&&!m.segments[1].extrusion&&m.segments[1].layer==0);
 assert(m.segments[1].a.x==4&&m.segments[1].b.x==5);assert(m.layer_anchor[1].sequence==7);
 enum{W=128,H=96};uint16_t pixels[W*H+2];pixels[0]=0x1234;pixels[W*H+1]=0xabcd;gcode_view_t view;gcode_view_reset(&view);view.layer=1;
 gcode_toolpath_render(&m,&view,pixels+1,W,H);unsigned colored=0;for(unsigned i=1;i<=W*H;i++)if(pixels[i]!=0x0863)colored++;assert(colored&&pixels[0]==0x1234&&pixels[W*H+1]==0xabcd);
 /* A late, sparse tool/layer and full bounds survive a much denser prefix. */
 reset(4);feed("M83\nG1 X0 Y0 Z.2\n");
 for(unsigned i=0;i<20000;i++){char line[64];snprintf(line,sizeof(line),"G1 X%u E1\n",i%2?0:1);feed(line);}
 feed("T7\nG0 X1000 Y1000 Z10\nG1 X1001 E1\nG1 Y1001 E1\n");
 assert(m.count<=4&&m.layer_count==2&&m.tool_anchor_mask&128&&m.tool_anchor[7].tool==7);
 assert(m.max.x==1001&&m.max.y==1001&&m.last_segment.b.y==1001&&m.layer_anchor[1].tool==7);
 view.layer=1;gcode_toolpath_render(&m,&view,pixels+1,W,H);colored=0;for(unsigned i=1;i<=W*H;i++)if(pixels[i]!=0x0863)colored++;assert(colored);
 /* Coarse giant arcs remain complete; capacity zero is still a hard error. */
 reset(4);feed("G1 X10000 Y0\nG3 I-10000 J0\n");assert(m.coarse_arcs&&!m.bounded&&m.input_segments==4096&&m.last_segment.b.x==10000);
 reset(0);assert(gcode_parser_feed(&p,"G1 X0 Y0\n",strlen("G1 X0 Y0\n")));assert(!gcode_parser_feed(&p,"G1 X1\n",6)&&m.bounded);
 printf("PASS: original 32759+G3 limit, 901 full input layers, independent uniform samples, late bounds/tool/layer anchors, last path, travel ownership, reduced-layer raster and zero-storage guard; geometry budget %zu bytes\n",sizeof(m)+sizeof(samples));
}
