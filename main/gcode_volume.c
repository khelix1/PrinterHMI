#include "gcode_volume.h"
#include <math.h>
#include <float.h>
#include <string.h>
#define RUNS 32768u
#define SLOTS 131072u
struct gcode_volume {
    unsigned n[3];size_t cells,bit_bytes,capacity_bytes;
    float origin[3],cell;
    uint8_t *bits,*air;
    uint64_t *queue;uint32_t *slots,*keys;
    void *work;mesh_preview_t *mesh;
    gcode_volume_alloc allocate;gcode_volume_free release;
    gcode_volume_cancel cancelled;gcode_volume_progress progress;void *context;
    uint64_t operations;bool failed;
};
static bool stop(gcode_volume_t *v){return v->failed||(v->cancelled&&v->cancelled(v->context));}
static size_t index_of(gcode_volume_t *v,unsigned x,unsigned y,unsigned z){return x+(size_t)v->n[0]*(y+(size_t)v->n[1]*z);}
static bool bit(const uint8_t *p,size_t i){return (p[i>>3]&(1u<<(i&7)))!=0;}
static void set(uint8_t *p,size_t i){p[i>>3]|=(uint8_t)(1u<<(i&7));}
void gcode_volume_destroy(gcode_volume_t *v){if(v){v->release(v->work);v->release(v->mesh);v->release(v);}}
gcode_volume_t *gcode_volume_create(const gcode_toolpath_t *m,unsigned target,gcode_volume_alloc allocate,gcode_volume_free release,gcode_volume_cancel cancelled,gcode_volume_progress progress,void *context){
    if(!m||!m->extrusion_seen||target<32||target>512||!allocate||!release)return NULL;
    gcode_volume_t *v=allocate(sizeof(*v));if(!v)return NULL;memset(v,0,sizeof(*v));v->allocate=allocate;v->release=release;v->cancelled=cancelled;v->progress=progress;v->context=context;
    float min[3]={m->extrusion_min.x,m->extrusion_min.y,m->extrusion_min.z},max[3]={m->extrusion_max.x,m->extrusion_max.y,m->extrusion_max.z};
    float width=m->max_width>0?m->max_width:.45f,height=m->max_height>0?m->max_height:.2f;
    min[0]-=width*.5f;min[1]-=width*.5f;min[2]-=height;max[0]+=width*.5f;max[1]+=width*.5f;
    float span=fmaxf(max[0]-min[0],fmaxf(max[1]-min[1],max[2]-min[2]));v->cell=fmaxf(span/(target-4),.025f);
    v->cells=1;for(unsigned a=0;a<3;a++){if(!isfinite(min[a])||!isfinite(max[a])||max[a]<min[a]){gcode_volume_destroy(v);return NULL;}v->origin[a]=min[a]-2*v->cell;v->n[a]=(unsigned)ceilf((max[a]-min[a])/v->cell)+4;if(v->n[a]>target+1||v->n[a]<4){gcode_volume_destroy(v);return NULL;}v->cells*=v->n[a];}
    v->bit_bytes=(v->cells+7)/8;v->capacity_bytes=v->bit_bytes;
    size_t offset=RUNS*sizeof(uint64_t)+v->bit_bytes*2;offset=(offset+3)&~(size_t)3;
    v->work=allocate(offset+SLOTS*sizeof(uint32_t)+MESH_PREVIEW_VERTICES_MAX*sizeof(uint32_t));
    v->mesh=allocate(sizeof(mesh_preview_t)+MESH_PREVIEW_VERTICES_MAX*(sizeof(mesh_vertex_t)+sizeof(mesh_projected_t))+MESH_PREVIEW_TRIANGLES_MAX*sizeof(mesh_triangle_t));
    if(!v->work||!v->mesh){gcode_volume_destroy(v);return NULL;}memset(v->work,0,offset+SLOTS*sizeof(uint32_t));memset(v->mesh,0,sizeof(*v->mesh));
    v->queue=v->work;v->bits=(uint8_t *)(v->queue+RUNS);v->air=v->bits+v->bit_bytes;v->slots=(uint32_t *)((uint8_t *)v->work+offset);v->keys=v->slots+SLOTS;
    v->mesh->vertex=(mesh_vertex_t *)(v->mesh+1);v->mesh->projected=(mesh_projected_t *)(v->mesh->vertex+MESH_PREVIEW_VERTICES_MAX);v->mesh->triangle=(mesh_triangle_t *)(v->mesh->projected+MESH_PREVIEW_VERTICES_MAX);
    return v;
}
bool gcode_volume_feed(gcode_volume_t *v,const gcode_segment_t *s){
    if(!v||v->failed)return false;
    if(!s->extrusion)return true;
    float width=s->width>0?s->width:.45f,height=s->height>0?s->height:.2f;
    if(!isfinite(width)||!isfinite(height)||width>10||height>5||width<.05f||height<.01f){v->failed=true;return false;}
    float ax=(s->a.x-v->origin[0])/v->cell,ay=(s->a.y-v->origin[1])/v->cell,bx=(s->b.x-v->origin[0])/v->cell,by=(s->b.y-v->origin[1])/v->cell,dx=bx-ax,dy=by-ay,len2=dx*dx+dy*dy;
    float radius=width/(2*v->cell)+.707107f,extra=fabsf(s->b.z-s->a.z)*fminf(1,.707107f/fmaxf(sqrtf(len2),.00001f));
    int y0=(int)floorf(fminf(ay,by)-radius),y1=(int)ceilf(fmaxf(ay,by)+radius);if(y0<0)y0=0;if(y1>=(int)v->n[1])y1=(int)v->n[1]-1;
    for(int y=y0;y<=y1;y++){
        float py=y+.5f,lo=0,hi=1;if(fabsf(dy)>.00001f){float a=(py-radius-ay)/dy,b=(py+radius-ay)/dy;lo=fmaxf(0,fminf(a,b));hi=fminf(1,fmaxf(a,b));if(lo>hi)continue;}else if(fabsf(py-ay)>radius)continue;
        int x0=(int)floorf(fminf(ax+dx*lo,ax+dx*hi)-radius),x1=(int)ceilf(fmaxf(ax+dx*lo,ax+dx*hi)+radius);if(x0<0)x0=0;if(x1>=(int)v->n[0])x1=(int)v->n[0]-1;
        for(int x=x0;x<=x1;x++){
            if(++v->operations>1000000000ull||((v->operations&4095)==0&&stop(v))){v->failed=true;return false;}
            float px=x+.5f,t=len2>.00001f?fmaxf(0,fminf(1,((px-ax)*dx+(py-ay)*dy)/len2)):0,nx=px-ax-t*dx,ny=py-ay-t*dy;if(nx*nx+ny*ny>radius*radius)continue;
            float z=s->a.z+(s->b.z-s->a.z)*t;
            int z0=(int)floorf((fmaxf(z-height-extra,fminf(s->a.z,s->b.z)-height)-v->origin[2])/v->cell),z1=(int)floorf((fminf(z+extra,fmaxf(s->a.z,s->b.z))-v->origin[2])/v->cell);if(z0<0)z0=0;if(z1>=(int)v->n[2])z1=(int)v->n[2]-1;
            for(int zi=z0;zi<=z1;zi++)set(v->bits,index_of(v,(unsigned)x,(unsigned)y,(unsigned)zi));
        }
    }
    return true;
}
static bool air_run(gcode_volume_t *v,unsigned x,unsigned y,unsigned z,size_t *head,size_t *tail){
    size_t i=index_of(v,x,y,z);if(bit(v->bits,i)||bit(v->air,i))return true;unsigned left=x,right=x;
    while(left&& !bit(v->bits,index_of(v,left-1,y,z))&&!bit(v->air,index_of(v,left-1,y,z)))left--;
    while(right+1<v->n[0]&&!bit(v->bits,index_of(v,right+1,y,z))&&!bit(v->air,index_of(v,right+1,y,z)))right++;
    if(*tail-*head==RUNS)return false;
    for(unsigned a=left;a<=right;a++)set(v->air,index_of(v,a,y,z));
    v->queue[(*tail)++%RUNS]=left|((uint64_t)right<<10)|((uint64_t)y<<20)|((uint64_t)z<<30);return true;
}
static bool outside(gcode_volume_t *v){
    memset(v->air,0,v->capacity_bytes);size_t head=0,tail=0;if(!air_run(v,0,0,0,&head,&tail))return false;
    while(head<tail){
        if((head&255)==0&&stop(v))return false;
        uint64_t run=v->queue[head++%RUNS];unsigned left=run&1023,right=(run>>10)&1023,y=(run>>20)&1023,z=(run>>30)&1023;
        for(unsigned x=left;x<=right;x++){
            if(y&&!air_run(v,x,y-1,z,&head,&tail))return false;
            if(y+1<v->n[1]&&!air_run(v,x,y+1,z,&head,&tail))return false;
            if(z&&!air_run(v,x,y,z-1,&head,&tail))return false;
            if(z+1<v->n[2]&&!air_run(v,x,y,z+1,&head,&tail))return false;
        }
    }
    return true;
}
static uint32_t cell_key(unsigned x,unsigned y,unsigned z){return x|(y<<10)|(z<<20);}
static unsigned lookup(gcode_volume_t *v,unsigned x,unsigned y,unsigned z){
    uint32_t key=cell_key(x,y,z),slot=(key*2654435761u)&(SLOTS-1);
    while(v->slots[slot]){unsigned i=v->slots[slot]-1;if(v->keys[i]==key)return i;slot=(slot+1)&(SLOTS-1);}return UINT32_MAX;
}
static unsigned vertex(gcode_volume_t *v,unsigned x,unsigned y,unsigned z,unsigned mask){
    mesh_preview_t *m=v->mesh;if(m->vertices==MESH_PREVIEW_VERTICES_MAX)return UINT32_MAX;
    static const unsigned edges[12][2]={{0,1},{2,3},{4,5},{6,7},{0,2},{1,3},{4,6},{5,7},{0,4},{1,5},{2,6},{3,7}};
    float position[3]={0};unsigned crossings=0;
    for(unsigned e=0;e<12;e++){unsigned a=edges[e][0],b=edges[e][1];if(((mask>>a)&1)==((mask>>b)&1))continue;crossings++;for(unsigned axis=0;axis<3;axis++)position[axis]+=((a>>axis)&1)*.5f+((b>>axis)&1)*.5f;}
    float normal[3]={0};for(unsigned c=0;c<8;c++)if(mask&(1u<<c))for(unsigned a=0;a<3;a++)normal[a]+=(c&(1u<<a))?-1.f:1.f;
    float length=sqrtf(normal[0]*normal[0]+normal[1]*normal[1]+normal[2]*normal[2]);if(length<.001f){normal[2]=1;length=1;}
    unsigned i=m->vertices++;m->vertex[i]=(mesh_vertex_t){v->origin[0]+(x+.5f+position[0]/crossings)*v->cell,v->origin[1]+(y+.5f+position[1]/crossings)*v->cell,v->origin[2]+(z+.5f+position[2]/crossings)*v->cell,normal[0]/length,normal[1]/length,normal[2]/length};
    uint32_t key=cell_key(x,y,z),slot=(key*2654435761u)&(SLOTS-1);while(v->slots[slot])slot=(slot+1)&(SLOTS-1);v->slots[slot]=i+1;v->keys[i]=key;return i;
}
static bool connect(gcode_volume_t *v,unsigned axis,unsigned x,unsigned y,unsigned z,unsigned current,bool reverse){
    unsigned ua=(axis+1)%3,va=(axis+2)%3,p[4]={current,0,0,0},xyz[3]={x,y,z};if(!xyz[ua]||!xyz[va])return true;
    xyz[ua]--;p[1]=lookup(v,xyz[0],xyz[1],xyz[2]);xyz[va]--;p[2]=lookup(v,xyz[0],xyz[1],xyz[2]);xyz[ua]++;p[3]=lookup(v,xyz[0],xyz[1],xyz[2]);
    if(p[1]==UINT32_MAX||p[2]==UINT32_MAX||p[3]==UINT32_MAX)return false;
    mesh_preview_t *m=v->mesh;if(m->triangles+2>MESH_PREVIEW_TRIANGLES_MAX)return false;
    if(reverse){unsigned tmp=p[1];p[1]=p[3];p[3]=tmp;}
    m->triangle[m->triangles++]=(mesh_triangle_t){p[0],p[1],p[2]};m->triangle[m->triangles++]=(mesh_triangle_t){p[0],p[2],p[3]};return true;
}
static unsigned pair(gcode_volume_t *v,size_t i){unsigned value=v->air[i>>3];if((i&7)==7)value|=(unsigned)v->air[(i>>3)+1]<<8;return (value>>(i&7))&3;}
static bool surface(gcode_volume_t *v){
    mesh_preview_t *m=v->mesh;m->vertices=m->triangles=0;memset(v->slots,0,SLOTS*sizeof(uint32_t));
    for(unsigned z=0;z+1<v->n[2];z++){
        if(stop(v))return false;
        if(v->progress)v->progress(v->context,70+29*z/v->n[2],v->cell);
        for(unsigned y=0;y+1<v->n[1];y++)for(unsigned x=0;x+1<v->n[0];x++){
            size_t i=index_of(v,x,y,z),row=v->n[0],plane=(size_t)v->n[0]*v->n[1];
            unsigned air=pair(v,i)|(pair(v,i+row)<<2)|(pair(v,i+plane)<<4)|(pair(v,i+plane+row)<<6),mask=(~air)&255;if(!mask||mask==255)continue;
            unsigned vertex_id=vertex(v,x,y,z,mask);if(vertex_id==UINT32_MAX)return false;
            for(unsigned axis=0;axis<3;axis++)if((mask&1)!=((mask>>(1u<<axis))&1))if(!connect(v,axis,x,y,z,vertex_id,(mask&1)==0))return false;
        }
    }
    return m->triangles>0;
}
static bool coarsen(gcode_volume_t *v){
    unsigned old[3]={v->n[0],v->n[1],v->n[2]},n[3];for(unsigned a=0;a<3;a++)n[a]=(old[a]+1)/2+2;
    size_t count=(size_t)n[0]*n[1]*n[2];if(count>=v->cells||fmaxf(old[0],fmaxf(old[1],old[2]))<32)return false;
    memset(v->air,0,v->capacity_bytes);
    for(unsigned z=0;z<old[2];z++){if(stop(v))return false;for(unsigned y=0;y<old[1];y++)for(unsigned x=0;x<old[0];x++)if(bit(v->bits,index_of(v,x,y,z)))set(v->air,(x/2+1)+(size_t)n[0]*((y/2+1)+(size_t)n[1]*(z/2+1)));}
    uint8_t *tmp=v->bits;v->bits=v->air;v->air=tmp;memcpy(v->n,n,sizeof(n));v->cells=count;v->bit_bytes=(count+7)/8;v->cell*=2;for(unsigned a=0;a<3;a++)v->origin[a]-=v->cell;return true;
}
static bool smooth(gcode_volume_t *v){
    mesh_preview_t *m=v->mesh;
    /* Keep smoothing local to the sampled cell; projected storage is scratch
     * until the UI begins rendering. Paired positive/negative steps reduce
     * voxel stair-steps without the large shrink of plain Laplacian smoothing. */
    for(unsigned i=0;i<m->vertices;i++){m->vertex[i].nx=m->vertex[i].x;m->vertex[i].ny=m->vertex[i].y;m->vertex[i].nz=m->vertex[i].z;}
    for(unsigned pass=0;pass<8;pass++){
        if(stop(v))return false;
        memset(m->projected,0,(size_t)m->vertices*sizeof(mesh_projected_t));
        for(unsigned i=0;i<m->triangles;i++){
            if((i&8191)==0&&stop(v))return false;
            mesh_triangle_t t=m->triangle[i];unsigned p[3]={t.a,t.b,t.c};
            for(unsigned a=0;a<3;a++)for(unsigned b=0;b<3;b++)if(a!=b){mesh_projected_t *sum=&m->projected[p[a]];mesh_vertex_t q=m->vertex[p[b]];sum->x+=q.x;sum->y+=q.y;sum->z+=q.z;sum->light++;}
        }
        float factor=pass%2?-.47f:.45f;
        for(unsigned i=0;i<m->vertices;i++){mesh_vertex_t *p=&m->vertex[i];mesh_projected_t sum=m->projected[i];if(sum.light<1)continue;p->x=fmaxf(p->nx-.75f*v->cell,fminf(p->nx+.75f*v->cell,p->x+factor*(sum.x/sum.light-p->x)));p->y=fmaxf(p->ny-.75f*v->cell,fminf(p->ny+.75f*v->cell,p->y+factor*(sum.y/sum.light-p->y)));p->z=fmaxf(p->nz-.75f*v->cell,fminf(p->nz+.75f*v->cell,p->z+factor*(sum.z/sum.light-p->z)));}
    }
    for(unsigned i=0;i<m->vertices;i++)m->vertex[i].nx=m->vertex[i].ny=m->vertex[i].nz=0;
    for(unsigned i=0;i<m->triangles;i++){
        mesh_triangle_t t=m->triangle[i];mesh_vertex_t a=m->vertex[t.a],b=m->vertex[t.b],c=m->vertex[t.c];float x=b.x-a.x,y=b.y-a.y,z=b.z-a.z,dx=c.x-a.x,dy=c.y-a.y,dz=c.z-a.z,nx=y*dz-z*dy,ny=z*dx-x*dz,nz=x*dy-y*dx;unsigned ids[3]={t.a,t.b,t.c};
        for(unsigned j=0;j<3;j++){m->vertex[ids[j]].nx+=nx;m->vertex[ids[j]].ny+=ny;m->vertex[ids[j]].nz+=nz;}
    }
    for(unsigned i=0;i<m->vertices;i++){mesh_vertex_t *p=&m->vertex[i];float length=sqrtf(p->nx*p->nx+p->ny*p->ny+p->nz*p->nz);if(length<1e-12f){p->nz=1;p->nx=p->ny=0;}else{p->nx/=length;p->ny/=length;p->nz/=length;}}
    return true;
}
mesh_preview_t *gcode_volume_finish(gcode_volume_t *v){
    if(!v)return NULL;
    bool good=false;for(unsigned attempt=0;attempt<5&&!stop(v);attempt++){if(v->progress)v->progress(v->context,55,v->cell);if(outside(v)&&surface(v)){good=true;break;}if(!coarsen(v))break;}
    if(!good||!smooth(v)){gcode_volume_destroy(v);return NULL;}mesh_preview_t *m=v->mesh;
    for(unsigned a=0;a<3;a++){m->min[a]=FLT_MAX;m->max[a]=-FLT_MAX;}
    for(unsigned i=0;i<m->vertices;i++){float p[3]={m->vertex[i].x,m->vertex[i].y,m->vertex[i].z};for(unsigned a=0;a<3;a++){m->min[a]=fminf(m->min[a],p[a]);m->max[a]=fmaxf(m->max[a],p[a]);}}
    mesh_preview_update_radius(m);
    m->cell_mm=v->cell;m->cull_backfaces=true;v->release(v->work);v->work=NULL;
    mesh_preview_t *small=v->allocate(sizeof(*small)+(size_t)m->vertices*(sizeof(mesh_vertex_t)+sizeof(mesh_projected_t))+(size_t)m->triangles*sizeof(mesh_triangle_t));
    if(small){*small=*m;small->vertex=(mesh_vertex_t *)(small+1);small->projected=(mesh_projected_t *)(small->vertex+m->vertices);small->triangle=(mesh_triangle_t *)(small->projected+m->vertices);memcpy(small->vertex,m->vertex,(size_t)m->vertices*sizeof(mesh_vertex_t));memcpy(small->triangle,m->triangle,(size_t)m->triangles*sizeof(mesh_triangle_t));v->release(m);m=small;}
    v->mesh=NULL;v->release(v);return m;
}
