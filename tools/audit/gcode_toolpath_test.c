#include "gcode_toolpath.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static gcode_toolpath_t model;
static gcode_parser_t parser;
static gcode_segment_t storage[100];
static void reset(size_t capacity){gcode_parser_init(&parser,&model,storage,capacity);}
static void feed(const char *s){gcode_parser_feed(&parser,s,strlen(s));}
static void near(float a,float b){assert(fabsf(a-b)<.0001f);}
int main(void){
 const char *source="G90\r\nM83\nG1X10Y10Z.2\nG1X20E1\nG1Y20E1\nG0Z.4\nG1X10E1\nG92E0\nG1Y10E1";
 reset(100);feed(source);gcode_parser_finish(&parser);assert(model.count==5&&model.layer_count==2);near(model.min.x,10);near(model.max.y,20);near(model.layer_z[1],.4);assert(!model.unsupported&&!model.malformed);assert(!storage[2].extrusion&&storage[2].layer==1);
 gcode_segment_t expected[5];memcpy(expected,storage,sizeof(expected));
 for(size_t chunk=1;chunk<=strlen(source);chunk++){
  reset(100);for(size_t at=0;at<strlen(source);at+=chunk){size_t n=strlen(source)-at;if(n>chunk)n=chunk;assert(gcode_parser_feed(&parser,source+at,n));}gcode_parser_finish(&parser);assert(model.count==5&&model.layer_count==2);for(unsigned i=0;i<5;i++){assert(!memcmp(&expected[i].a,&storage[i].a,sizeof(gcode_point_t)));assert(!memcmp(&expected[i].b,&storage[i].b,sizeof(gcode_point_t)));assert(expected[i].layer==storage[i].layer&&expected[i].extrusion==storage[i].extrusion);}
 }
 reset(100);feed("G1 X10 Y20 Z.2\nG92 X0 Y0 E0\nG1 X5 Y6 E2\nG91\nG1 X-2 Y3 E1\nG90\nG1 X8 Y9 E4\n");assert(model.count==3);near(storage[0].b.x,15);near(storage[0].b.y,26);near(storage[1].b.x,13);near(storage[1].b.y,29);near(storage[2].b.x,18);assert(storage[2].extrusion);
 reset(100);feed("G1 X10 Y10\nM82\nG1 X11 E3\nG1 X12 E2\nG1 X13 E3\nG1 E8\nG1 X14 E8\n");assert(model.count==4);assert(storage[0].extrusion&&!storage[1].extrusion&&storage[2].extrusion&&!storage[3].extrusion);
 reset(100);feed("G1 X10 Y10\nG1 X20 E1\nG28\nG1 X100 Y100\nG1 X110 E2\n");assert(model.count==2);near(storage[1].a.x,100);
 reset(1);assert(gcode_parser_feed(&parser,source,strlen(source)));gcode_parser_finish(&parser);assert(model.count==1&&!model.bounded&&model.input_segments==5&&model.sample_stride>1);
 reset(100);feed("START_PRINT BED_TEMP=60\nG90\nM83\nG1 X10 Y10\nG1 X20 E1\nG5 X30 Y30 I1 J1\nG1 X40 E2\n");assert(model.count==1&&model.unsupported&&parser.halted);
 reset(100);feed("G1 X10 Y10\nG1 Xnan\n");assert(model.malformed&&model.count==0);
 reset(100);feed("G1 X10 Y10\nG1 X1e2\n");assert(!model.malformed);assert(storage[0].b.x==1&&storage[0].extrusion); /* compact X1 E2 */
 reset(100);feed("G1 X10 Y10\nG1 X20 X30\n");assert(model.malformed);
 reset(100);char longline[2048];memset(longline,'x',sizeof(longline));assert(!gcode_parser_feed(&parser,longline,sizeof(longline)));assert(model.malformed);
 reset(100);feed("; thumbnail begin\n;");memset(longline,'A',sizeof(longline));for(unsigned i=0;i<100;i++)assert(gcode_parser_feed(&parser,longline,sizeof(longline)));feed("\nG1 X10 Y10\nG1 X20 E1\n");assert(model.count==1&&!model.malformed);
 reset(100);parser.path->bytes=GCODE_TOOLPATH_BYTES_MAX;assert(!gcode_parser_feed(&parser,";",1)&&model.bounded);
 reset(100);feed("G1 X10 Y10\n");model.layer_count=GCODE_TOOLPATH_LAYERS_MAX;feed("G1 X20 Z1 E1\n");assert(model.bounded&&model.count==0);
 reset(100);feed(source);gcode_parser_finish(&parser);gcode_view_t view;gcode_view_reset(&view);
 enum{W=128,H=96};uint16_t *pixels=calloc(W*H+2,sizeof(*pixels));pixels[0]=0x1234;pixels[W*H+1]=0xabcd;
 gcode_toolpath_render(&model,&view,pixels+1,W,H);uint16_t original[W*H];memcpy(original,pixels+1,sizeof(original));view.yaw+=.5f;gcode_toolpath_render(&model,&view,pixels+1,W,H);assert(memcmp(original,pixels+1,sizeof(original)));
 gcode_view_reset(&view);gcode_toolpath_render(&model,&view,pixels+1,W,H);assert(!memcmp(original,pixels+1,sizeof(original)));view.layer=0;gcode_toolpath_render(&model,&view,pixels+1,W,H);assert(memcmp(original,pixels+1,sizeof(original)));view.layer=1;view.cumulative=true;gcode_toolpath_render(&model,&view,pixels+1,W,H);assert(!memcmp(original,pixels+1,sizeof(original)));
 view.layer=0;view.cumulative=false;gcode_toolpath_render(&model,&view,pixels+1,W,H);uint16_t first_layer[W*H];memcpy(first_layer,pixels+1,sizeof(first_layer));view.cumulative=true;gcode_toolpath_render(&model,&view,pixels+1,W,H);assert(!memcmp(first_layer,pixels+1,sizeof(first_layer)));
 view.zoom=4;view.travel=true;gcode_toolpath_render(&model,&view,pixels+1,W,H);assert(pixels[0]==0x1234&&pixels[W*H+1]==0xabcd);free(pixels);
 /* Deterministic malformed byte streams under ASan/UBSan. */
 unsigned rng=12345;for(unsigned trial=0;trial<5000;trial++){reset(100);unsigned char fuzz[128];for(unsigned i=0;i<sizeof(fuzz);i++){rng=rng*1664525u+1013904223u;fuzz[i]=(unsigned char)(rng>>24);}gcode_parser_feed(&parser,fuzz,sizeof(fuzz));gcode_parser_finish(&parser);assert(model.count<=100);}
 puts("PASS: chunks, compact axes, modes, G92, extrusion/travel, homing discontinuity, bounds, unsupported paths, rendering and 5000 malformed streams");
}
