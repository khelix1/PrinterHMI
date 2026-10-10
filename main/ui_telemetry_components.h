#pragma once
#include "lvgl.h"
lv_obj_t *telemetry_make_label(lv_obj_t *parent,const char *text,const lv_font_t *font,lv_color_t color);
lv_obj_t *telemetry_create_metric_card(lv_obj_t *parent,const char *title,lv_color_t accent,lv_obj_t **title_out,lv_obj_t **value_out);
