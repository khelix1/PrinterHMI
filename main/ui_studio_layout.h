#pragma once
#include "ui_theme.h"
#include "ui_font_fallback.h"
#include "ui_text_fit.h"

/* STUDIO owns these primitives; application controllers own their behavior. */
static inline lv_obj_t *studio_plane(lv_obj_t *parent,int x,int y,int w,int h)
{
    lv_obj_t *o=lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);
    lv_obj_clear_flag(o,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);
    return o;
}
static inline lv_obj_t *studio_text(lv_obj_t *parent,const char *text,int x,int y,int w,const lv_font_t *font,lv_color_t color)
{
    lv_obj_t *o=lv_label_create(parent);
    lv_label_set_text(o,text?text:"");lv_obj_set_pos(o,x,y);lv_obj_set_width(o,w);
    lv_obj_set_style_text_font(o,ui_font_with_fallback(font),0);
    lv_obj_set_style_text_color(o,color,0);
    lv_label_set_long_mode(o,LV_LABEL_LONG_DOT);
    lv_obj_set_height(o,lv_font_get_line_height(ui_font_with_fallback(font)));
    return o;
}
static inline lv_obj_t *studio_rule(lv_obj_t *parent,int x,int y,int w,int h)
{
    lv_obj_t *o=studio_plane(parent,x,y,w,h);
    lv_obj_set_style_bg_color(o,UI_BORDER,0);
    lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);return o;
}
static inline lv_obj_t *studio_action(lv_obj_t *parent,const char *text,int x,int y,int w,int h,lv_event_cb_t cb,void *data)
{
    lv_obj_t *o=lv_button_create(parent);
    lv_obj_remove_style_all(o);lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);
    lv_obj_set_style_bg_color(o,UI_CONTROL,LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(o,LV_OPA_COVER,LV_STATE_PRESSED);
    lv_obj_set_style_radius(o,h/2,0);
    lv_obj_set_style_border_width(o,1,0);lv_obj_set_style_border_color(o,UI_BORDER_BRIGHT,0);
    lv_obj_set_style_opa(o,LV_OPA_40,LV_STATE_DISABLED);
    lv_obj_set_style_outline_color(o,UI_ACCENT_BRIGHT,LV_STATE_FOCUS_KEY);
    lv_obj_set_style_outline_width(o,2,LV_STATE_FOCUS_KEY);
    lv_obj_clear_flag(o,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *label=studio_text(o,text,8,0,w-16,UI_FONT_BODY,UI_TEXT);
    ui_text_fit_single_line(label,UI_FONT_BODY);lv_obj_center(label);
    if(cb)lv_obj_add_event_cb(o,cb,LV_EVENT_CLICKED,data);
    return o;
}
