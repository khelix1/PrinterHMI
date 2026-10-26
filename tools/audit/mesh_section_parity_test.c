#include "mesh_preview.h"
#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
/* Separately compiled renderer with the interior-face shortcut removed. */
size_t reference_mesh_preview_render_joined_section_step(mesh_preview_t *,const gcode_view_t *,uint16_t *,float *,int,int,size_t,size_t,float,float,uint32_t *);
size_t reference_mesh_preview_render_layer_step(const mesh_preview_t *,const gcode_toolpath_t *,const gcode_view_t *,uint16_t *,float *,int,int,size_t,size_t,int [4]);
int main(int argc,char **argv){
    assert(argc==2);FILE *file=fopen(argv[1],"rb");assert(file);char error[128];
    mesh_preview_t *mesh=mesh_preview_read(file,malloc,free,NULL,NULL,error,sizeof(error));fclose(file);assert(mesh);
    enum{W=192,H=144,N=W*H,WORDS=((W+31)/32)*H};
    uint16_t *actual=malloc(N*sizeof(*actual)),*expected=malloc(N*sizeof(*expected));
    float *depth=malloc(N*sizeof(*depth)),*reference_depth=malloc(N*sizeof(*reference_depth));
    uint32_t *mask=malloc(WORDS*sizeof(*mask)),*reference_mask=malloc(WORDS*sizeof(*reference_mask));
    assert(actual&&expected&&depth&&reference_depth&&mask&&reference_mask);
    for(unsigned frame=0;frame<192;frame++){
        gcode_view_t view;gcode_view_reset(&view);view.yaw=frame*.17f;view.pitch=-.6f+(frame%16)*.14f;
        view.zoom=frame%3==0?16:frame%3==1?.25f:2;view.pan_x=(frame%5-2.f)*.1f;view.pan_y=(frame%7-3.f)*.1f;
        float high=frame%4==0?mesh->vertex[frame%mesh->vertices].z:mesh->min[2]+(mesh->max[2]-mesh->min[2])*(frame%17)/16.f;
        float low=frame%2?high-.2f:-FLT_MAX;mesh->cull_backfaces=(frame&2)!=0;
        memset(actual,0,N*sizeof(*actual));memset(expected,0,N*sizeof(*expected));
        memset(mask,0,WORDS*sizeof(*mask));memset(reference_mask,0,WORDS*sizeof(*reference_mask));
        size_t cursor=0;do{cursor=mesh_preview_render_joined_section_step(mesh,&view,actual,depth,W,H,cursor,137,low,high,frame&4?mask:NULL);}while(cursor);
        reference_mesh_preview_render_joined_section_step(mesh,&view,expected,reference_depth,W,H,0,SIZE_MAX,low,high,frame&4?reference_mask:NULL);
        assert(!memcmp(actual,expected,N*sizeof(*actual)));
        assert(!memcmp(depth,reference_depth,N*sizeof(*depth)));
        assert(!memcmp(mask,reference_mask,WORDS*sizeof(*mask)));
    }
    /* Dense layer includes tiny arc chords, long infill, gaps, slopes and Z moves.
     * Compare full and half-resolution shaded solids, never substitute Lines. */
    gcode_toolpath_t *paths=calloc(1,sizeof(*paths));assert(paths);paths->segments=calloc(1500,sizeof(*paths->segments));assert(paths->segments);
    paths->count=paths->capacity=1500;paths->sample_stride=1;paths->layer_count=1;
    mesh_preview_t bounds={.min={-35,-28,0},.max={35,28,34}};
    for(unsigned i=0;i<1500;i++){
        float angle=i*.015f;
        gcode_point_t a={20*cosf(angle),20*sinf(angle),12},b={20*cosf(angle+.015f),20*sinf(angle+.015f),12};
        if(i%7==0){a=(gcode_point_t){-25,-24+(i%120)*.4f,12};b=(gcode_point_t){25,a.y,12};}
        if(i%19==0){a.z=11.8f;b.z=12.2f;}
        if(i%31==0){b.x=a.x;b.y=a.y;b.z=12.4f;}
        paths->segments[i]=(gcode_segment_t){.a=a,.b=b,.extrusion=i%13!=0,.width=.4f+(i%3)*.12f,.height=.2f};
    }
    paths->layer_anchor[0]=paths->segments[0];
    clock_t actual_ticks=0,reference_ticks=0;
    for(unsigned frame=0;frame<96;frame++){
        gcode_view_t view;gcode_view_reset(&view);view.layer=0;view.yaw=frame*.19f;view.pitch=-1.57f+(frame%24)*.137f;
        view.zoom=frame%3==0?16:frame%3==1?.25f:2;view.pan_x=(frame%5-2.f)*.1f;view.pan_y=(frame%7-3.f)*.1f;
        int width=frame&1?W/2:W,height=frame&1?H/2:H;view.raster_ratio=frame&1?.5f:1;
        for(unsigned i=0;i<N;i++){actual[i]=expected[i]=0x0863;depth[i]=reference_depth[i]=-FLT_MAX;}
        int actual_bounds[4]={width,height,-1,-1},expected_bounds[4]={width,height,-1,-1};
        clock_t started=clock();size_t cursor=0;do{cursor=mesh_preview_render_layer_step(&bounds,paths,&view,actual,depth,width,height,cursor,64,actual_bounds);}while(cursor);
        actual_ticks+=clock()-started;started=clock();cursor=0;
        do{cursor=reference_mesh_preview_render_layer_step(&bounds,paths,&view,expected,reference_depth,width,height,cursor,64,expected_bounds);}while(cursor);
        reference_ticks+=clock()-started;
        assert(!memcmp(actual,expected,N*sizeof(*actual)));assert(!memcmp(depth,reference_depth,N*sizeof(*depth)));assert(!memcmp(actual_bounds,expected_bounds,sizeof(actual_bounds)));
    }
    printf("Dense material host CPU: optimized %.2f ms/frame, v26 reference %.2f ms/frame (96 views, not tablet timings)\n",1000.*actual_ticks/CLOCKS_PER_SEC/96,1000.*reference_ticks/CLOCKS_PER_SEC/96);
    free(paths->segments);free(paths);
    puts("PASS: 96 dense 1500-path solid layer frames exactly match v26 pixels, depths and damage bounds at full/half resolution, all camera sides and extreme zoom");
    mesh_preview_free(mesh,free);free(actual);free(expected);free(depth);free(reference_depth);free(mask);free(reference_mask);
    puts("PASS: 192 section/slab frames exactly match unclipped-shortcut reference pixels, depths and masks across boundary heights, cameras, culling, masks and incremental batches");return 0;
}
