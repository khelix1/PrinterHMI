#include "gcode_toolpath.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static gcode_toolpath_t m;
static gcode_parser_t p;
static gcode_segment_t segments[5000];
static void reset(void){gcode_parser_init(&p,&m,segments,5000);}
static void feed(const char *s){gcode_parser_feed(&p,s,strlen(s));}
static void near(double a,double b){if(fabs(a-b)>.002){fprintf(stderr,"expected %.6f got %.6f\n",b,a);abort();}}
static void good(void){assert(!m.bounded&&!m.malformed&&!p.halted);}
static void circle_radius(float x,float y,float radius){for(size_t i=0;i<m.count;i++){near(hypot(segments[i].b.x-x,segments[i].b.y-y),radius);}}
int main(void){
 reset();feed("G90\nM83\nG1 X10 Y0 Z.2\nG3 X0 Y10 I-10 J0 E2\n");good();assert(m.arcs==1&&m.count>=18);near(m.max.y,10);near(m.min.x,0);circle_radius(0,0,10);assert(segments[0].b.y>0&&segments[0].b.x<10);near(segments[m.count-1].b.y,10);
 reset();feed("G1 X10 Y0 Z.2\nM83\nG2 X0 Y-10 I-10 J0 E1\n");good();assert(segments[0].b.y<0);circle_radius(0,0,10);
 reset();feed("G1 X10 Y0 Z.2\nM83\nG3 I-10 J0 E1\n");good();assert(m.arcs==1&&m.count>=72);near(m.min.x,-10);near(m.max.x,10);near(m.min.y,-10);near(m.max.y,10);near(segments[m.count-1].b.x,10);
 reset();feed("G1 X10 Y0 Z.2\nM83\nG2 I-10 J0 Z1.2 E1\n");good();assert(m.layer_count>1);near(segments[m.count-1].b.z,1.2);for(size_t i=0;i<m.count;i++)assert(segments[i].b.z>segments[i].a.z);
 reset();feed("G1 X9 Y6 Z.2\nM83\nG3 X2 Y7 R5 E1\n");good();assert(m.count<40);circle_radius(5,3,5);near(segments[m.count-1].b.x,2);
 reset();feed("G1 X9 Y6 Z.2\nM83\nG3 X2 Y7 R-5 E1\n");good();assert(m.count>40);
 reset();feed("G1 X10 Y0 Z0\nG18\nM83\nG3 X0 Z10 I-10 K0 E1\n");good();near(segments[m.count-1].b.z,10);assert(segments[0].b.z>0);
 reset();feed("G1 X0 Y10 Z0\nG19\nM83\nG3 Y0 Z10 J-10 K0 E1\n");good();near(segments[m.count-1].b.z,10);
 reset();feed("G1 X10 Y0 Z.2\nG91\nM83\nG3 X-10 Y10 I-10 J0 E1\n");good();near(segments[m.count-1].b.x,0);near(segments[m.count-1].b.y,10);
 reset();feed("G1 X20 Y10 Z.2\nG90.1\nM83\nG3 X10 Y20 I10 J10 E1\n");good();circle_radius(10,10,10);
 const char *bad[]={"G3 X20 Y20 I-10 J0\n","G2 R1\n","G3 X0 Y10 R1\n","G3 X0 Y10 I-10 R10\n","G3 I0 J0\n","G3 X0 Y10 K10\n"};
 for(unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);i++){reset();feed("G1 X10 Y0 Z.2\n");feed(bad[i]);assert(m.malformed&&m.count==0&&m.stop_line==2);}
 reset();feed("G1 X10000 Y0\nG3 I-10000 J0\n");good();assert(m.coarse_arcs&&m.count==GCODE_TOOLPATH_ARC_MAX&&!m.stop_reason[0]);
 reset();m.capacity=10;feed("G1 X10 Y0\nG3 I-10 J0\n");good();assert(m.input_segments>=72&&m.count<=10&&m.sample_stride>1);
 reset();feed("G20\nM83\nG1 X1 Y1 Z.1\nG1 X2 E1\nG21\nG1 X60 E1\n");good();near(segments[0].a.x,25.4);near(segments[0].b.x,50.8);near(segments[1].b.x,60);near(segments[0].b.z,2.54);
 reset();feed("G1 X10 Y10 Z.2\nM82\nG1 X20 E10\nT1\nG0 X30 Y10\nG1 X40 E1\nT0\nG0 X50 Y10\nG1 X60 E11\n");good();assert(m.count==3&&m.tool_changes==2&&m.tool_mask==3&&m.unsupported);assert(segments[0].tool==0&&segments[1].tool==1&&segments[2].tool==0);for(unsigned i=0;i<3;i++)assert(segments[i].extrusion);
 reset();feed("ACTIVATE_EXTRUDER EXTRUDER=extruder2\nG1 X10 Y10\nM83\nG1 X20 E1\n");good();assert(segments[0].tool==2&&m.tool_mask==4);
 reset();feed("T16\n");assert(p.halted&&!m.count);reset();feed("ACTIVATE_EXTRUDER EXTRUDER=a\n");assert(p.halted);
 reset();feed("G1 X10 Y10 Z1\nSET_GCODE_OFFSET Z=2 MOVE=1\nG1 X20 Z1 E1\nSET_GCODE_OFFSET Z_ADJUST=1\nG1 X30 Z1 E2\n");good();assert(m.count==3);near(segments[0].b.z,3);assert(!segments[0].extrusion);near(segments[1].b.z,3);near(segments[2].b.z,4);
 reset();feed("G1 X10 Y10 Z1\nM83\nSAVE_GCODE_STATE NAME=a\nG91\nG1 X5 E1\nRESTORE_GCODE_STATE NAME=a MOVE=1\nG1 X20 E1\n");good();assert(m.count==3);near(segments[1].b.x,10);assert(!segments[1].extrusion);near(segments[2].b.x,20);assert(!p.relative_xyz);
 reset();feed("G1 X10 Y10\nG92\nG1 X5 Y5 E1\n");good();near(segments[0].b.x,15);near(segments[0].b.y,15);
 reset();feed("G10\nG11\n");good();assert(m.unsupported&&m.retracts==2);
 reset();feed("RESTORE_GCODE_STATE NAME=missing\n");assert(p.halted);
 reset();feed("SAVE_GCODE_STATE NAME=a\nSAVE_GCODE_STATE NAME=b\nSAVE_GCODE_STATE NAME=c\nSAVE_GCODE_STATE NAME=d\nSAVE_GCODE_STATE NAME=e\n");assert(m.bounded);
 reset();feed("SET_GCODE_OFFSET X=1 X=2\n");assert(m.malformed);
 reset();feed("; header\r\n\r\n  start_print SECRET=hidden\r\n\nG1 X10 Y10\nG1 X20 E1\nG5 X0 PASSWORD=hidden\n");assert(m.omitted_line==3&&m.stop_line==7&&m.lines==7&&!strcmp(m.stop_command,"G5"));assert(!strstr(m.stop_reason,"hidden")&&!strstr(m.omitted_command,"SECRET"));
 reset();feed("G1 X10 Y10\nM83\nG1 X20 E1\n  stop_print SECRET=hidden\nG1 X100 Y100\nG1 X110 E1\n");good();assert(m.unsupported&&m.omitted_count==1&&m.omitted_line==4&&!strcmp(m.omitted_command,"STOP_PRINT")&&!m.stop_line&&m.count==2);near(segments[1].a.x,100);near(segments[1].b.x,110);
 reset();feed("STOP_PRINTING\n");assert(p.halted&&m.stop_line==1&&!strcmp(m.stop_command,"STOP_PRINTING"));
 reset();feed(";WIDTH:0.62\r\n;HEIGHT:0.31\nM83\nG1 X0 Y0 Z.31\nG1 X10 E1\n");good();near(segments[0].width,.62);near(segments[0].height,.31);assert(m.dimension_tags==2&&m.max_width>.61f&&m.extrusion_min.x==0&&m.extrusion_max.x==10);
 reset();feed("G1 X0 Y0 Z.2 ;WIDTH:9\n;WIDTH:nan\n;HEIGHT:100\nM83\nG1 X10 E1\n");good();near(segments[0].width,.45);near(segments[0].height,.2);assert(!m.dimension_tags);
 reset();feed(";WID");feed("TH:0.7\n;HEIGHT:0.4");gcode_parser_finish(&p);assert(m.dimension_tags==2);near(p.extrusion_width,.7);near(p.extrusion_height,.4);
 const char *metadata="SET_PRINT_STATS_INFO CURRENT_LAYER=1\nEXCLUDE_OBJECT_START NAME=cube\nSET_VELOCITY_LIMIT ACCEL=500\nSET_PRESSURE_ADVANCE ADVANCE=.04\nG1 X10 Y10\nM83\nG1 X20 E1\nEXCLUDE_OBJECT_END NAME=cube\n";
 for(size_t chunk=1;chunk<strlen(metadata);chunk++){reset();for(size_t at=0;at<strlen(metadata);at+=chunk){size_t n=strlen(metadata)-at;if(n>chunk)n=chunk;assert(gcode_parser_feed(&p,metadata+at,n));}gcode_parser_finish(&p);good();assert(m.count==1&&m.lines==8&&!m.unsupported);}
 reset();feed("EXCLUDE_OBJECT_DEFINE NAME=cube POLYGON=");char payload[2000];memset(payload,'1',sizeof(payload));assert(gcode_parser_feed(&p,payload,sizeof(payload)));feed("\nG1 X10 Y10\nG1 X20 E1\n");good();assert(m.count==1&&m.lines==3);
 reset();feed("SET_PRINT_STATS_INFO_EVIL X=0\n");assert(p.halted);reset();feed("SET_GCODE_OFFSET_EVIL X=0\n");assert(p.halted);
 /* Every source chunk size gives identical arc endpoints/counts. */
 const char *arcs="G1 X10 Y0 Z.2\r\nM83\r\nG3 I-10 J0 E1\r\nG2 X0 Y-10 R10 E1";
 size_t expected=0;for(size_t chunk=1;chunk<=strlen(arcs);chunk++){reset();for(size_t at=0;at<strlen(arcs);at+=chunk){size_t n=strlen(arcs)-at;if(n>chunk)n=chunk;assert(gcode_parser_feed(&p,arcs+at,n));}gcode_parser_finish(&p);good();if(!expected)expected=m.count;assert(m.count==expected&&m.lines==4);near(segments[m.count-1].b.y,-10);}
 reset();feed("G1 X10 Y10 Z1\nG92 X0\nG28 Z\nG1 X0 Y10 Z.2\nG1 X5 E1\n");good();assert(m.homings==1&&m.unsupported);near(segments[0].a.x,10);near(segments[0].b.x,15);
 unsigned rng=2468;const char *prefixes[]={"G2 ","G3 ","SET_GCODE_OFFSET ","SAVE_GCODE_STATE ","RESTORE_GCODE_STATE ","ACTIVATE_EXTRUDER "};
 for(unsigned trial=0;trial<2000;trial++){
  reset();feed("G1 X10 Y10 Z.2\n");feed(prefixes[trial%6]);char bytes[128];
  for(unsigned i=0;i<sizeof(bytes);i++){rng=rng*1664525u+1013904223u;bytes[i]=(char)(32+(rng>>24)%95);}
  gcode_parser_feed(&p,bytes,sizeof(bytes));gcode_parser_finish(&p);assert(m.count<=5000);
 }
 puts("PASS: CW/CCW/full/helical/three-plane/IJK/R/relative/absolute-center arcs, units, bounded invalid arcs, tool E/colors metadata, offsets/restore moves, physical line diagnostics and exact bookkeeping allowlist");
}
