#pragma once
#include <assert.h>
#include <string.h>
/* Exercise actual owners: focus reveals the complete field without moving keys/actions. */
static inline void keyboard_layout_check(lv_obj_t *form,lv_obj_t *keyboard,lv_obj_t *input)
{
    assert(form&&keyboard&&input&&lv_obj_get_parent(keyboard)==form);
    lv_obj_update_layout(form);
    lv_obj_t *body=input;
    while(lv_obj_get_parent(body)!=form)body=lv_obj_get_parent(body);
    lv_obj_t *footer=lv_obj_get_child(form,-1);
    lv_area_t keys,actions,after,field,viewport;
    lv_obj_get_coords(keyboard,&keys);lv_obj_get_coords(footer,&actions);
    assert(keys.y2<actions.y1);
    lv_obj_scroll_to_y(body,lv_obj_get_scroll_bottom(body)+1000,LV_ANIM_OFF);
    lv_obj_send_event(input,LV_EVENT_FOCUSED,NULL);lv_obj_update_layout(form);
    lv_obj_get_coords(input,&field);lv_obj_get_coords(body,&viewport);
    assert(field.x1>=viewport.x1&&field.x2<=viewport.x2&&field.y1>=viewport.y1&&field.y2<=viewport.y2);
    assert(lv_obj_get_height(input)>=56);
    lv_obj_get_coords(keyboard,&after);assert(!memcmp(&keys,&after,sizeof(keys)));
    lv_obj_get_coords(footer,&after);assert(!memcmp(&actions,&after,sizeof(actions)));
}
