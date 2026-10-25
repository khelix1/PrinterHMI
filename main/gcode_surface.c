#include "gcode_surface.h"
#include <math.h>
#include <float.h>
#include <string.h>
static void canonical(gcode_segment_t *s){if(s->a.x>s->b.x||(s->a.x==s->b.x&&s->a.y>s->b.y)){gcode_point_t t=s->a;s->a=s->b;s->b=t;}}
static uint32_t hash(const gcode_segment_t *s){
    const float f[]={s->a.x,s->a.y,s->b.x,s->b.y};uint32_t h=2166136261u;
    for(unsigned i=0;i<4;i++){h^=(uint32_t)(int32_t)lroundf(f[i]*1000);h*=16777619u;}
    return (h^s->tool)&(GCODE_SURFACE_SLOTS-1);
}
static bool same(const gcode_panel_t *p,const gcode_segment_t *s){
    return p->tool==s->tool&&lroundf(p->ax*1000)==lroundf(s->a.x*1000)&&lroundf(p->ay*1000)==lroundf(s->a.y*1000)&&lroundf(p->bx*1000)==lroundf(s->b.x*1000)&&lroundf(p->by*1000)==lroundf(s->b.y*1000);
}
static void emit(struct gcode_surface *m,gcode_segment_t s){
    canonical(&s);uint32_t slot=hash(&s);
    for(unsigned probe=0;probe<GCODE_SURFACE_SLOTS;probe++,slot=(slot+1)&(GCODE_SURFACE_SLOTS-1)){
        uint32_t index=m->slots[slot];
        if(index){
            gcode_panel_t *p=&m->panels[index-1];if(!same(p,&s))continue;
            float dz=s.b.z-p->top;
            if(s.layer==(unsigned)p->last_layer+1&&dz>.001f&&dz<=.5f){p->top=s.b.z;p->last_layer=s.layer;m->merged++;return;}
            /* A gap, duplicate or descent starts another independent ribbon. */
        }
        if(m->count==m->capacity){m->overflow=true;return;}
        m->panels[m->count]=(gcode_panel_t){s.a.x,s.a.y,s.b.x,s.b.y,s.b.z-.2f,s.b.z,s.layer,s.layer,s.tool};
        m->slots[slot]=(uint32_t)++m->count;return;
    }
    m->overflow=true;
}
void gcode_surface_init(struct gcode_surface *m,gcode_panel_t *p,size_t n,uint32_t *slots){
    memset(m,0,sizeof(*m));m->panels=p;m->capacity=n<GCODE_SURFACE_PANELS_MAX?n:GCODE_SURFACE_PANELS_MAX;m->slots=slots;
    memset(slots,0,GCODE_SURFACE_SLOTS*sizeof(*slots));
}
void gcode_surface_finish(struct gcode_surface *m){if(m->pending&&!m->overflow)emit(m,m->tail);m->pending=false;}
void gcode_surface_feed(struct gcode_surface *m,const gcode_segment_t *s){
    if(!s->extrusion){gcode_surface_finish(m);return;}
    m->inputs++;if(m->overflow)return;
    if(fabsf(s->a.z-s->b.z)>.001f){gcode_surface_finish(m);m->nonplanar=true;return;}
    if(m->pending){
        gcode_segment_t *t=&m->tail;
        float dx=t->b.x-t->a.x,dy=t->b.y-t->a.y,ex=s->b.x-s->a.x,ey=s->b.y-s->a.y,len=hypotf(dx,dy);
        if(t->layer==s->layer&&t->tool==s->tool&&fabsf(t->b.x-s->a.x)<.0001f&&fabsf(t->b.y-s->a.y)<.0001f&&fabsf(t->b.z-s->a.z)<.0001f&&len>.0001f&&dx*ex+dy*ey>0&&fabsf(dx*ey-dy*ex)/len<.0001f){t->b=s->b;m->merged++;return;}
        gcode_surface_finish(m);
    }
    m->tail=*s;m->pending=true;
}
bool gcode_surface_usable(const struct gcode_surface *m){return m&&m->count&&!m->overflow&&!m->nonplanar&&!m->pending;}
typedef struct{float x,y,z;} projected_t;
static void triangle(uint16_t *pixels,float *depth,int w,int h,projected_t a,projected_t b,projected_t c,uint16_t color){
    float den=(b.y-c.y)*(a.x-c.x)+(c.x-b.x)*(a.y-c.y);if(fabsf(den)<.0001f)return;
    int y0=(int)floorf(fminf(a.y,fminf(b.y,c.y))),y1=(int)ceilf(fmaxf(a.y,fmaxf(b.y,c.y)));
    if(y0<0)y0=0;
    if(y1>=h)y1=h-1;
    projected_t edge[3]={a,b,c};
    for(int y=y0;y<=y1;y++){
        float left=FLT_MAX,right=-FLT_MAX;
        for(unsigned i=0;i<3;i++){
            projected_t p=edge[i],q=edge[(i+1)%3];float dy=q.y-p.y;
            if(fabsf(dy)<.00001f){if(fabsf(y-p.y)<.00001f){left=fminf(left,fminf(p.x,q.x));right=fmaxf(right,fmaxf(p.x,q.x));}continue;}
            float t=(y-p.y)/dy;if(t<0||t>1)continue;float x=p.x+t*(q.x-p.x);left=fminf(left,x);right=fmaxf(right,x);
        }
        if(left>right)continue;
        int x0=(int)floorf(left),x1=(int)ceilf(right);if(x0<0)x0=0;if(x1>=w)x1=w-1;
        for(int x=x0;x<=x1;x++){
            float u=((b.y-c.y)*(x-c.x)+(c.x-b.x)*(y-c.y))/den;
            float v=((c.y-a.y)*(x-c.x)+(a.x-c.x)*(y-c.y))/den;
            if(u<-.00001f||v<-.00001f||u+v>1.00001f)continue;
            float z=u*a.z+v*b.z+(1-u-v)*c.z;size_t index=(size_t)y*w+x;
            if(z>depth[index]){depth[index]=z;pixels[index]=color;}
        }
    }
}
size_t gcode_surface_render_step(const gcode_toolpath_t *m,const gcode_view_t *v,uint16_t *pixels,float *depth,int w,int h,size_t first,size_t budget){
    if(!pixels||!depth||!m||!v||w<1||h<1||w>2048||h>2048||!budget)return 0;
    if(!first)for(size_t i=0;i<(size_t)w*h;i++){pixels[i]=0x0863;depth[i]=-FLT_MAX;}
    if(!gcode_surface_usable(m->surface)||!isfinite(v->yaw)||!isfinite(v->pitch)||!isfinite(v->zoom)||!isfinite(v->pan_x)||!isfinite(v->pan_y))return 0;
    struct gcode_surface *s=m->surface;if(first>=s->count)return 0;
    float cx=(m->min.x+m->max.x)*.5f,cy=(m->min.y+m->max.y)*.5f,cz=(m->min.z+m->max.z)*.5f;
    float radius=sqrtf(powf(m->max.x-m->min.x,2)+powf(m->max.y-m->min.y,2)+powf(m->max.z-m->min.z,2))*.5f;
    float scale=.43f*(w<h?w:h)/fmaxf(radius,.1f)*fmaxf(.25f,fminf(v->zoom,GCODE_VIEW_ZOOM_MAX));
    float sy=sinf(v->yaw),co=cosf(v->yaw),sp=sinf(v->pitch),cp=cosf(v->pitch);
    size_t end=s->count-first>budget?first+budget:s->count;
    static const uint16_t colors[]={0x07ff,0xfd20,0x07e0,0xf81f,0x4bff,0xffe0,0xf800,0xffff};
    static const unsigned face[6][4]={{4,7,6,5},{0,1,2,3},{0,4,5,1},{1,5,6,2},{2,6,7,3},{3,7,4,0}};
    for(size_t i=first;i<end;i++){
        const gcode_panel_t *p=&s->panels[i];float top=p->top;
        if(v->layer>=0){if(p->first_layer>v->layer)continue;if(p->last_layer>v->layer)top=fminf(top,m->layer_z[v->layer]);}
        if(top<=p->bottom)continue;
        float dx=p->bx-p->ax,dy=p->by-p->ay,len=hypotf(dx,dy);if(len<.0001f)continue;
        float nx=-dy/len*.225f,ny=dx/len*.225f;
        gcode_point_t pt[8]={{p->ax+nx,p->ay+ny,p->bottom},{p->bx+nx,p->by+ny,p->bottom},{p->bx-nx,p->by-ny,p->bottom},{p->ax-nx,p->ay-ny,p->bottom},
            {p->ax+nx,p->ay+ny,top},{p->bx+nx,p->by+ny,top},{p->bx-nx,p->by-ny,top},{p->ax-nx,p->ay-ny,top}};
        projected_t q[8];for(unsigned j=0;j<8;j++){float x=pt[j].x-cx,y=pt[j].y-cy,z=pt[j].z-cz,along=x*sy+y*co;q[j]=(projected_t){w*(.5f+fmaxf(-32,fminf(32,v->pan_x)))+(x*co-y*sy)*scale,h*(.5f+fmaxf(-32,fminf(32,v->pan_y)))+(along*sp-z*cp)*scale,along*cp+z*sp};}
        for(unsigned f=0;f<6;f++){
            const unsigned *a=face[f];
            /* The winding test culls rear faces before filling triangles. */
            float cross=(q[a[1]].x-q[a[0]].x)*(q[a[2]].y-q[a[0]].y)-(q[a[1]].y-q[a[0]].y)*(q[a[2]].x-q[a[0]].x);
            if(cross>=0)continue;
            float light=f==0?.95f:f==1?.35f:(f==2||f==4)?.64f:.8f;
            uint16_t c=colors[p->tool%8],shade=(uint16_t)(((unsigned)((c>>11)*light)<<11)|((unsigned)(((c>>5)&63)*light)<<5)|(unsigned)((c&31)*light));
            triangle(pixels,depth,w,h,q[a[0]],q[a[1]],q[a[2]],shade);triangle(pixels,depth,w,h,q[a[0]],q[a[2]],q[a[3]],shade);
        }
    }
    return end<s->count?end:0;
}
