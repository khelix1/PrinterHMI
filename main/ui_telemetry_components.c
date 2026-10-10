#include "ui_telemetry_components.h"
#include "ui_theme.h"
#include "ui_font_fallback.h"
#include "ui_text_fit.h"

static void value_resized(lv_event_t *event){ui_text_fit_single_line(lv_event_get_target(event),UI_FONT_BODY_LARGE);}

lv_obj_t *telemetry_make_label(lv_obj_t *parent,const char *text,const lv_font_t *font,lv_color_t color)
{
    lv_obj_t *label=lv_label_create(parent);
    lv_label_set_text(label,text?text:"");
    ui_apply_custom_label_style(label,ui_font_with_fallback(font),color);
    lv_label_set_long_mode(label,LV_LABEL_LONG_WRAP);
    return label;
}

lv_obj_t *telemetry_create_metric_card(lv_obj_t *parent,const char *title,lv_color_t accent,lv_obj_t **title_out,lv_obj_t **value_out)
{
    lv_obj_t *card=lv_obj_create(parent);
    ui_apply_surface_role(card,UI_SURFACE_TELEMETRY_CARD);
    lv_obj_set_style_pad_all(card,12,0);
    lv_obj_set_style_pad_row(card,8,0);
    lv_obj_clear_flag(card,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(card,LV_FLEX_FLOW_COLUMN);
    lv_obj_set_size(card,400,LV_SIZE_CONTENT);
    lv_obj_t *header=lv_obj_create(card);
    lv_obj_remove_style_all(header);lv_obj_clear_flag(header,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(header,LV_PCT(100),LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(header,LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(header,8,0);
    lv_obj_t *name=telemetry_make_label(header,title,UI_FONT_BODY,accent);
    lv_obj_set_width(name,LV_PCT(45));
    lv_obj_t *value=telemetry_make_label(header,"--",UI_FONT_BODY_LARGE,UI_TEXT_BRIGHT);
    lv_obj_set_width(value,0);lv_obj_set_flex_grow(value,1);
    lv_obj_set_style_text_align(value,LV_TEXT_ALIGN_RIGHT,0);
    lv_obj_add_event_cb(value,value_resized,LV_EVENT_SIZE_CHANGED,NULL);
    if(title_out)*title_out=name;
    if(value_out)*value_out=value;
    return card;
}
