#include "mesh_preview.h"
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
static unsigned allocations,attempts,fail_at,cancel_after;static bool fail_alloc;
static void *allocate(size_t n){if(fail_alloc||++attempts==fail_at)return NULL;void *p=malloc(n);if(p)allocations++;return p;}
static void release(void *p){if(p){assert(allocations);allocations--;free(p);}}
static bool cancel_lod(void *p){(void)p;return --cancel_after==0;}
static bool cancel(void *p){return *(bool *)p;}
static uint32_t crc(const unsigned char *p,size_t n){uint32_t v=0xffffffffu;while(n--){v^=*p++;for(unsigned j=0;j<8;j++)v=(v>>1)^((v&1)?0xedb88320u:0);}return v^0xffffffffu;}
static void put(unsigned char *p,uint32_t n){for(unsigned i=0;i<4;i++)p[i]=(unsigned char)(n>>(8*i));}
static void checksums(unsigned char *blob,size_t n){put(blob+56,crc(blob+80,n-80));put(blob+68,0);put(blob+68,crc(blob,80));}
static mesh_preview_t *read_blob(const unsigned char *p,size_t n,bool stopped){FILE *f=tmpfile();assert(f);assert(fwrite(p,1,n,f)==n);rewind(f);char error[128];mesh_preview_t *m=mesh_preview_read(f,allocate,release,cancel,&stopped,error,sizeof(error));fclose(f);if(!m)assert(error[0]);return m;}
int main(int argc,char **argv){
 assert(argc==2);FILE *f=fopen(argv[1],"rb");assert(f);fseek(f,0,SEEK_END);size_t size=(size_t)ftell(f);rewind(f);unsigned char *blob=malloc(size+1),*changed=malloc(size+1);assert(blob&&changed);assert(fread(blob,1,size,f)==size);fclose(f);
 mesh_preview_t *m=read_blob(blob,size,false);assert(m&&m->radius>0&&m->triangles==6144&&m->vertices==3201&&allocations==1);
 enum{W=192,H=144,N=W*H};static uint16_t pixels[N+2],reference[N];static float depth[N+2];pixels[0]=0x1234;pixels[N+1]=0xabcd;depth[0]=12345;depth[N+1]=54321;
 gcode_view_t view;gcode_view_reset(&view);mesh_preview_render_step(m,&view,pixels+1,depth+1,W,H,0,SIZE_MAX);memcpy(reference,pixels+1,sizeof(reference));unsigned count=0;uint16_t first=0;bool shaded=false;for(unsigned i=1;i<=N;i++)if(pixels[i]!=0x0863){count++;if(first&&pixels[i]!=first)shaded=true;first=pixels[i];}assert(count>1500&&shaded);
 float rotated_x=(m->vertex[0].x-(m->min[0]+m->max[0])*.5f)*cosf(view.yaw)-(m->vertex[0].y-(m->min[1]+m->max[1])*.5f)*sinf(view.yaw);assert(fabsf(rotated_x)>.01f);float base_scale=(m->projected[0].x-W*.5f)/rotated_x/view.zoom;
 size_t cursor=0;unsigned steps=0;do{cursor=mesh_preview_render_step(m,&view,pixels+1,depth+1,W,H,cursor,137);steps++;}while(cursor);assert(steps>50&&!memcmp(reference,pixels+1,sizeof(reference)));
 view.yaw=0;view.pitch=1.5707963f;mesh_preview_render_step(m,&view,pixels+1,depth+1,W,H,0,SIZE_MAX);assert(pixels[1+(H/2)*W+W/2]==0x0863);
 gcode_view_reset(&view);mesh_preview_render_step(m,&view,pixels+1,depth+1,W,H,0,SIZE_MAX);memcpy(reference,pixels+1,sizeof(reference));
 mesh_preview_render_clipped_step(m,&view,pixels+1,depth+1,W,H,0,SIZE_MAX,m->max[2]+1);assert(!memcmp(reference,pixels+1,sizeof(reference)));
 mesh_preview_render_clipped_step(m,&view,pixels+1,depth+1,W,H,0,SIZE_MAX,m->min[2]-1);for(unsigned i=0;i<N;i++)assert(pixels[i+1]==0x0863);
 float cut=(m->min[2]+m->max[2])*.5f;mesh_preview_render_clipped_step(m,&view,pixels+1,depth+1,W,H,0,SIZE_MAX,cut);assert(memcmp(reference,pixels+1,sizeof(reference)));memcpy(reference,pixels+1,sizeof(reference));cursor=0;do{cursor=mesh_preview_render_clipped_step(m,&view,pixels+1,depth+1,W,H,cursor,137,cut);}while(cursor);assert(!memcmp(reference,pixels+1,sizeof(reference)));
 for(unsigned i=0;i<80;i++){view.yaw=i*.13f;view.pitch=i*.03f;view.zoom=i%2?GCODE_VIEW_ZOOM_MAX:.25f;mesh_preview_render_step(m,&view,pixels+1,depth+1,W,H,0,SIZE_MAX);float rx=(m->vertex[0].x-(m->min[0]+m->max[0])*.5f)*cosf(view.yaw)-(m->vertex[0].y-(m->min[1]+m->max[1])*.5f)*sinf(view.yaw);if(fabsf(rx)>.01f)assert(fabsf((m->projected[0].x-W*.5f)/rx/view.zoom-base_scale)<.001f);int bounds[4];assert(mesh_preview_screen_bounds(m,&view,W,H,bounds));for(int y=0;y<H;y++)for(int x=0;x<W;x++)if(pixels[1+y*W+x]!=0x0863)assert(x>=bounds[0]&&x<=bounds[2]&&y>=bounds[1]&&y<=bounds[3]);mesh_preview_render_wire(m,&view,pixels+1,W,H,2000);assert(pixels[0]==0x1234&&pixels[N+1]==0xabcd&&depth[0]==12345&&depth[N+1]==54321);}
 /* Travel is a depth-tested overlay in the mesh camera, never a clear. */
 gcode_view_reset(&view);view.yaw=0;view.pitch=1.5707963f;mesh_preview_render_step(m,&view,pixels+1,depth+1,W,H,0,SIZE_MAX);memcpy(reference,pixels+1,sizeof(reference));static gcode_toolpath_t paths;gcode_segment_t travel[2]={0};paths.segments=travel;paths.count=paths.capacity=2;paths.sample_stride=1;view.travel=true;view.layer=-1;float cy=(m->min[1]+m->max[1])*.5f+1;int travel_y=H/2+(int)lroundf(base_scale);
 travel[0]=(gcode_segment_t){.a={m->min[0],cy,m->max[2]+5},.b={m->max[0],cy,m->max[2]+5},.layer=1};travel[1]=(gcode_segment_t){.a={m->min[0],cy,m->max[2]+5},.b={m->max[0],cy,m->max[2]+5},.extrusion=true};
 mesh_preview_render_travel_step(m,&paths,&view,pixels+1,depth+1,W,H,0,SIZE_MAX);assert(pixels[1+travel_y*W+W/2]==0x52aa);unsigned gold=0;for(unsigned i=0;i<N;i++)if(pixels[i+1]!=0x0863&&pixels[i+1]!=0x52aa)gold++;assert(gold>1000);memcpy(reference,pixels+1,sizeof(reference));mesh_preview_render_step(m,&view,pixels+1,depth+1,W,H,0,SIZE_MAX);cursor=0;do{cursor=mesh_preview_render_travel_step(m,&paths,&view,pixels+1,depth+1,W,H,cursor,1);}while(cursor);assert(!memcmp(reference,pixels+1,sizeof(reference)));
 mesh_preview_render_step(m,&view,pixels+1,depth+1,W,H,0,SIZE_MAX);memcpy(reference,pixels+1,sizeof(reference));view.layer=0;mesh_preview_render_travel_step(m,&paths,&view,pixels+1,depth+1,W,H,0,SIZE_MAX);assert(!memcmp(reference,pixels+1,sizeof(reference)));view.layer=1;travel[0].a.z=travel[0].b.z=m->min[2]-5;mesh_preview_render_travel_step(m,&paths,&view,pixels+1,depth+1,W,H,0,SIZE_MAX);assert(reference[travel_y*W+W/2+45]!=0x0863);assert(pixels[1+travel_y*W+W/2]==0x52aa&&pixels[1+travel_y*W+W/2+45]==reference[travel_y*W+W/2+45]);assert(pixels[0]==0x1234&&pixels[N+1]==0xabcd&&depth[0]==12345&&depth[N+1]==54321);
 /* Actual extrusion infill has shaded material and real empty gaps. */
 gcode_view_reset(&view);view.yaw=0;view.pitch=1.5707963f;view.layer=2;view.cumulative=true;paths.layer_count=3;travel[0]=(gcode_segment_t){.a={-10,-4,10},.b={10,-4,10},.extrusion=true,.layer=2,.width=2,.height=.4f};travel[1]=travel[0];travel[1].a.y=travel[1].b.y=4;
 for(unsigned i=0;i<N;i++){pixels[i+1]=0x0863;depth[i+1]=-INFINITY;}int material_bounds[4]={W,H,-1,-1};mesh_preview_render_layer_step(m,&paths,&view,pixels+1,depth+1,W,H,0,SIZE_MAX,material_bounds);assert(pixels[1+H/2*W+W/2]==0x0863);unsigned material=0;for(int y=0;y<H;y++)for(int x=0;x<W;x++)if(pixels[1+y*W+x]!=0x0863){material++;assert(x>=material_bounds[0]&&x<=material_bounds[2]&&y>=material_bounds[1]&&y<=material_bounds[3]);}assert(material>100);memcpy(reference,pixels+1,sizeof(reference));for(unsigned i=0;i<N;i++){pixels[i+1]=0x0863;depth[i+1]=-INFINITY;}cursor=0;do{cursor=mesh_preview_render_layer_step(m,&paths,&view,pixels+1,depth+1,W,H,cursor,1,NULL);}while(cursor);assert(!memcmp(reference,pixels+1,sizeof(reference)));
 for(unsigned i=0;i<N;i++){pixels[i+1]=0x0863;depth[i+1]=-INFINITY;}view.layer=1;mesh_preview_render_layer_step(m,&paths,&view,pixels+1,depth+1,W,H,0,SIZE_MAX,NULL);for(unsigned i=0;i<N;i++)assert(pixels[i+1]==0x0863);assert(pixels[0]==0x1234&&pixels[N+1]==0xabcd&&depth[0]==12345&&depth[N+1]==54321);
 /* Realistic 0.45 x 0.2 mm extrusion has a filled flattened end and real height. */
 paths.count=1;paths.layer_count=1;travel[0]=(gcode_segment_t){.a={-10,0,10},.b={10,0,10},.extrusion=true,.layer=0,.width=.45f,.height=.2f};view.layer=0;view.yaw=1.5707963f;view.pitch=0;view.zoom=GCODE_VIEW_ZOOM_MAX;
 for(unsigned i=0;i<N;i++){pixels[i+1]=0x0863;depth[i+1]=-INFINITY;}mesh_preview_render_layer_step(m,&paths,&view,pixels+1,depth+1,W,H,0,SIZE_MAX,NULL);int minx=W,miny=H,maxx=-1,maxy=-1;unsigned end_pixels=0;for(int y=0;y<H;y++)for(int x=0;x<W;x++)if(pixels[1+y*W+x]!=0x0863){end_pixels++;if(x<minx)minx=x;if(x>maxx)maxx=x;if(y<miny)miny=y;if(y>maxy)maxy=y;}assert(end_pixels>20&&maxx-minx>5&&maxy-miny<(maxx-minx)*.65f);
 travel[0].height=.8f;for(unsigned i=0;i<N;i++){pixels[i+1]=0x0863;depth[i+1]=-INFINITY;}mesh_preview_render_layer_step(m,&paths,&view,pixels+1,depth+1,W,H,0,SIZE_MAX,NULL);unsigned taller=0;for(unsigned i=0;i<N;i++)if(pixels[i+1]!=0x0863)taller++;assert(taller>end_pixels*2);assert(pixels[0]==0x1234&&pixels[N+1]==0xabcd&&depth[0]==12345&&depth[N+1]==54321);
 /* At whole-model fit, unresolved interior material must differ from flat Lines.
  * Its screen-sized cross-section is gold and shaded, with separate open gaps. */
 mesh_preview_t broad={.min={-100,-100,0},.max={100,100,40}};
 paths.count=2;paths.layer_count=1;travel[0]=(gcode_segment_t){.a={-50,-15,20},.b={50,-15,20},.extrusion=true,.layer=0,.width=.45f,.height=.2f};travel[1]=travel[0];travel[1].a.y=travel[1].b.y=15;
 gcode_view_reset(&view);view.layer=0;view.yaw=0;view.pitch=1.5707963f;
 for(unsigned i=0;i<N;i++){pixels[i+1]=0x0863;depth[i+1]=-INFINITY;}mesh_preview_render_layer_step(&broad,&paths,&view,pixels+1,depth+1,W,H,0,SIZE_MAX,NULL);
 unsigned row_count=0;
 for(int y=0;y<H;y++){uint16_t color=pixels[1+y*W+W/2];if(color!=0x0863){row_count++;assert((color>>11)>(color&31)*2);}}
 assert(row_count>=4&&pixels[1+H/2*W+W/2]==0x0863);
 /* Half-resolution staging scales the display-thickness aid rather than
  * doubling it when the frame is expanded for the tablet. */
 view.raster_ratio=.5f;for(unsigned i=0;i<N;i++){pixels[i+1]=0x0863;depth[i+1]=-INFINITY;}mesh_preview_render_layer_step(&broad,&paths,&view,pixels+1,depth+1,W/2,H/2,0,SIZE_MAX,NULL);
 unsigned half_rows=0;for(int y=0;y<H/2;y++)if(pixels[1+y*(W/2)+W/4]!=0x0863)half_rows++;assert(half_rows>0&&half_rows*2<=row_count+2);
 assert(pixels[0]==0x1234&&pixels[N+1]==0xabcd&&depth[0]==12345&&depth[N+1]==54321);
 /* Dense top skin should be one level surface, not scalloped separate ropes.
  * Use physical dimensions above the display floor so this exercises shape. */
 mesh_preview_t roof={.min={-5,-5,0},.max={5,5,2}};gcode_segment_t skin[7];
 for(unsigned i=0;i<7;i++)skin[i]=(gcode_segment_t){.a={-4,-1.26f+i*.42f,1},.b={4,-1.26f+i*.42f,1},.extrusion=true,.layer=0,.width=.45f,.height=.2f};
 paths.segments=skin;paths.count=paths.capacity=7;paths.sample_stride=1;paths.layer_anchor[0]=skin[0];gcode_view_reset(&view);view.layer=0;view.yaw=0;view.pitch=1.5707963f;view.zoom=2;
 for(unsigned i=0;i<N;i++){pixels[i+1]=0x0863;depth[i+1]=-INFINITY;}mesh_preview_render_layer_step(&roof,&paths,&view,pixels+1,depth+1,W,H,0,SIZE_MAX,NULL);
 uint16_t top_color=pixels[1+H/2*W+W/2];float top_depth=depth[1+H/2*W+W/2];assert(top_color!=0x0863);
 for(int y=H/2-18;y<=H/2+18;y++)for(int x=W/2-30;x<=W/2+30;x++){assert(pixels[1+y*W+x]==top_color);assert(fabsf(depth[1+y*W+x]-top_depth)<.001f);}
 memcpy(reference,pixels+1,sizeof(reference));for(unsigned i=0;i<N;i++){pixels[i+1]=0x0863;depth[i+1]=-INFINITY;}cursor=0;do{cursor=mesh_preview_render_layer_step(&roof,&paths,&view,pixels+1,depth+1,W,H,cursor,1,NULL);}while(cursor);assert(!memcmp(reference,pixels+1,sizeof(reference)));
 /* Sparse paths retain the empty interior; tilted faces retain real shading. */
 paths.count=2;skin[1]=skin[6];paths.layer_anchor[0]=skin[0];
 for(unsigned i=0;i<N;i++){pixels[i+1]=0x0863;depth[i+1]=-INFINITY;}mesh_preview_render_layer_step(&roof,&paths,&view,pixels+1,depth+1,W,H,0,SIZE_MAX,NULL);assert(pixels[1+H/2*W+W/2]==0x0863);
 view.pitch=.55f;for(unsigned i=0;i<N;i++){pixels[i+1]=0x0863;depth[i+1]=-INFINITY;}mesh_preview_render_layer_step(&roof,&paths,&view,pixels+1,depth+1,W,H,0,SIZE_MAX,NULL);uint16_t color0=0;bool face_shading=false;for(unsigned i=0;i<N;i++)if(pixels[i+1]!=0x0863){if(color0&&pixels[i+1]!=color0)face_shading=true;color0=pixels[i+1];}assert(face_shading);
 assert(pixels[0]==0x1234&&pixels[N+1]==0xabcd&&depth[0]==12345&&depth[N+1]==54321);
 /* Optional gesture mesh must leave full-detail geometry and its camera bounds intact. */
 bool stopped=true;assert(!mesh_preview_prepare_interaction(m,allocate,release,cancel,&stopped)&&!m->interaction&&allocations==1);stopped=false;fail_alloc=true;assert(!mesh_preview_prepare_interaction(m,allocate,release,cancel,&stopped)&&!m->interaction&&allocations==1);fail_alloc=false;
 for(unsigned i=1;i<=2;i++){attempts=0;fail_at=i;assert(!mesh_preview_prepare_interaction(m,allocate,release,cancel,&stopped)&&!m->interaction&&allocations==1);}fail_at=0;
 cancel_after=3;assert(!mesh_preview_prepare_interaction(m,allocate,release,cancel_lod,NULL)&&!m->interaction&&allocations==1);
 gcode_view_reset(&view);mesh_preview_render_step(m,&view,pixels+1,depth+1,W,H,0,SIZE_MAX);memcpy(reference,pixels+1,sizeof(reference));
 assert(mesh_preview_prepare_interaction(m,allocate,release,cancel,&stopped)&&m->interaction&&allocations==2);assert(m->interaction->radius==m->radius&&m->interaction->triangles<=4096&&m->interaction->vertices<=4096&&m->triangles==6144);assert(!memcmp(m->min,m->interaction->min,sizeof(m->min))&&!memcmp(m->max,m->interaction->max,sizeof(m->max)));
 mesh_preview_render_step(m->interaction,&view,pixels+1,depth+1,W/2,H/2,0,SIZE_MAX);mesh_preview_render_step(m,&view,pixels+1,depth+1,W,H,0,SIZE_MAX);assert(!memcmp(reference,pixels+1,sizeof(reference)));
 mesh_preview_render_edges_step(m,&view,pixels+1,depth+1,1,1,0,SIZE_MAX);mesh_preview_free(m,release);assert(!allocations);
 assert(!read_blob(blob,size,true)&&!allocations);fail_alloc=true;assert(!read_blob(blob,size,false)&&!allocations);fail_alloc=false;
 for(unsigned i=0;i<80;i++)assert(!read_blob(blob,i,false));
 assert(!read_blob(blob,size-1,false));blob[size]=0;assert(!read_blob(blob,size+1,false));
 memcpy(changed,blob,size);changed[100]^=1;assert(!read_blob(changed,size,false)&&!allocations);
 memcpy(changed,blob,size);put(changed+16,UINT32_MAX);checksums(changed,size);assert(!read_blob(changed,size,false));
 memcpy(changed,blob,size);put(changed+32,0x7fc00000);checksums(changed,size);assert(!read_blob(changed,size,false));
 memcpy(changed,blob,size);memset(changed+86,0,6);checksums(changed,size);assert(!read_blob(changed,size,false)&&!allocations);
 memcpy(changed,blob,size);uint32_t nv=(uint32_t)changed[16]|((uint32_t)changed[17]<<8);changed[80+nv*12]=255;changed[81+nv*12]=255;checksums(changed,size);assert(!read_blob(changed,size,false)&&!allocations);
 assert(mesh_preview_name_valid("demo.phm")&&mesh_preview_name_valid("Model 1-2.phm"));assert(!mesh_preview_name_valid("../demo.phm")&&!mesh_preview_name_valid("/demo.phm")&&!mesh_preview_name_valid("demo.stl")&&!mesh_preview_name_valid(".hidden.phm"));
 free(blob);free(changed);puts("PASS: PHM decode/CRCs/count/index/normal/NaN/truncation/OOM/cancel guards; shaded geometry and hole preservation; incremental/full raster agreement; zoom/orbit/edge clipping and pixel/depth sentinels; bounded gesture mesh, cancellation/OOM, unchanged full-detail output, height clipping, projected damage coverage, angle-independent scale/gesture fit, travel overlay/depth/filtering, solid extrusion end faces/height, infill gaps/material bounds, 16x clipping and child cleanup");return 0;
}
