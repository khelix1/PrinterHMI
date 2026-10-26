/* Frozen v7.1.26 material geometry for pixel/depth regression checks. */
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
