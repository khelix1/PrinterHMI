#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

lv_obj_t *telemetry_make_label(
    lv_obj_t *parent,
    const char *text,
    const lv_font_t *font,
    lv_color_t color);

lv_obj_t *telemetry_create_metric_card(
    lv_obj_t *parent,
    int x,
    const char *title,
    lv_color_t accent,
    lv_obj_t **value_out);

void telemetry_create_legend_item(
    lv_obj_t *parent,
    int x,
    int y,
    lv_color_t color,
    const char *text);

#ifdef __cplusplus
}
#endif
