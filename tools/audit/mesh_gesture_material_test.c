#include "mesh_preview.h"
#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
int main(void){
    enum{W=952,H=332,N=W*H,COUNT=1500};
    mesh_preview_t mesh={.min={-50,-30,0},.max={50,30,1}};
    gcode_toolpath_t *paths=calloc(1,sizeof(*paths));assert(paths);
    paths->segments=calloc(COUNT,sizeof(*paths->segments));assert(paths->segments);
    paths->count=paths->capacity=COUNT;paths->sample_stride=1;paths->layer_count=1;
    for(unsigned i=0;i<COUNT;i++){
        float x=-49+(i%50)*2.f,y=-29+(i/50)*2.f;
        paths->segments[i]=(gcode_segment_t){.a={x,y,.2f},.b={x+1,y,.2f},.extrusion=true,.width=.45f,.height=.2f};
    }
    paths->layer_anchor[0]=paths->segments[0];
    uint16_t *pixels=malloc((N+2)*sizeof(*pixels)),*reference=malloc(N*sizeof(*reference));
    float *depth=malloc((N+2)*sizeof(*depth)),*reference_depth=malloc(N*sizeof(*reference_depth));assert(pixels&&reference&&depth&&reference_depth);
    pixels[0]=0x1234;pixels[N+1]=0xabcd;depth[0]=12345;depth[N+1]=54321;
    gcode_view_t view;gcode_view_reset(&view);view.layer=0;view.pitch=1.5707963f;view.yaw=0;
    for(unsigned i=0;i<N;i++){pixels[i+1]=0x0863;depth[i+1]=-FLT_MAX;}
    mesh_preview_render_layer_gesture_step(&mesh,paths,&view,pixels+1,depth+1,W,H,0,SIZE_MAX,NULL);
    float scale=.43f*H/(.5f*sqrtf(100*100+60*60+1));
    for(unsigned i=0;i<COUNT;i++){
        const gcode_segment_t *p=&paths->segments[i];int x=(int)lroundf(W*.5f+(p->a.x+.5f)*scale),y=(int)lroundf(H*.5f+p->a.y*scale);
        assert(pixels[1+y*W+x]!=0x0863);assert((pixels[1+y*W+x]>>11)>(pixels[1+y*W+x]&31)*2);
        assert(fabsf(depth[1+y*W+x]+.3f)<.0001f||fabsf(depth[1+y*W+x]+.298f)<.0001f);
        if(i/50<29){int gap=(int)lroundf(H*.5f+(p->a.y+1)*scale);assert(pixels[1+gap*W+x]==0x0863);}
    }
    memcpy(reference,pixels+1,N*sizeof(*reference));memcpy(reference_depth,depth+1,N*sizeof(*reference_depth));
    for(unsigned i=0;i<N;i++){pixels[i+1]=0x0863;depth[i+1]=-FLT_MAX;}
    size_t cursor=0;do{cursor=mesh_preview_render_layer_gesture_step(&mesh,paths,&view,pixels+1,depth+1,W,H,cursor,17,NULL);}while(cursor);
    assert(!memcmp(reference,pixels+1,N*sizeof(*reference)));assert(!memcmp(reference_depth,depth+1,N*sizeof(*reference_depth)));
    /* Existing depth occludes all material, proving this is a depth-tested solid overlay. */
    for(unsigned i=0;i<N;i++){pixels[i+1]=0x1234;depth[i+1]=FLT_MAX;}
    mesh_preview_render_layer_gesture_step(&mesh,paths,&view,pixels+1,depth+1,W,H,0,SIZE_MAX,NULL);
    for(unsigned i=0;i<N;i++)assert(pixels[i+1]==0x1234&&depth[i+1]==FLT_MAX);
    /* Never turn travel or other layers into deposited material. */
    for(unsigned i=0;i<COUNT;i++){paths->segments[i].extrusion=false;}paths->layer_anchor[0].extrusion=false;
    for(unsigned i=0;i<N;i++){pixels[i+1]=0x0863;depth[i+1]=-FLT_MAX;}
    mesh_preview_render_layer_gesture_step(&mesh,paths,&view,pixels+1,depth+1,W,H,0,SIZE_MAX,NULL);
    for(unsigned i=0;i<N;i++)assert(pixels[i+1]==0x0863);
    for(unsigned i=0;i<COUNT;i++){paths->segments[i].extrusion=true;paths->segments[i].layer=1;}paths->layer_anchor[0].layer=1;
    mesh_preview_render_layer_gesture_step(&mesh,paths,&view,pixels+1,depth+1,W,H,0,SIZE_MAX,NULL);
    for(unsigned i=0;i<N;i++)assert(pixels[i+1]==0x0863);
    for(unsigned i=0;i<COUNT;i++){paths->segments[i].layer=0;}paths->layer_anchor[0]=paths->segments[0];
    clock_t elapsed[2]={0,0};
    for(unsigned kind=0;kind<2;kind++)for(unsigned frame=0;frame<48;frame++){
        gcode_view_reset(&view);view.layer=0;view.raster_ratio=.5f;view.yaw=frame*.19f;view.pitch=-1.5f+(frame%24)*.13f;view.zoom=frame%3==0?16:frame%3==1?.25f:2;
        for(unsigned i=0;i<N;i++){pixels[i+1]=0x0863;depth[i+1]=-FLT_MAX;}
        clock_t started=clock();size_t first=0;
        do{first=(kind?mesh_preview_render_layer_gesture_step:mesh_preview_render_layer_step)(&mesh,paths,&view,pixels+1,depth+1,W/2,H/2,first,64,NULL);}while(first);
        elapsed[kind]+=clock()-started;
        assert(pixels[0]==0x1234&&pixels[N+1]==0xabcd&&depth[0]==12345&&depth[N+1]==54321);
    }
    printf("Dense material host CPU (not tablet FPS): detailed %.2f ms/frame, gesture %.2f ms/frame\n",1000.*elapsed[0]/CLOCKS_PER_SEC/48,1000.*elapsed[1]/CLOCKS_PER_SEC/48);
    free(paths->segments);free(paths);free(pixels);free(reference);free(depth);free(reference_depth);
    puts("PASS: all 1500 source paths present as gold solids, real top Z, open gaps, incremental equivalence, depth occlusion, layer/travel filtering and raster canaries");return 0;
}
