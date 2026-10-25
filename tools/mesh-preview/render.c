/* Host-only PHM raster/benchmark; never contacts a printer. */
#include "mesh_preview.h"
#include <stdlib.h>
#include <time.h>
int main(int argc,char **argv){
    if(argc!=3){fprintf(stderr,"Usage: %s model.phm output.ppm\n",argv[0]);return 2;}
    FILE *file=fopen(argv[1],"rb");if(!file){perror("mesh");return 1;}char error[128];
    mesh_preview_t *m=mesh_preview_read(file,malloc,free,NULL,NULL,error,sizeof(error));fclose(file);if(!m){fprintf(stderr,"%s\n",error);return 1;}
    enum{W=952,H=412};uint16_t *pixels=calloc(W*H,sizeof(*pixels));float *depth=calloc(W*H,sizeof(*depth));if(!pixels||!depth){free(pixels);free(depth);free(m);return 1;}
    gcode_view_t view;gcode_view_reset(&view);clock_t start=clock();for(unsigned i=0;i<20;i++){view.yaw+=.02f;mesh_preview_render_step(m,&view,pixels,depth,W,H,0,SIZE_MAX);}double elapsed=1000.*(clock()-start)/CLOCKS_PER_SEC/20;
    file=fopen(argv[2],"wb");if(!file){perror("image");free(pixels);free(depth);free(m);return 1;}fprintf(file,"P6\n%d %d\n255\n",W,H);for(unsigned i=0;i<W*H;i++){uint16_t p=pixels[i];unsigned char c[3]={(p>>11)*255/31,((p>>5)&63)*255/63,(p&31)*255/31};fwrite(c,1,3,file);}int result=fclose(file);
    printf("Host CPU only: %lu triangles, %lu vertices, %.3f ms/frame; mesh RAM %zu bytes\n",(unsigned long)m->triangles,(unsigned long)m->vertices,elapsed,sizeof(*m)+(size_t)m->vertices*40+(size_t)m->triangles*6);free(pixels);free(depth);free(m);return result?1:0;
}
