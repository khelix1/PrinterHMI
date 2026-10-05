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
    assert(thumbnail_render_to_rgb565(&dsc,buffer+1,dw,dh));
    int cw=sw,ch=sh;
    if((int64_t)sw*dh>(int64_t)sh*dw)cw=(int)((int64_t)sh*dw/dh);
    else ch=(int)((int64_t)sw*dh/dw);
    if(cw<1)cw=1;
    if(ch<1)ch=1;
    for(int y=0;y<dh;++y)for(int x=0;x<dw;++x){
        int sx=(sw-cw)/2+(int)((int64_t)x*cw/dw);
        int sy=(sh-ch)/2+(int)((int64_t)y*ch/dh);
        assert(buffer[1+y*dw+x]==pixel(sx,sy));
    }
    assert(buffer[0]==0xABCD && buffer[dw*dh+1]==0xDCBA);
    if(sw>1 && sh>1){
        assert(thumbnail_render_to_rgb565_fit(&dsc,buffer+1,dw,dh));
        if(sw*dh!=sh*dw) assert(buffer[1]==0 || buffer[dw*dh]==0);
        if(sw==dw && sh==dh) assert(!memcmp(buffer+1,source,(size_t)sw*sh*2));
    }
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
    lv_image_set_src(image,&dsc);ui_thumbnail_fill_object(image,box,0,0);
    lv_obj_update_layout(box);
    int scale=lv_image_get_scale(image);
    assert((int64_t)286*scale>=lv_obj_get_content_width(box)*256);
    assert((int64_t)215*scale>=lv_obj_get_content_height(box)*256);
    assert(lv_image_get_inner_align(image)==LV_IMAGE_ALIGN_COVER);
    assert(lv_obj_get_style_clip_corner(box,0));
    assert(!lv_obj_has_flag(box,LV_OBJ_FLAG_OVERFLOW_VISIBLE));
    ui_thumbnail_fit_object(image,box,286,215,6);
    lv_obj_update_layout(box);
    assert(lv_image_get_inner_align(image)==LV_IMAGE_ALIGN_CONTAIN);
    assert(lv_obj_get_width(image)==lv_obj_get_content_width(box)-12);
    assert(lv_obj_get_height(image)==lv_obj_get_content_height(box)-12);
    scale=lv_image_get_scale(image);
    assert((int64_t)286*scale<=lv_obj_get_width(image)*256);
    assert((int64_t)215*scale<=lv_obj_get_height(image)*256);
    lv_obj_set_size(box,width+73,height+51);
    ui_thumbnail_fill_object(image,box,0,0);lv_obj_update_layout(box);
    scale=lv_image_get_scale(image);
    assert((int64_t)286*scale>=lv_obj_get_content_width(box)*256);
    assert((int64_t)215*scale>=lv_obj_get_content_height(box)*256);
    lv_obj_delete(box);
}
int main(void)
{
    lv_init();lv_display_create(1024,600);
    render_case(900,520,286,215);render_case(520,900,286,215);
    render_case(320,240,286,215);render_case(1,1,286,215);
    render_case(900,520,900,520);render_case(1,17,13,1);
    ui_case(286,215);ui_case(184,160);ui_case(158,220);ui_case(420,160);
    assert(!thumbnail_render_to_rgb565(NULL,NULL,0,0));
    puts("PASS: proportional preview fill, centered crops, all pixels, bounds, fullscreen fit and LVGL well coverage");
}
