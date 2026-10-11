#pragma once
#include "ui_popup.h"
#include "ui_responsive_layout.h"
#include "ui_text_fit.h"

/* Title, scrolling content and pinned controls shared by update owners. */
static inline lv_obj_t *ui_ota_dialog(const char *title,int width,int height,ui_popup_kind_t kind)
{
    int available=lv_display_get_horizontal_resolution(NULL)-32;
    if(width>available)width=available;
    available=lv_display_get_vertical_resolution(NULL)-32;
    if(height>available)height=available;
    lv_obj_t *popup=ui_popup_create(lv_layer_top(),width,height,kind);
    if(!popup)return NULL;
    lv_obj_set_flex_flow(popup,LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(popup,16,0);lv_obj_set_style_pad_row(popup,12,0);
    lv_obj_t *heading=lv_label_create(popup);
    lv_label_set_text(heading,title);ui_apply_custom_label_style(heading,UI_FONT_TITLE,UI_TEXT);
    lv_obj_set_size(heading,LV_PCT(100),UI_FONT_TITLE->line_height);
    lv_label_set_long_mode(heading,LV_LABEL_LONG_DOT);
    lv_obj_t *body=lv_obj_create(popup);lv_obj_remove_style_all(body);
    lv_obj_set_size(body,LV_PCT(100),0);lv_obj_set_flex_grow(body,1);
    lv_obj_set_flex_flow(body,LV_FLEX_FLOW_COLUMN);lv_obj_set_style_pad_row(body,12,0);
    lv_obj_set_scroll_dir(body,LV_DIR_VER);lv_obj_set_scrollbar_mode(body,LV_SCROLLBAR_MODE_AUTO);
    lv_obj_t *footer=lv_obj_create(popup);ui_responsive_column(footer);
    lv_obj_set_flex_flow(footer,LV_FLEX_FLOW_ROW);lv_obj_set_style_pad_column(footer,8,0);
    return popup;
}
static inline lv_obj_t *ui_ota_body(lv_obj_t *popup){return lv_obj_get_child(popup,1);}
static inline lv_obj_t *ui_ota_text(lv_obj_t *parent,const char *text,const lv_font_t *font,lv_color_t color)
{
    lv_obj_t *label=lv_label_create(parent);lv_obj_set_width(label,LV_PCT(100));
    lv_label_set_long_mode(label,LV_LABEL_LONG_WRAP);lv_label_set_text(label,text);
    ui_apply_custom_label_style(label,font,color);return label;
}
static inline void ui_ota_fit_action(lv_event_t *event);
static inline lv_obj_t *ui_ota_action(lv_obj_t *popup,ui_popup_action_t kind,const char *text,lv_event_cb_t callback,void *data)
{
    lv_obj_t *button=ui_popup_add_action_at(lv_obj_get_child(popup,2),kind,text,0,0,96,48,callback,data,NULL);
    lv_obj_set_flex_grow(button,1);
    lv_obj_add_event_cb(button,ui_ota_fit_action,LV_EVENT_SIZE_CHANGED,NULL);
    ui_text_fit_single_line(lv_obj_get_child(button,0),UI_FONT_BODY);
    return button;
}
static inline void ui_ota_fit_action(lv_event_t *event)
{
    if (!event || lv_event_get_target_obj(event) != lv_event_get_current_target_obj(event)) return;
    lv_obj_t *label=lv_obj_get_child(lv_event_get_current_target_obj(event),0);
    if (!label || !lv_obj_check_type(label, &lv_label_class)) return;
    lv_obj_set_width(label,LV_PCT(100));
    ui_text_fit_single_line(label,UI_FONT_BODY);
}
