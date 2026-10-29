#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "thumbnail_render.h"
#include "ui_thumbnail.h"
static uint16_t pixel(int x,int y){return (uint16_t)(1+((x%100)<<8)+(y%100));}
static void render_case(int sw,int sh,int dw,int dh)
{
    uint16_t *source=malloc((size_t)sw*sh*2);
    uint16_t *buffer=malloc(((size_t)dw*dh+2)*2);
    assert(source && buffer);
    for(int y=0;y<sh;++y)for(int x=0;x<sw;++x)source[y*sw+x]=pixel(x,y);
    lv_image_dsc_t dsc={0};dsc.header.magic=LV_IMAGE_HEADER_MAGIC;
    dsc.header.cf=LV_COLOR_FORMAT_RGB565;dsc.header.w=sw;dsc.header.h=sh;
    dsc.header.stride=sw*2;dsc.data_size=sw*sh*2;dsc.data=(uint8_t*)source;
    buffer[0]=0xABCD;buffer[dw*dh+1]=0xDCBA;
    memset(buffer+1,0,(size_t)dw*dh*2);
    assert(thumbnail_render_to_rgb565_fit(&dsc,buffer+1,dw,dh));
    int fw=dw,fh=(int)((int64_t)sh*fw/sw);
    if(fh>dh){fh=dh;fw=(int)((int64_t)sw*fh/sh);}
    if(fw<1)fw=1;
    if(fh<1)fh=1;
    int ox=(dw-fw)/2,oy=(dh-fh)/2;
    for(int y=0;y<dh;y++)for(int x=0;x<dw;x++){
        uint16_t expected=0;
        if(x>=ox && x<ox+fw && y>=oy && y<oy+fh)
            expected=pixel((int)((int64_t)(x-ox)*sw/fw),(int)((int64_t)(y-oy)*sh/fh));
        assert(buffer[1+y*dw+x]==expected);
    }
    assert(buffer[0]==0xABCD && buffer[dw*dh+1]==0xDCBA);
    int packed_w=0,packed_h=0;
    for(int i=0;i<dw*dh;i++)buffer[1+i]=0xBEEF;
    assert(thumbnail_render_to_rgb565_aspect(&dsc,buffer+1,dw,dh,&packed_w,&packed_h));
    assert(packed_w==fw && packed_h==fh);
    for(int y=0;y<packed_h;y++)for(int x=0;x<packed_w;x++)
        assert(buffer[1+y*packed_w+x]==pixel((int)((int64_t)x*sw/packed_w),(int)((int64_t)y*sh/packed_h)));
    for(int i=packed_w*packed_h;i<dw*dh;i++)assert(buffer[1+i]==0xBEEF);
    assert(buffer[0]==0xABCD && buffer[dw*dh+1]==0xDCBA);
    free(source);free(buffer);
}
static void ui_case(int width,int height)
{
    lv_obj_t *box=lv_obj_create(lv_screen_active());lv_obj_set_size(box,width,height);
    lv_obj_set_style_pad_all(box,0,0);lv_obj_set_style_border_width(box,1,0);
    lv_obj_t *image=lv_image_create(box);
    static uint16_t pixels[286*215];
    lv_image_dsc_t dsc={0};dsc.header.magic=LV_IMAGE_HEADER_MAGIC;
    dsc.header.cf=LV_COLOR_FORMAT_RGB565;dsc.header.w=286;dsc.header.h=215;
    dsc.header.stride=572;dsc.data_size=sizeof(pixels);dsc.data=(uint8_t*)pixels;
    lv_image_set_src(image,&dsc);
    int scale;
    ui_thumbnail_fit_object(image,box,286,215,6);
    lv_obj_update_layout(box);
    assert(lv_image_get_inner_align(image)==LV_IMAGE_ALIGN_CONTAIN);
    assert(lv_obj_get_width(image)==lv_obj_get_content_width(box)-12);
    assert(lv_obj_get_height(image)==lv_obj_get_content_height(box)-12);
    scale=lv_image_get_scale(image);
    assert((int64_t)286*scale<=lv_obj_get_width(image)*256);
    assert((int64_t)215*scale<=lv_obj_get_height(image)*256);
    ui_thumbnail_fit_object(image,box,0,0,0);
    lv_obj_update_layout(box);
    assert(lv_image_get_inner_align(image)==LV_IMAGE_ALIGN_CONTAIN);
    scale=lv_image_get_scale(image);
    assert((int64_t)286*scale<=lv_obj_get_content_width(box)*256);
    assert((int64_t)215*scale<=lv_obj_get_content_height(box)*256);
    lv_obj_set_size(box,width+73,height+51);
    ui_thumbnail_fit_object(image,box,0,0,0);lv_obj_update_layout(box);
    assert(lv_image_get_inner_align(image)==LV_IMAGE_ALIGN_CONTAIN);
    assert(lv_obj_get_width(image)==lv_obj_get_content_width(box));
    assert(lv_obj_get_height(image)==lv_obj_get_content_height(box));
    lv_obj_delete(box);
}
int main(void)
{
    lv_init();lv_display_create(1024,600);
    render_case(900,520,286,215);render_case(520,900,286,215);
    render_case(320,240,286,215);render_case(1,1,286,215);
    render_case(900,520,900,520);render_case(1,17,13,1);
    ui_case(286,215);ui_case(184,160);ui_case(158,220);ui_case(420,160);
    int w=99,h=99;
    assert(!thumbnail_render_to_rgb565_aspect(NULL,NULL,286,215,&w,&h));assert(w==0 && h==0);
    assert(!thumbnail_render_to_rgb565_fit(NULL,NULL,0,0));
    puts("PASS: tightly packed aspect previews, actual dimensions/stride and untouched buffer tails, complete-image aspect fit, exact sampling/padding, degenerate sources, buffer bounds, zero-inset containment");
}
