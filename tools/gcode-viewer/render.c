/* Host-only parser/raster benchmark. Does not connect to a printer. */
#include "gcode_toolpath.h"
#include "gcode_surface.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static void surface_sink(void *m,const gcode_segment_t *s){gcode_surface_feed(m,s);}
int main(int argc,char **argv){
 if(argc!=3&&(argc!=4||(strcmp(argv[3],"--solid")&&strcmp(argv[3],"--surface")))){fprintf(stderr,"Usage: %s input.gcode output.ppm [--solid|--surface]\n",argv[0]);return 2;}
 FILE *in=fopen(argv[1],"rb");if(!in){perror("input");return 1;}
 gcode_toolpath_t *model=calloc(1,sizeof(*model));gcode_parser_t *parser=calloc(1,sizeof(*parser));
 gcode_segment_t *segments=calloc(GCODE_TOOLPATH_SEGMENTS_MAX,sizeof(*segments));uint16_t *pixels=calloc(952*332,sizeof(*pixels));
 struct gcode_surface *surface=argc==4&&!strcmp(argv[3],"--surface")?calloc(1,sizeof(*surface)+GCODE_SURFACE_PANELS_MAX*sizeof(gcode_panel_t)+GCODE_SURFACE_SLOTS*sizeof(uint32_t)):NULL;
 float *depth=argc==4?calloc(952*332,sizeof(float)):NULL;
 if(!model||!parser||!segments||!pixels||(argc==4&&!depth)){fclose(in);free(model);free(parser);free(segments);free(pixels);free(depth);free(surface);return 1;}
 clock_t start=clock();gcode_parser_init(parser,model,segments,GCODE_TOOLPATH_SEGMENTS_MAX);
 if(surface){gcode_panel_t *panels=(gcode_panel_t *)(surface+1);gcode_surface_init(surface,panels,GCODE_SURFACE_PANELS_MAX,(uint32_t *)(panels+GCODE_SURFACE_PANELS_MAX));model->surface=surface;parser->segment_sink=surface_sink;parser->sink_context=surface;}
 char chunk[2048];size_t n;
 while((n=fread(chunk,1,sizeof(chunk),in))){if(!gcode_parser_feed(parser,chunk,n))break;}
 if(ferror(in)){perror("read");fclose(in);free(model);free(parser);free(segments);free(pixels);free(depth);free(surface);return 1;}
 fclose(in);gcode_parser_finish(parser);if(surface)gcode_surface_finish(surface);double parse_ms=1000.0*(clock()-start)/CLOCKS_PER_SEC;
 gcode_view_t view;gcode_view_reset(&view);start=clock();
 for(unsigned frame=0;frame<20;frame++){view.yaw+=.025f;if(gcode_surface_usable(surface))gcode_surface_render_step(model,&view,pixels,depth,952,332,0,SIZE_MAX);else if(depth)gcode_toolpath_render_solid(model,&view,pixels,depth,952,332);else gcode_toolpath_render(model,&view,pixels,952,332);}
 double render_ms=1000.0*(clock()-start)/CLOCKS_PER_SEC/20;
 FILE*out=fopen(argv[2],"wb");if(!out){perror("output");free(model);free(parser);free(segments);free(pixels);free(depth);free(surface);return 1;}
 fprintf(out,"P6\n952 332\n255\n");for(unsigned i=0;i<952*332;i++){uint16_t v=pixels[i];unsigned char rgb[3]={(v>>11)*255/31,((v>>5)&63)*255/63,(v&31)*255/31};fwrite(rgb,1,3,out);}int error=fclose(out);
 printf("Host CPU only: %zu bytes, %zu segments, %u layers; parse %.2f ms, mean raster %.2f ms/frame; geometry capacity %zu bytes; partial=%d\n",model->bytes,model->count,model->layer_count,parse_ms,render_ms,sizeof(*model)+GCODE_TOOLPATH_SEGMENTS_MAX*sizeof(*segments),model->bounded||model->unsupported||model->malformed);
 if(surface)printf("Surface: %zu panels, %zu merged runs, fallback=%d\n",surface->count,surface->merged,!gcode_surface_usable(surface));
 free(model);free(parser);free(segments);free(pixels);free(depth);free(surface);return error?1:0;
}
