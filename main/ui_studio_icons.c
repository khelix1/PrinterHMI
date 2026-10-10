#include "ui_studio_icons.h"
#include "ui_theme.h"
static void stroke(lv_obj_t *host,const lv_point_precise_t *points,unsigned count)
{
    lv_obj_t *line=lv_line_create(host);
    lv_line_set_points(line,points,count);
    lv_obj_set_style_line_width(line,2,0);
    lv_obj_set_style_line_rounded(line,true,0);
    lv_obj_set_style_line_color(line,UI_TEXT_DIM,0);
    lv_obj_clear_flag(line,LV_OBJ_FLAG_CLICKABLE);
}
#define PATH(name,...) static const lv_point_precise_t name[]={__VA_ARGS__}
#define ADD(name) stroke(host,name,sizeof(name)/sizeof(name[0]))
lv_obj_t *ui_studio_icon_create(lv_obj_t *parent,unsigned icon)
{
    lv_obj_t *host=lv_obj_create(parent);lv_obj_remove_style_all(host);
    lv_obj_set_size(host,26,26);lv_obj_clear_flag(host,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);
    switch(icon) {
    case 0: {
        PATH(a,{2,2},{10,2},{10,10},{2,10},{2,2});
        PATH(b,{16,2},{24,2},{24,10},{16,10},{16,2});
        PATH(c,{2,16},{10,16},{10,24},{2,24},{2,16});
        PATH(d,{16,16},{24,16},{24,24},{16,24},{16,16});ADD(a);ADD(b);ADD(c);ADD(d);break;
    }
    case 1: {
        PATH(a,{6,9},{6,2},{20,2},{20,9});
        PATH(b,{6,19},{2,19},{2,9},{24,9},{24,19},{20,19});
        PATH(c,{6,15},{20,15},{20,24},{6,24},{6,15});ADD(a);ADD(b);ADD(c);break;
    }
    case 2: {
        PATH(a,{5,2},{16,2},{22,8},{22,24},{5,24},{5,2});
        PATH(b,{16,2},{16,8},{22,8});PATH(c,{9,14},{18,14});PATH(d,{9,19},{18,19});ADD(a);ADD(b);ADD(c);ADD(d);break;
    }
    case 3: {
        PATH(a,{2,7},{8,7},{10,3},{17,3},{19,7},{24,7},{24,23},{2,23},{2,7});
        PATH(b,{10,10},{16,10},{19,13},{19,17},{16,20},{10,20},{7,17},{7,13},{10,10});ADD(a);ADD(b);break;
    }
    case 4: {
        PATH(a,{3,23},{2,20},{13,9},{13,4},{17,1},{18,1},{16,6},{20,10},{25,7},{25,9},{22,13},{17,13},{6,24},{3,23});ADD(a);break;
    }
    case 5: {
        PATH(a,{2,3},{24,3},{24,23},{2,23},{2,3});PATH(b,{7,8},{11,12},{7,16});PATH(c,{14,17},{20,17});ADD(a);ADD(b);ADD(c);break;
    }
    case 6: {
        PATH(a,{3,8},{7,2},{20,2},{24,8},{24,24},{3,24},{3,8},{24,8});
        PATH(b,{8,20},{10,17},{8,14},{10,11});PATH(c,{14,20},{16,17},{14,14},{16,11});ADD(a);ADD(b);ADD(c);break;
    }
    default: {
        PATH(a,{10,2},{16,2},{17,6},{21,6},{24,10},{21,13},{23,17},{20,21},{16,20},{14,24},{9,24},{8,20},{4,20},{2,16},{5,13},{3,9},{6,5},{10,6},{10,2});
        PATH(b,{10,9},{16,9},{18,13},{16,17},{10,17},{8,13},{10,9});ADD(a);ADD(b);break;
    }
    }
    return host;
}
void ui_studio_icon_color(lv_obj_t *icon,lv_color_t color)
{
    if(!icon)return;
    for(unsigned i=0;i<lv_obj_get_child_count(icon);i++)lv_obj_set_style_line_color(lv_obj_get_child(icon,i),color,0);
}
