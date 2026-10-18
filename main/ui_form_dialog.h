#pragma once
#include "ui_popup.h"
#include "ui_responsive_layout.h"
#include "ui_text_fit.h"
#include "ui_widgets.h"

/* Native forms: scrolling content, optional pinned keyboard, pinned actions. */
static inline lv_obj_t *ui_form_dialog(const char *title,int width,int height,ui_popup_kind_t kind)
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
    lv_label_set_text(heading,title);lv_obj_set_width(heading,LV_PCT(100));
    ui_apply_custom_label_style(heading,UI_FONT_TITLE,UI_TEXT);
    lv_label_set_long_mode(heading,LV_LABEL_LONG_DOT);
    lv_obj_t *body=lv_obj_create(popup);lv_obj_remove_style_all(body);
    lv_obj_set_size(body,LV_PCT(100),0);lv_obj_set_flex_grow(body,1);
    lv_obj_set_flex_flow(body,LV_FLEX_FLOW_COLUMN);lv_obj_set_style_pad_row(body,8,0);
    lv_obj_set_scroll_dir(body,LV_DIR_VER);lv_obj_set_scrollbar_mode(body,LV_SCROLLBAR_MODE_OFF);
    lv_obj_t *footer=lv_obj_create(popup);ui_responsive_column(footer);
    ui_apply_divider_border(footer,LV_BORDER_SIDE_TOP);
    lv_obj_set_style_pad_top(footer,8,0);
    lv_obj_set_flex_flow(footer,LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_column(footer,8,0);lv_obj_set_style_pad_row(footer,8,0);
    return popup;
}
static inline lv_obj_t *ui_form_body(lv_obj_t *popup){return lv_obj_get_child(popup,1);}
static inline lv_obj_t *ui_form_text(lv_obj_t *parent,const char *text,const lv_font_t *font,lv_color_t color)
{
    lv_obj_t *label=lv_label_create(parent);lv_obj_set_width(label,LV_PCT(100));
    lv_label_set_text(label,text?text:"");lv_label_set_long_mode(label,LV_LABEL_LONG_WRAP);
    ui_apply_custom_label_style(label,font,color);return label;
}
static inline void ui_form_action_fit(lv_event_t *event)
{
    if (!event || lv_event_get_target_obj(event) != lv_event_get_current_target_obj(event)) return;
    lv_obj_t *label=lv_obj_get_child(lv_event_get_current_target_obj(event),0);
    if (!label || !lv_obj_check_type(label,&lv_label_class)) return;
    ui_text_fit_single_line(label,UI_FONT_BODY);
}
static inline lv_obj_t *ui_form_action(lv_obj_t *popup,ui_button_kind_t kind,const char *text,lv_event_cb_t cb,void *data)
{
    lv_obj_t *button=ui_button_create(lv_obj_get_child(popup,-1),kind,text);
    lv_obj_set_size(button,156,48);lv_obj_set_flex_grow(button,1);
    lv_obj_add_event_cb(button,ui_form_action_fit,LV_EVENT_SIZE_CHANGED,NULL);
    if(cb)lv_obj_add_event_cb(button,cb,LV_EVENT_CLICKED,data);
    return button;
}
static inline void ui_form_finish(lv_obj_t *popup)
{
    lv_obj_update_layout(popup);lv_obj_t *footer=lv_obj_get_child(popup,-1);
    if(lv_obj_get_child_count(footer)==1) {
        lv_obj_set_flex_grow(lv_obj_get_child(footer,0),0);
        lv_obj_set_flex_align(footer,LV_FLEX_ALIGN_END,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
        lv_obj_update_layout(popup);
    }
    for(unsigned i=0;i<lv_obj_get_child_count(footer);i++)
        ui_text_fit_single_line(lv_obj_get_child(lv_obj_get_child(footer,i),0),UI_FONT_BODY);
}
static inline lv_obj_t *ui_form_input(lv_obj_t *parent,const char *placeholder,const char *value,bool password,unsigned max_length)
{
    lv_obj_t *input=lv_textarea_create(parent);lv_obj_set_size(input,LV_PCT(100),56);
    lv_textarea_set_one_line(input,true);lv_textarea_set_password_mode(input,password);
    lv_obj_set_style_min_height(input,56,0);
    lv_textarea_set_max_length(input,max_length);
    lv_textarea_set_placeholder_text(input,placeholder?placeholder:"");
    if(value)lv_textarea_set_text(input,value);
    ui_apply_custom_label_style(input,UI_FONT_BODY,UI_TEXT);
    ui_apply_surface_role(input,UI_SURFACE_TEXT_INPUT);
    lv_obj_add_event_cb(input,ui_popup_input_focus_cb,LV_EVENT_FOCUSED,NULL);
    return input;
}
static inline lv_obj_t *ui_form_keyboard(lv_obj_t *popup,lv_obj_t *input,lv_keyboard_mode_t mode)
{
    int height=lv_display_get_vertical_resolution(NULL)/3;
    if(height<140)height=140;
    if(height>200)height=200;
    lv_obj_t *keyboard=ui_popup_add_keyboard(popup,input,LV_PCT(100),height,
        LV_ALIGN_TOP_LEFT,0,0,mode);
    if (!keyboard) return NULL;
    lv_obj_move_to_index(keyboard,2);
    return keyboard;
}
