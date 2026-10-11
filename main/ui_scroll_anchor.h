#pragma once
#include "lvgl.h"
#include <string.h>

/* Store values rather than widget pointers across a page rebuild. Section
 * titles identify the same content even when a theme reorders its cards. */
typedef struct { char title[96]; int32_t offset; int32_t fraction; } ui_scroll_anchor_t;
static inline const char *ui_scroll_section_title(lv_obj_t *section)
{
    for(uint32_t i=0;i<lv_obj_get_child_count(section);i++) {
        lv_obj_t *child=lv_obj_get_child(section,i);
        if(lv_obj_check_type(child,&lv_label_class))return lv_label_get_text(child);
    }
    return "";
}
static inline ui_scroll_anchor_t ui_scroll_anchor_capture(lv_obj_t *body)
{
    ui_scroll_anchor_t saved={0};if(!body)return saved;
    lv_obj_update_layout(body);
    int32_t y=lv_obj_get_scroll_y(body),total=y+lv_obj_get_scroll_bottom(body);
    if(y<0)y=0;
    if(total>0)saved.fraction=(int32_t)((int64_t)y*10000/total);
    int32_t nearest=INT32_MIN;
    for(uint32_t i=0;i<lv_obj_get_child_count(body);i++) {
        lv_obj_t *section=lv_obj_get_child(body,i);
        if(lv_obj_has_flag(section,LV_OBJ_FLAG_HIDDEN))continue;
        int32_t top=lv_obj_get_y(section);const char *title=ui_scroll_section_title(section);
        if(title[0] && strlen(title)<sizeof(saved.title) && top<=y && top>=nearest) {
            nearest=top;memcpy(saved.title,title,strlen(title)+1);saved.offset=y-top;
        }
    }
    return saved;
}
static inline void ui_scroll_anchor_restore(lv_obj_t *body,ui_scroll_anchor_t saved)
{
    if(!body)return;
    lv_obj_update_layout(body);
    int32_t total=lv_obj_get_scroll_y(body)+lv_obj_get_scroll_bottom(body);
    if(total<0)total=0;
    int32_t y=(int32_t)((int64_t)total*saved.fraction/10000);
    for(uint32_t i=0;saved.title[0] && i<lv_obj_get_child_count(body);i++) {
        lv_obj_t *section=lv_obj_get_child(body,i);
        if(!lv_obj_has_flag(section,LV_OBJ_FLAG_HIDDEN) && !strcmp(saved.title,ui_scroll_section_title(section))) {
            int32_t offset=saved.offset,height=lv_obj_get_height(section);
            if(offset>=height)offset=height>0?height-1:0;
            y=lv_obj_get_y(section)+offset;break;
        }
    }
    if(y<0)y=0;
    if(y>total)y=total;
    lv_obj_scroll_to_y(body,y,LV_ANIM_OFF);
}
