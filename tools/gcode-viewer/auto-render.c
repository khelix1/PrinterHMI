/* Host-only two-pass automatic G-code volume/mesh benchmark. */
#include "gcode_volume.h"
#include <stdlib.h>
#include <time.h>
static bool fed;
static void sink(void *v,const gcode_segment_t *s){if(fed)fed=gcode_volume_feed(v,s);}
static bool parse(FILE *f,gcode_parser_t *p){char data[2048];size_t n;while((n=fread(data,1,sizeof(data),f)))if(!gcode_parser_feed(p,data,n))break;gcode_parser_finish(p);return !ferror(f);}
int main(int argc,char **argv){
 if(argc<3||argc>5){fprintf(stderr,"Usage: %s input.gcode output.ppm [resolution:32..512] [frames:1..200]\n",argv[0]);return 2;}
 unsigned frames=argc==5?(unsigned)atoi(argv[4]):1;if(!frames||frames>200)return 2;
 unsigned resolution=argc>=4?(unsigned)atoi(argv[3]):256;if(resolution<32||resolution>512)return 2;
 FILE *f=fopen(argv[1],"rb");if(!f)return 1;
 gcode_toolpath_t *m=calloc(1,sizeof(*m)),*r=calloc(1,sizeof(*r));gcode_parser_t *p=calloc(1,sizeof(*p));gcode_segment_t initial[8],one[1];if(!m||!r||!p)return 1;
 clock_t start=clock();gcode_parser_init(p,m,initial,8);if(!parse(f,p))return 1;
 gcode_volume_t *v=gcode_volume_create(m,resolution,malloc,free,NULL,NULL,NULL);if(!v)return 1;
 rewind(f);gcode_parser_init(p,r,one,1);p->segment_sink=sink;p->sink_context=v;fed=true;if(!parse(f,p)||!fed){gcode_volume_destroy(v);return 1;}fclose(f);
 mesh_preview_t *mesh=gcode_volume_finish(v);if(!mesh)return 1;double prepare=1000.*(clock()-start)/CLOCKS_PER_SEC;
 clock_t lod_start=clock();mesh_preview_prepare_interaction(mesh,malloc,free,NULL,NULL);double lod_ms=1000.*(clock()-lod_start)/CLOCKS_PER_SEC;
 enum{W=952,H=332};uint16_t *pixels=calloc(W*H,2);float *depth=calloc(W*H,4);if(!pixels||!depth)return 1;gcode_view_t view;gcode_view_reset(&view);double interactive_ms=0;
 if(mesh->interaction){uint16_t *small=calloc(W/2*H/2,2);if(!small)return 1;start=clock();for(unsigned frame=0;frame<frames;frame++){view.yaw=-.7f+frame*.09f;mesh_preview_render_step(mesh->interaction,&view,small,depth,W/2,H/2,0,SIZE_MAX);}interactive_ms=1000.*(clock()-start)/CLOCKS_PER_SEC/frames;free(small);}
 start=clock();for(unsigned frame=0;frame<frames;frame++){view.yaw=-.7f+frame*.09f;mesh_preview_render_step(mesh,&view,pixels,depth,W,H,0,SIZE_MAX);}double render=1000.*(clock()-start)/CLOCKS_PER_SEC/frames;
 f=fopen(argv[2],"wb");if(!f)return 1;fprintf(f,"P6\n%d %d\n255\n",W,H);for(unsigned i=0;i<W*H;i++){uint16_t x=pixels[i];unsigned char c[3]={(x>>11)*255/31,((x>>5)&63)*255/63,(x&31)*255/31};fwrite(c,1,3,f);}int result=fclose(f);
 printf("Host CPU only: %lu input paths -> %lu triangles / %lu vertices; %.3f mm cells; prepare %.1f ms, raster %.2f ms/frame (%u views)\n",(unsigned long)m->input_segments,(unsigned long)mesh->triangles,(unsigned long)mesh->vertices,(double)mesh->cell_mm,prepare,render,frames);
 printf("Gesture mesh: %lu triangles / %lu vertices; build %.1f ms, half-resolution raster %.2f ms/frame\n",(unsigned long)(mesh->interaction?mesh->interaction->triangles:0),(unsigned long)(mesh->interaction?mesh->interaction->vertices:0),lod_ms,interactive_ms);
 free(m);free(r);free(p);mesh_preview_free(mesh,free);free(pixels);free(depth);return result?1:0;
}
