#include "mesh_preview.h"
#include <float.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
_Static_assert(sizeof(float)==4,"PHM requires IEEE binary32 floats");
static uint16_t u16(const unsigned char *p){return (uint16_t)(p[0]|((uint16_t)p[1]<<8));}
static uint32_t u32(const unsigned char *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static float f32(const unsigned char *p){uint32_t n=u32(p);float f;memcpy(&f,&n,4);return f;}
static uint32_t crc(uint32_t v,const unsigned char *p,size_t n){while(n--){v^=*p++;for(unsigned j=0;j<8;j++)v=(v>>1)^((v&1)?0xedb88320u:0);}return v;}
bool mesh_preview_name_valid(const char *s){
    if(!s||!*s||s[0]=='.')return false;
    size_t n=strlen(s);if(n<5||n>80||strcmp(s+n-4,".phm"))return false;
    for(size_t i=0;i<n;i++){unsigned char c=(unsigned char)s[i];if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='.'||c==' '))return false;}return true;
}
mesh_preview_t *mesh_preview_read(FILE *f,void *(*allocate)(size_t),void (*release)(void *),bool (*cancelled)(void *),void *ctx,char *error,size_t size){
    unsigned char header[80],record[12];mesh_preview_t *m=NULL;const char *why="Invalid or incomplete PHM mesh.";
    if(!f||!allocate||!release)goto bad;
    if(fread(header,1,80,f)!=80||memcmp(header,"PHMESH1\0",8)||u32(header+8)!=1||u32(header+12)!=80||u32(header+24)!=1||u32(header+72)||u32(header+76))goto bad;
    uint32_t header_crc=u32(header+68);memset(header+68,0,4);if((crc(0xffffffffu,header,80)^0xffffffffu)!=header_crc){why="Mesh header checksum failed.";goto bad;}
    uint32_t nv=u32(header+16),nt=u32(header+20),bytes=u32(header+28),source=u32(header+60);
    if(!nv||nv>MESH_PREVIEW_VERTICES_MAX||!nt||nt>MESH_PREVIEW_TRIANGLES_MAX||bytes!=nv*12u+nt*6u||source<nt||source>2000000u)goto bad;
    if(fseek(f,0,SEEK_END)||ftell(f)!=(long)(80u+bytes)||fseek(f,80,SEEK_SET))goto bad;
    float min[3],max[3],cell=f32(header+64);
    if(!isfinite(cell)||cell<0||cell>2000000)goto bad;
    for(unsigned j=0;j<3;j++){min[j]=f32(header+32+j*4);max[j]=f32(header+44+j*4);if(!isfinite(min[j])||!isfinite(max[j])||fabsf(min[j])>1000000||fabsf(max[j])>1000000||max[j]<min[j])goto bad;}
    m=allocate(sizeof(*m)+(size_t)nv*(sizeof(mesh_vertex_t)+sizeof(mesh_projected_t))+(size_t)nt*sizeof(mesh_triangle_t));
    if(!m){why="Not enough PSRAM for this mesh.";goto bad;}
    memset(m,0,sizeof(*m));m->vertices=nv;m->triangles=nt;m->source_triangles=source;m->payload_bytes=bytes;m->cell_mm=cell;memcpy(m->min,min,sizeof(min));memcpy(m->max,max,sizeof(max));
    m->vertex=(mesh_vertex_t *)(m+1);m->projected=(mesh_projected_t *)(m->vertex+nv);m->triangle=(mesh_triangle_t *)(m->projected+nv);
    uint32_t checksum=0xffffffffu;
    for(uint32_t i=0;i<nv;i++){
        if(i%128==0&&cancelled&&cancelled(ctx)){why="Mesh loading cancelled.";goto bad;}
        if(fread(record,1,12,f)!=12)goto bad;
        checksum=crc(checksum,record,12);
        mesh_vertex_t *v=&m->vertex[i];v->x=min[0]+(max[0]-min[0])*(u16(record)/65535.0f);v->y=min[1]+(max[1]-min[1])*(u16(record+2)/65535.0f);v->z=min[2]+(max[2]-min[2])*(u16(record+4)/65535.0f);
        int16_t nx=(int16_t)u16(record+6),ny=(int16_t)u16(record+8),nz=(int16_t)u16(record+10);
        if(nx==INT16_MIN||ny==INT16_MIN||nz==INT16_MIN)goto bad;
        float length=sqrtf((float)nx*nx+(float)ny*ny+(float)nz*nz);if(length<1)goto bad;
        v->nx=nx/length;v->ny=ny/length;v->nz=nz/length;
    }
    for(uint32_t i=0;i<nt;i++){
        if(i%128==0&&cancelled&&cancelled(ctx)){why="Mesh loading cancelled.";goto bad;}
        if(fread(record,1,6,f)!=6)goto bad;
        checksum=crc(checksum,record,6);
        mesh_triangle_t t={u16(record),u16(record+2),u16(record+4)};if(t.a>=nv||t.b>=nv||t.c>=nv||t.a==t.b||t.b==t.c||t.a==t.c)goto bad;
        mesh_vertex_t a=m->vertex[t.a],b=m->vertex[t.b],c=m->vertex[t.c];float x=b.x-a.x,y=b.y-a.y,z=b.z-a.z,dx=c.x-a.x,dy=c.y-a.y,dz=c.z-a.z;
        if(fabsf(y*dz-z*dy)+fabsf(z*dx-x*dz)+fabsf(x*dy-y*dx)<1e-12f)goto bad;
        m->triangle[i]=t;
    }
    if((checksum^0xffffffffu)!=u32(header+56)){why="Mesh payload checksum failed.";goto bad;}
    mesh_preview_update_radius(m);
    if(error&&size)error[0]=0;
    return m;
bad:
    if(m)release(m);
    if(error&&size)snprintf(error,size,"%s",why);
    return NULL;
}
void mesh_preview_update_radius(mesh_preview_t *m){
    if(!m)return;
    float cx=(m->min[0]+m->max[0])*.5f,cy=(m->min[1]+m->max[1])*.5f,cz=(m->min[2]+m->max[2])*.5f,r2=0;
    for(unsigned i=0;i<m->vertices;i++){mesh_vertex_t p=m->vertex[i];float x=p.x-cx,y=p.y-cy,z=p.z-cz;r2=fmaxf(r2,x*x+y*y+z*z);}
    m->radius=sqrtf(r2);
}
typedef struct{float cx,cy,cz,scale,sy,co,sp,cp;int w,h;float ox,oy;} camera_t;
static camera_t camera(const mesh_preview_t *m,const gcode_view_t *v,int w,int h){
    camera_t c={(m->min[0]+m->max[0])*.5f,(m->min[1]+m->max[1])*.5f,(m->min[2]+m->max[2])*.5f,1,sinf(v->yaw),cosf(v->yaw),sinf(v->pitch),cosf(v->pitch),w,h,0,0};
    c.ox=w*(.5f+fmaxf(-32,fminf(32,v->pan_x)));c.oy=h*(.5f+fmaxf(-32,fminf(32,v->pan_y)));
    float radius=m->radius;
    if(!isfinite(radius)||radius<=0){float x=m->max[0]-m->min[0],y=m->max[1]-m->min[1],z=m->max[2]-m->min[2];radius=.5f*sqrtf(x*x+y*y+z*z);}
    /* Rotation changes projection only. Pinch/buttons own the zoom factor. */
    c.scale=.43f*(w<h?w:h)/fmaxf(radius,.1f)*fmaxf(.25f,fminf(v->zoom,GCODE_VIEW_ZOOM_MAX));return c;
}
bool mesh_preview_screen_bounds(const mesh_preview_t *m,const gcode_view_t *v,int w,int h,int bounds[4]){
    if(!m||!v||!bounds||w<1||h<1||!isfinite(v->yaw)||!isfinite(v->pitch)||!isfinite(v->zoom)||!isfinite(v->pan_x)||!isfinite(v->pan_y))return false;
    camera_t c=camera(m,v,w,h);float lowx=FLT_MAX,lowy=FLT_MAX,highx=-FLT_MAX,highy=-FLT_MAX;
    for(unsigned i=0;i<8;i++){float x=(i&1?m->max[0]:m->min[0])-c.cx,y=(i&2?m->max[1]:m->min[1])-c.cy,z=(i&4?m->max[2]:m->min[2])-c.cz;float px=c.ox+(x*c.co-y*c.sy)*c.scale,py=c.oy+((x*c.sy+y*c.co)*c.sp-z*c.cp)*c.scale;lowx=fminf(lowx,px);highx=fmaxf(highx,px);lowy=fminf(lowy,py);highy=fmaxf(highy,py);}
    bounds[0]=(int)fmaxf(0,fminf(w-1,floorf(lowx)-2));bounds[1]=(int)fmaxf(0,fminf(h-1,floorf(lowy)-2));bounds[2]=(int)fmaxf(0,fminf(w-1,ceilf(highx)+2));bounds[3]=(int)fmaxf(0,fminf(h-1,ceilf(highy)+2));return true;
}
static mesh_projected_t project(mesh_vertex_t v,camera_t c){
    float x=v.x-c.cx,y=v.y-c.cy,z=v.z-c.cz,along=x*c.sy+y*c.co;
    float nx=v.nx*c.co-v.ny*c.sy,ny=(v.nx*c.sy+v.ny*c.co)*c.sp-v.nz*c.cp,nz=(v.nx*c.sy+v.ny*c.co)*c.cp+v.nz*c.sp;
    if(nz<0){nx=-nx;ny=-ny;nz=-nz;}
    float diffuse=fmaxf(0,-.35f*nx-.50f*ny+.79f*nz),spec=fmaxf(0,-.19f*nx-.27f*ny+.94f*nz);spec*=spec;spec*=spec;spec*=spec;spec*=spec;spec*=.12f;
    return (mesh_projected_t){c.ox+(x*c.co-y*c.sy)*c.scale,c.oy+(along*c.sp-z*c.cp)*c.scale,along*c.cp+z*c.sp,fminf(1,.23f+.70f*diffuse+spec)};
}
static const uint16_t shades[256]={
    0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0020,0x0020,0x0020,0x0820,0x0820,0x0820,0x0840,0x0840,0x0840,0x0840,
    0x0840,0x1040,0x1060,0x1060,0x1060,0x1060,0x1060,0x1060,0x1080,0x1880,0x1880,0x1880,0x1880,0x1881,0x18a1,0x18a1,
    0x18a1,0x20a1,0x20a1,0x20a1,0x20c1,0x20c1,0x20c1,0x20c1,0x20c1,0x20c1,0x28e1,0x28e1,0x28e1,0x28e1,0x28e1,0x28e1,
    0x2901,0x2901,0x3101,0x3101,0x3101,0x3101,0x3121,0x3121,0x3121,0x3122,0x3922,0x3922,0x3942,0x3942,0x3942,0x3942,
    0x3942,0x3942,0x4162,0x4162,0x4162,0x4162,0x4162,0x4162,0x4182,0x4182,0x4182,0x4982,0x4982,0x4982,0x49a2,0x49a2,
    0x49a2,0x49a2,0x49a2,0x51a2,0x51c2,0x51c3,0x51c3,0x51c3,0x51c3,0x51e3,0x51e3,0x59e3,0x59e3,0x59e3,0x59e3,0x5a03,
    0x5a03,0x5a03,0x5a03,0x6203,0x6203,0x6223,0x6223,0x6223,0x6223,0x6223,0x6223,0x6a43,0x6a43,0x6a43,0x6a43,0x6a43,
    0x6a43,0x6a63,0x6a64,0x6a64,0x7264,0x7264,0x7264,0x7284,0x7284,0x7284,0x7284,0x7284,0x7a84,0x7aa4,0x7aa4,0x7aa4,
    0x7aa4,0x7aa4,0x7aa4,0x7ac4,0x82c4,0x82c4,0x82c4,0x82c4,0x82c4,0x82e4,0x82e4,0x82e4,0x8ae4,0x8ae4,0x8ae5,0x8b05,
    0x8b05,0x8b05,0x8b05,0x8b05,0x8b05,0x9325,0x9325,0x9325,0x9325,0x9325,0x9325,0x9345,0x9345,0x9b45,0x9b45,0x9b45,
    0x9b45,0x9b65,0x9b65,0x9b65,0x9b65,0xa365,0xa365,0xa385,0xa385,0xa385,0xa386,0xa386,0xa3a6,0xaba6,0xaba6,0xaba6,
    0xaba6,0xaba6,0xabc6,0xabc6,0xabc6,0xb3c6,0xb3c6,0xb3c6,0xb3e6,0xb3e6,0xb3e6,0xb3e6,0xb3e6,0xb3e6,0xbc06,0xbc06,
    0xbc06,0xbc06,0xbc06,0xbc06,0xbc26,0xbc26,0xc426,0xc427,0xc427,0xc427,0xc447,0xc447,0xc447,0xc447,0xcc47,0xcc47,
    0xcc67,0xcc67,0xcc67,0xcc67,0xcc67,0xcc67,0xd487,0xd487,0xd487,0xd487,0xd487,0xd487,0xd4a7,0xd4a7,0xd4a7,0xdca7,
    0xdca7,0xdca7,0xdcc7,0xdcc8,0xdcc8,0xdcc8,0xdcc8,0xe4c8,0xe4e8,0xe4e8,0xe4e8,0xe4e8,0xe4e8,0xe4e8,0xe508,0xed08,
    0xed08,0xed08,0xed08,0xed08,0xed28,0xed28,0xed28,0xf528,0xf528,0xf528,0xf548,0xf548,0xf548,0xf548,0xf548,0xfd69,
};
/* One reciprocal per triangle; barycentric, depth and light gradients are
 * stepped across scanlines. No division or RGB conversion in the pixel loop. */
static void triangle(uint16_t *pixels,float *depth,int w,int h,mesh_projected_t a,mesh_projected_t b,mesh_projected_t c,bool cull,const uint16_t *palette){
    float den=(b.y-c.y)*(a.x-c.x)+(c.x-b.x)*(a.y-c.y);if(fabsf(den)<.0001f||(cull&&den<=0))return;
    float reciprocal=1/den,ux=(b.y-c.y)*reciprocal,uy=(c.x-b.x)*reciprocal,vx=(c.y-a.y)*reciprocal,vy=(a.x-c.x)*reciprocal;
    float dz=ux*(a.z-c.z)+vx*(b.z-c.z),dl=ux*(a.light-c.light)+vx*(b.light-c.light);
    int y0=(int)ceilf(fminf(a.y,fminf(b.y,c.y))),y1=(int)floorf(fmaxf(a.y,fmaxf(b.y,c.y)));if(y0<0)y0=0;if(y1>=h)y1=h-1;
    mesh_projected_t edge[3]={a,b,c};float slope[3],low[3],high[3];
    for(unsigned i=0;i<3;i++){mesh_projected_t p=edge[i],q=edge[(i+1)%3];float dy=q.y-p.y;slope[i]=fabsf(dy)<.00001f?0:(q.x-p.x)/dy;low[i]=fminf(p.y,q.y);high[i]=fmaxf(p.y,q.y);}
    for(int y=y0;y<=y1;y++){
        float left=FLT_MAX,right=-FLT_MAX;
        for(unsigned i=0;i<3;i++){
            if(y<low[i]||y>high[i])continue;
            mesh_projected_t p=edge[i],q=edge[(i+1)%3];
            if(high[i]-low[i]<.00001f){left=fminf(left,fminf(p.x,q.x));right=fmaxf(right,fmaxf(p.x,q.x));}
            else{float x=p.x+(y-p.y)*slope[i];left=fminf(left,x);right=fmaxf(right,x);}
        }
        if(left>right)continue;
        int x0=(int)ceilf(left),x1=(int)floorf(right);if(x0<0)x0=0;if(x1>=w)x1=w-1;
        float u=ux*(x0-c.x)+uy*(y-c.y),v=vx*(x0-c.x)+vy*(y-c.y);
        float z=c.z+u*(a.z-c.z)+v*(b.z-c.z),light=c.light+u*(a.light-c.light)+v*(b.light-c.light);
        size_t index=(size_t)y*w+x0;
        for(int x=x0;x<=x1;x++,index++,u+=ux,v+=vx,z+=dz,light+=dl){
            if(u<-.00001f||v<-.00001f||u+v>1.00001f||z<=depth[index])continue;
            depth[index]=z;int shade=(int)(light*255);if(shade<0)shade=0;if(shade>255)shade=255;pixels[index]=palette[shade];
        }
    }
}
static mesh_projected_t interpolate(mesh_projected_t a,mesh_projected_t b,float t){return (mesh_projected_t){a.x+t*(b.x-a.x),a.y+t*(b.y-a.y),a.z+t*(b.z-a.z),a.light+t*(b.light-a.light)};}
static void cap_edge(uint32_t *mask,int w,int h,mesh_projected_t a,mesh_projected_t b){
    if(!mask||fabsf(a.y-b.y)<.00001f)return;
    if(a.y>b.y){mesh_projected_t swap=a;a=b;b=swap;}
    int y0=(int)ceilf(a.y),y1=(int)ceilf(b.y)-1;if(y0<0)y0=0;if(y1>=h)y1=h-1;unsigned words=(w+31)/32;float slope=(b.x-a.x)/(b.y-a.y);
    for(int y=y0;y<=y1;y++){int x=(int)ceilf(a.x+(y-a.y)*slope);if(x<0)x=0;if(x>=w)continue;unsigned word=(unsigned)x/32;uint32_t *row=mask+(size_t)y*words;row[word]^=UINT32_MAX<<((unsigned)x&31);for(word++;word<words;word++)row[word]^=UINT32_MAX;}
}
typedef struct{mesh_projected_t q;float z;} cut_vertex_t;
static unsigned clip_plane(const cut_vertex_t *in,unsigned count,cut_vertex_t out[6],float cut,bool below,mesh_projected_t crossing[2],unsigned *crossings){
    *crossings=0;if(!count)return 0;unsigned used=0;cut_vertex_t previous=in[count-1];bool inside=below?previous.z<=cut:previous.z>=cut;
    for(unsigned i=0;i<count;i++){
        cut_vertex_t current=in[i];bool next=below?current.z<=cut:current.z>=cut;
        if(inside!=next){cut_vertex_t lo=previous.z<current.z?previous:current,hi=previous.z<current.z?current:previous;mesh_projected_t q=cut==lo.z?lo.q:cut==hi.z?hi.q:interpolate(lo.q,hi.q,(cut-lo.z)/(hi.z-lo.z));out[used++]=(cut_vertex_t){q,cut};if(*crossings<2)crossing[(*crossings)++]=q;}
        if(next)out[used++]=current;
        previous=current;inside=next;
    }
    return used;
}
static void clipped_triangle(mesh_preview_t *m,mesh_triangle_t t,float low,float cut,uint16_t *pixels,float *depth,int w,int h,uint32_t *cap){
    if((m->vertex[t.a].z>cut&&m->vertex[t.b].z>cut&&m->vertex[t.c].z>cut)||(m->vertex[t.a].z<low&&m->vertex[t.b].z<low&&m->vertex[t.c].z<low))return;
    unsigned ids[3]={t.a,t.b,t.c};cut_vertex_t input[6],upper[6],lower[6];mesh_projected_t crossing[2];unsigned crossings=0;
    for(unsigned i=0;i<3;i++)input[i]=(cut_vertex_t){m->projected[ids[i]],m->vertex[ids[i]].z};
    unsigned count=clip_plane(input,3,upper,cut,true,crossing,&crossings);if(crossings==2)cap_edge(cap,w,h,crossing[0],crossing[1]);cut_vertex_t *poly=upper;
    if(low>-FLT_MAX){count=clip_plane(upper,count,lower,low,false,crossing,&crossings);poly=lower;}
    for(unsigned i=1;i+1<count;i++)triangle(pixels,depth,w,h,poly[0].q,poly[i].q,poly[i+1].q,m->cull_backfaces,shades);
}
/* Read a shifted mask word, treating pixels outside the section as empty. */
static uint32_t cap_shift(const uint32_t *row,int words,int word,int shift){
    int base=word*32+shift,slot=base>=0?base/32:-((-base+31)/32);unsigned bits=(unsigned)(base-slot*32);
    uint32_t a=slot>=0&&slot<words?row[slot]:0,b=slot+1>=0&&slot+1<words?row[slot+1]:0;
    return bits?(a>>bits)|(b<<(32-bits)):a;
}
static void cap_row(uint16_t *pixels,float *depth,int w,int h,int y,const uint32_t *mask,camera_t c,float cut,const int bounds[4],float rim){
    if(y<bounds[1]||y>bounds[3])return;
    int words=(w+31)/32;const uint32_t *row=mask+(size_t)y*words;float z=(cut-c.cz)/c.sp+((y-c.oy)/c.scale)*(c.cp/c.sp);
    mesh_projected_t top=project((mesh_vertex_t){.nz=1},c);int shade=(int)(top.light*255);if(shade<0)shade=0;if(shade>255)shade=255;
    int rx=rim>0?(int)fminf(w,ceilf(rim*c.scale)):0,ry=rim>0?(int)fminf(h,ceilf(rim*c.scale*c.sp)):0;
    for(int word=bounds[0]/32;word<=bounds[2]/32;word++){
        uint32_t fill=row[word];if(!fill)continue;
        if(rim>0){
            uint32_t interior=fill;
            /* Erode only to find the narrow inside boundary band. The final
             * mask always intersects the original section, preserving holes. */
            for(int dy=-ry;dy<=ry&&interior;dy++)interior&=y+dy<0||y+dy>=h?0:mask[(size_t)(y+dy)*words+word];
            for(int dx=1;dx<=rx&&interior;dx++)interior&=cap_shift(row,words,word,dx)&cap_shift(row,words,word,-dx);
            fill&=~interior;
        }
        while(fill){unsigned bit=(unsigned)__builtin_ctz(fill);fill&=fill-1;int x=word*32+(int)bit;if(x<bounds[0]||x>bounds[2])continue;size_t i=(size_t)y*w+x;if(z>depth[i]){depth[i]=z;pixels[i]=shades[shade];}}
    }
}
static void line(uint16_t *,int,int,mesh_projected_t,mesh_projected_t);
static size_t render(mesh_preview_t *m,const gcode_view_t *v,uint16_t *pixels,float *depth,int w,int h,size_t first,size_t budget,bool filled,float low,float cut,uint32_t *cap,float rim){
    if(!pixels||!depth||!v||w<1||h<1||w>2048||h>2048||!budget)return 0;
    if(!first)for(size_t i=0;i<(size_t)w*h;i++)depth[i]=-FLT_MAX;
    if(!m||!isfinite(v->yaw)||!isfinite(v->pitch)||!isfinite(v->zoom)||!isfinite(v->pan_x)||!isfinite(v->pan_y))return 0;
    camera_t c=camera(m,v,w,h);bool capped=cap&&m->cull_backfaces&&cut<FLT_MAX&&cut>m->min[2]&&cut<m->max[2]&&c.sp>.0001f;
    size_t geometry=m->vertices+m->triangles,total=geometry+(capped?(size_t)h*16:0);if(first>=total)return 0;size_t end=total-first>budget?first+budget:total;int bounds[4]={0,0,w-1,h-1};if(capped)mesh_preview_screen_bounds(m,v,w,h,bounds);
    if(!first&&capped)memset(cap,0,(size_t)((w+31)/32)*h*sizeof(*cap));
    for(size_t i=first;i<end;i++){if(i>=geometry){if((i-geometry)%16==0)cap_row(pixels,depth,w,h,(int)((i-geometry)/16),cap,c,cut,bounds,rim);continue;}if(i<m->vertices)m->projected[i]=project(m->vertex[i],c);else{if(i==m->vertices)for(size_t j=0;j<(size_t)w*h;j++)pixels[j]=0x0863;mesh_triangle_t t=m->triangle[i-m->vertices];if(filled){if(cut<FLT_MAX)clipped_triangle(m,t,low,cut,pixels,depth,w,h,capped?cap:NULL);else triangle(pixels,depth,w,h,m->projected[t.a],m->projected[t.b],m->projected[t.c],m->cull_backfaces,shades);}else{line(pixels,w,h,m->projected[t.a],m->projected[t.b]);line(pixels,w,h,m->projected[t.b],m->projected[t.c]);line(pixels,w,h,m->projected[t.c],m->projected[t.a]);}}}
    return end<total?end:0;
}
static void line(uint16_t *pixels,int w,int h,mesh_projected_t a,mesh_projected_t b){
    /* Parametric clip before integer rasterization also bounds work at maximum zoom. */
    float dx=b.x-a.x,dy=b.y-a.y,lo=0,hi=1,p[4]={-dx,dx,-dy,dy},q[4]={a.x,w-1-a.x,a.y,h-1-a.y};
    for(unsigned i=0;i<4;i++){if(fabsf(p[i])<.00001f){if(q[i]<0)return;continue;}float t=q[i]/p[i];if(p[i]<0)lo=fmaxf(lo,t);else hi=fminf(hi,t);if(lo>hi)return;}
    int x0=(int)lroundf(a.x+lo*dx),y0=(int)lroundf(a.y+lo*dy),x1=(int)lroundf(a.x+hi*dx),y1=(int)lroundf(a.y+hi*dy);
    int ix=abs(x1-x0),sx=x0<x1?1:-1,iy=-abs(y1-y0),sy=y0<y1?1:-1,err=ix+iy;
    for(;;){if(x0>=0&&y0>=0&&x0<w&&y0<h)pixels[(size_t)y0*w+x0]=0xfda9;if(x0==x1&&y0==y1)break;int e=err*2;if(e>=iy){err+=iy;x0+=sx;}if(e<=ix){err+=ix;y0+=sy;}}
}
void mesh_preview_render_wire(const mesh_preview_t *m,const gcode_view_t *v,uint16_t *pixels,int w,int h,size_t max_triangles){
    if(!pixels||!v||w<1||h<1||w>2048||h>2048)return;
    for(size_t i=0;i<(size_t)w*h;i++)pixels[i]=0x0863;
    if(!m||!max_triangles||!isfinite(v->yaw)||!isfinite(v->pitch)||!isfinite(v->zoom)||!isfinite(v->pan_x)||!isfinite(v->pan_y))return;
    camera_t c=camera(m,v,w,h);size_t stride=(m->triangles-1)/max_triangles+1;
    for(size_t i=0;i<m->triangles;i+=stride){mesh_triangle_t t=m->triangle[i];mesh_projected_t a=project(m->vertex[t.a],c),b=project(m->vertex[t.b],c),d=project(m->vertex[t.c],c);line(pixels,w,h,a,b);line(pixels,w,h,b,d);line(pixels,w,h,d,a);}
}

size_t mesh_preview_render_step(mesh_preview_t *m,const gcode_view_t *v,uint16_t *p,float *d,int w,int h,size_t first,size_t budget){return render(m,v,p,d,w,h,first,budget,true,-FLT_MAX,FLT_MAX,NULL,0);}
size_t mesh_preview_render_edges_step(mesh_preview_t *m,const gcode_view_t *v,uint16_t *p,float *d,int w,int h,size_t first,size_t budget){return render(m,v,p,d,w,h,first,budget,false,-FLT_MAX,FLT_MAX,NULL,0);}

#define INTERACTION_VERTICES 4096u
#define INTERACTION_TRIANGLES 4096u
#define INTERACTION_SLOTS 8192u
void mesh_preview_free(mesh_preview_t *m,void (*release)(void *)){
    if(m){release(m->interaction);release(m);}
}
bool mesh_preview_prepare_interaction(mesh_preview_t *source,void *(*allocate)(size_t),void (*release)(void *),bool (*cancelled)(void *),void *context){
    if(!source||!allocate||!release||source->interaction||source->triangles<=INTERACTION_TRIANGLES)return false;
    if(cancelled&&cancelled(context))return false;
    size_t offset=32768u*sizeof(uint16_t)+(size_t)source->vertices*sizeof(uint16_t)+INTERACTION_VERTICES*sizeof(uint16_t);offset=(offset+7)&~(size_t)7;
    void *work=allocate(offset+INTERACTION_SLOTS*sizeof(uint64_t));
    mesh_preview_t *m=allocate(sizeof(*m)+INTERACTION_VERTICES*(sizeof(mesh_vertex_t)+sizeof(mesh_projected_t))+INTERACTION_TRIANGLES*sizeof(mesh_triangle_t));
    if(!work||!m){release(work);release(m);return false;}
    memset(m,0,sizeof(*m));m->vertex=(mesh_vertex_t *)(m+1);m->projected=(mesh_projected_t *)(m->vertex+INTERACTION_VERTICES);m->triangle=(mesh_triangle_t *)(m->projected+INTERACTION_VERTICES);
    uint16_t *slots=work,*remap=slots+32768,*counts=remap+source->vertices;uint64_t *faces=(uint64_t *)((uint8_t *)work+offset);
    float span=fmaxf(source->max[0]-source->min[0],fmaxf(source->max[1]-source->min[1],source->max[2]-source->min[2]));
    static const unsigned grids[]={32,24,16,12,8,4};bool good=false;
    for(unsigned attempt=0;attempt<sizeof(grids)/sizeof(grids[0]);attempt++){
        unsigned grid=grids[attempt];float scale=(grid-1)/fmaxf(span,.00001f);bool overflow=false;
        memset(slots,0,32768*sizeof(*slots));memset(counts,0,INTERACTION_VERTICES*sizeof(*counts));memset(faces,0,INTERACTION_SLOTS*sizeof(*faces));memset(m->vertex,0,INTERACTION_VERTICES*sizeof(*m->vertex));m->vertices=m->triangles=0;
        for(unsigned i=0;i<source->vertices;i++){
            if((i&511)==0&&cancelled&&cancelled(context))goto done;
            mesh_vertex_t p=source->vertex[i];unsigned xyz[3];float pos[3]={p.x,p.y,p.z};for(unsigned a=0;a<3;a++)xyz[a]=(unsigned)fmaxf(0,fminf(grid-1,floorf((pos[a]-source->min[a])*scale)));
            unsigned key=xyz[0]+grid*(xyz[1]+grid*xyz[2]),id;
            if(slots[key])id=slots[key]-1;else{if(m->vertices==INTERACTION_VERTICES){overflow=true;break;}id=m->vertices++;slots[key]=(uint16_t)(id+1);}
            remap[i]=(uint16_t)id;counts[id]++;mesh_vertex_t *q=&m->vertex[id];q->x+=p.x;q->y+=p.y;q->z+=p.z;q->nx+=p.nx;q->ny+=p.ny;q->nz+=p.nz;
        }
        if(overflow)continue;
        for(unsigned i=0;i<m->vertices;i++){mesh_vertex_t *p=&m->vertex[i];float n=counts[i];p->x/=n;p->y/=n;p->z/=n;float length=sqrtf(p->nx*p->nx+p->ny*p->ny+p->nz*p->nz);if(length<.00001f){p->nx=p->ny=0;p->nz=1;}else{p->nx/=length;p->ny/=length;p->nz/=length;}}
        for(unsigned i=0;i<source->triangles;i++){
            if((i&511)==0&&cancelled&&cancelled(context))goto done;
            mesh_triangle_t t=source->triangle[i];unsigned a=remap[t.a],b=remap[t.b],c=remap[t.c];if(a==b||b==c||a==c)continue;
            mesh_vertex_t p=m->vertex[a],q=m->vertex[b],r=m->vertex[c];float x=q.x-p.x,y=q.y-p.y,z=q.z-p.z,dx=r.x-p.x,dy=r.y-p.y,dz=r.z-p.z;if(fabsf(y*dz-z*dy)+fabsf(z*dx-x*dz)+fabsf(x*dy-y*dx)<1e-12f)continue;
            unsigned ids[3]={a,b,c};if(ids[0]>ids[1]){unsigned tmp=ids[0];ids[0]=ids[1];ids[1]=tmp;}if(ids[1]>ids[2]){unsigned tmp=ids[1];ids[1]=ids[2];ids[2]=tmp;}if(ids[0]>ids[1]){unsigned tmp=ids[0];ids[0]=ids[1];ids[1]=tmp;}
            uint64_t key=((uint64_t)ids[0]|((uint64_t)ids[1]<<12)|((uint64_t)ids[2]<<24))+1;unsigned slot=(unsigned)(key^(key>>13)^(key>>26))&(INTERACTION_SLOTS-1);while(faces[slot]&&faces[slot]!=key)slot=(slot+1)&(INTERACTION_SLOTS-1);if(faces[slot])continue;
            if(m->triangles==INTERACTION_TRIANGLES){overflow=true;break;}faces[slot]=key;m->triangle[m->triangles++]=(mesh_triangle_t){a,b,c};
        }
        if(!overflow&&m->triangles){m->cell_mm=span/(grid-1);good=true;break;}
    }
    if(good){
        memcpy(m->min,source->min,sizeof(m->min));memcpy(m->max,source->max,sizeof(m->max));m->source_triangles=source->triangles;m->radius=source->radius;
        /* Clustering can change local winding. The small gesture mesh stays two-sided. */
        source->interaction=m;m=NULL;
    }
done:
    release(work);release(m);return good;
}

size_t mesh_preview_render_clipped_step(mesh_preview_t *m,const gcode_view_t *v,uint16_t *p,float *d,int w,int h,size_t first,size_t budget,float cut){if(!isfinite(cut))return 0;return render(m,v,p,d,w,h,first,budget,true,-FLT_MAX,cut,NULL,0);}

size_t mesh_preview_render_section_step(mesh_preview_t *m,const gcode_view_t *v,uint16_t *p,float *d,int w,int h,size_t first,size_t budget,float cut,uint32_t *cap){if(!isfinite(cut))return 0;return render(m,v,p,d,w,h,first,budget,true,-FLT_MAX,cut,cap,0);}

size_t mesh_preview_render_slab_step(mesh_preview_t *m,const gcode_view_t *v,uint16_t *p,float *d,int w,int h,size_t first,size_t budget,float low,float high,uint32_t *cap){if(!isfinite(low)||!isfinite(high)||!(low<high))return 0;return render(m,v,p,d,w,h,first,budget,true,low,high,cap,0);}
size_t mesh_preview_render_joined_section_step(mesh_preview_t *m,const gcode_view_t *v,uint16_t *p,float *d,int w,int h,size_t first,size_t budget,float low,float high,uint32_t *mask){
    if(!isfinite(low)||!isfinite(high)||!(low<high))return 0;
    float rim=m&&isfinite(m->cell_mm)&&m->cell_mm>0?m->cell_mm*1.5f:0;
    /* A missing optional mask keeps the original open section. Never replace
     * the source infill with a full cap when contact joining is unavailable. */
    return render(m,v,p,d,w,h,first,budget,true,low,high,rim>0?mask:NULL,rim);
}
static mesh_projected_t project_point(gcode_point_t p,camera_t c){float x=p.x-c.cx,y=p.y-c.cy,z=p.z-c.cz,along=x*c.sy+y*c.co;return (mesh_projected_t){c.ox+(x*c.co-y*c.sy)*c.scale,c.oy+(along*c.sp-z*c.cp)*c.scale,along*c.cp+z*c.sp,0};}
static void travel_line(uint16_t *pixels,float *depth,int w,int h,mesh_projected_t a,mesh_projected_t b){
    float dx=b.x-a.x,dy=b.y-a.y,lo=0,hi=1,p[4]={-dx,dx,-dy,dy},q[4]={a.x,w-1-a.x,a.y,h-1-a.y};
    for(unsigned i=0;i<4;i++){if(fabsf(p[i])<.00001f){if(q[i]<0)return;continue;}float t=q[i]/p[i];if(p[i]<0)lo=fmaxf(lo,t);else hi=fminf(hi,t);if(lo>hi)return;}
    int x0=(int)lroundf(fmaxf(0,fminf(w-1,a.x+lo*dx))),y0=(int)lroundf(fmaxf(0,fminf(h-1,a.y+lo*dy))),x1=(int)lroundf(fmaxf(0,fminf(w-1,a.x+hi*dx))),y1=(int)lroundf(fmaxf(0,fminf(h-1,a.y+hi*dy))),ix=abs(x1-x0),sx=x0<x1?1:-1,iy=-abs(y1-y0),sy=y0<y1?1:-1,err=ix+iy;unsigned steps=(unsigned)(ix>-iy?ix:-iy);float za=a.z+lo*(b.z-a.z),zb=a.z+hi*(b.z-a.z),z=steps?za:fmaxf(za,zb),dz=steps?(zb-za)/steps:0;
    for(;;){if(x0>=0&&y0>=0&&x0<w&&y0<h){size_t i=(size_t)y0*w+x0;if(z+.002f>=depth[i]){pixels[i]=0x52aa;depth[i]=fmaxf(depth[i],z);}}if(x0==x1&&y0==y1)break;int e=err*2;if(e>=iy){err+=iy;x0+=sx;}if(e<=ix){err+=ix;y0+=sy;}z+=dz;}
}
size_t mesh_preview_render_travel_step(const mesh_preview_t *mesh,const gcode_toolpath_t *m,const gcode_view_t *v,uint16_t *pixels,float *depth,int w,int h,size_t first,size_t budget){
    if(!mesh||!m||!v||!pixels||!depth||!budget||w<1||h<1||w>2048||h>2048||!v->travel||!isfinite(v->yaw)||!isfinite(v->pitch)||!isfinite(v->zoom)||!isfinite(v->pan_x)||!isfinite(v->pan_y))return 0;
    size_t total=m->count+(m->sample_stride>1?1:0);if(first>=total)return 0;size_t end=total-first>budget?first+budget:total;camera_t c=camera(mesh,v,w,h);
    for(size_t i=first;i<end;i++){
        const gcode_segment_t *s=i<m->count?&m->segments[i]:&m->last_segment;
        if(s->extrusion||(v->layer>=0&&(v->cumulative?s->layer>v->layer:s->layer!=v->layer)))continue;
        if(s->a.x==s->b.x&&s->a.y==s->b.y&&s->a.z==s->b.z)continue;
        travel_line(pixels,depth,w,h,project_point(s->a,c),project_point(s->b,c));
    }
    return end<total?end:0;
}

/* Flattened deposited bead: broad top, bevelled sides and closed end faces.
 * Vertices are transient, so inspecting a layer does not allocate another mesh. */
static void extrusion_faces(const gcode_segment_t *s,const gcode_view_t *v,camera_t c,uint16_t *pixels,float *depth,int w,int h,int bounds[4]){
    float dx=s->b.x-s->a.x,dy=s->b.y-s->a.y,dz=s->b.z-s->a.z,length=sqrtf(dx*dx+dy*dy+dz*dz);if(length<.000001f)return;
    float width=s->width>0?s->width:.45f,height=s->height>0?s->height:.2f;
    /* Below the pixel grid physical faces alias to the same hairline as Lines.
     * Enlarge only unresolved cross-sections, keeping their top at source Z.
     * The UI supplies the staging/display ratio for half-resolution gestures. */
    float pixel_ratio=v->raster_ratio>0?fmaxf(.25f,fminf(1,v->raster_ratio)):1;
    width=fmaxf(width,3*pixel_ratio/c.scale);height=fmaxf(height,2*pixel_ratio/c.scale);
    float rw=width*.5f,rh=height*.5f,xy=hypotf(dx,dy),tx=dx/length,ty=dy/length,tz=dz/length;
    float sx,sy,sz,ux,uy,uz;if(xy>.00001f){sx=-dy/xy;sy=dx/xy;sz=0;ux=uy=0;uz=1;}else{sx=1;sy=sz=0;ux=uz=0;uy=dz>=0?1:-1;rh=rw;}
    gcode_point_t a=s->a,b=s->b;
    /* The source envelope includes material around endpoints. A small overlap
     * avoids separate flat caps at path joins without filling open infill bays. */
    if(xy>.00001f){float join=fminf(xy*.25f,fminf(width*.25f,height*.5f));a.x-=dx/xy*join;a.y-=dy/xy*join;b.x+=dx/xy*join;b.y+=dy/xy*join;}
    dx=b.x-a.x;dy=b.y-a.y;length=sqrtf(dx*dx+dy*dy+dz*dz);tx=dx/length;ty=dy/length;tz=dz/length;
    a.z-=height*.5f;b.z-=height*.5f;mesh_projected_t ca=project_point(a,c),cb=project_point(b,c);float pad=(rw+rh)*c.scale;
    if(fmaxf(ca.x,cb.x)+pad<0||fminf(ca.x,cb.x)-pad>w-1||fmaxf(ca.y,cb.y)+pad<0||fminf(ca.y,cb.y)-pad>h-1)return;
    static const float co[8]={1,.95f,-.95f,-1,-1,-.95f,.95f,1},si[8]={.75f,1,1,.75f,-.75f,-1,-1,-.75f};
    mesh_projected_t q[16];float minx=FLT_MAX,miny=FLT_MAX,maxx=-FLT_MAX,maxy=-FLT_MAX;
    for(unsigned i=0;i<8;i++){
        float x=rw*co[i]*sx+rh*si[i]*ux,y=rw*co[i]*sy+rh*si[i]*uy,z=rw*co[i]*sz+rh*si[i]*uz;
        q[i]=project((mesh_vertex_t){.x=a.x+x,.y=a.y+y,.z=a.z+z},c);q[i+8]=project((mesh_vertex_t){.x=b.x+x,.y=b.y+y,.z=b.z+z},c);
        for(unsigned end=0;end<2;end++){mesh_projected_t *p=&q[i+8*end];p->z+=.002f;minx=fminf(minx,p->x);miny=fminf(miny,p->y);maxx=fmaxf(maxx,p->x);maxy=fmaxf(maxy,p->y);}
    }
    if(bounds){int rect[4]={(int)fmaxf(0,fminf(w-1,floorf(minx)-2)),(int)fmaxf(0,fminf(h-1,floorf(miny)-2)),(int)fmaxf(0,fminf(w-1,ceilf(maxx)+2)),(int)fmaxf(0,fminf(h-1,ceilf(maxy)+2))};if(rect[0]<bounds[0])bounds[0]=rect[0];if(rect[1]<bounds[1])bounds[1]=rect[1];if(rect[2]>bounds[2])bounds[2]=rect[2];if(rect[3]>bounds[3])bounds[3]=rect[3];}
    const uint16_t *palette=shades;
    for(unsigned i=0;i<8;i++){
        unsigned j=(i+1)%8;float ds=rw*(co[j]-co[i]),du=rh*(si[j]-si[i]);
        float vx=ds*sx+du*ux,vy=ds*sy+du*uy,vz=ds*sz+du*uz;
        float nx=vy*tz-vz*ty,ny=vz*tx-vx*tz,nz=vx*ty-vy*tx,n=sqrtf(nx*nx+ny*ny+nz*nz);if(n<.0000001f)continue;
        float light=project((mesh_vertex_t){.nx=nx/n,.ny=ny/n,.nz=nz/n},c).light;
        mesh_projected_t p0=q[i],p1=q[j],p2=q[j+8],p3=q[i+8];p0.light=p1.light=p2.light=p3.light=light;
        triangle(pixels,depth,w,h,p0,p1,p2,true,palette);triangle(pixels,depth,w,h,p0,p2,p3,true,palette);
    }
    float lighta=project((mesh_vertex_t){.nx=-tx,.ny=-ty,.nz=-tz},c).light,lightb=project((mesh_vertex_t){.nx=tx,.ny=ty,.nz=tz},c).light;
    for(unsigned i=0;i<8;i++){q[i].light=lighta;q[i+8].light=lightb;}
    for(unsigned i=1;i<7;i++){triangle(pixels,depth,w,h,q[0],q[i+1],q[i],true,palette);triangle(pixels,depth,w,h,q[8],q[8+i],q[9+i],true,palette);}
}
static size_t layer_bound(const gcode_toolpath_t *m,unsigned layer,bool after){
    size_t lo=0,hi=m->count;while(lo<hi){size_t mid=lo+(hi-lo)/2;unsigned value=m->segments[mid].layer;if(value<layer||(after&&value==layer))lo=mid+1;else hi=mid;}return lo;
}
size_t mesh_preview_render_layer_step(const mesh_preview_t *mesh,const gcode_toolpath_t *m,const gcode_view_t *v,uint16_t *pixels,float *depth,int w,int h,size_t first,size_t budget,int bounds[4]){
    if(!mesh||!m||!v||!pixels||!depth||!budget||w<1||h<1||w>2048||h>2048||v->layer<0||!isfinite(v->yaw)||!isfinite(v->pitch)||!isfinite(v->zoom)||!isfinite(v->pan_x)||!isfinite(v->pan_y))return 0;
    /* Parser/cache arrays preserve monotonically assigned print-order level IDs. */
    size_t begin=layer_bound(m,(unsigned)v->layer,false),finish=layer_bound(m,(unsigned)v->layer,true),count=finish-begin,total=count+(m->sample_stride>1?1:0)+1;if(first>=total)return 0;size_t end=total-first>budget?first+budget:total;camera_t c=camera(mesh,v,w,h);
    for(size_t i=first;i<end;i++){
        const gcode_segment_t *s=i<count?&m->segments[begin+i]:i==count&&m->sample_stride>1?&m->last_segment:v->layer<m->layer_count?&m->layer_anchor[v->layer]:NULL;
        if(!s||!s->extrusion||s->layer!=v->layer)continue;
        extrusion_faces(s,v,c,pixels,depth,w,h,bounds);
    }
    return end<total?end:0;
}
