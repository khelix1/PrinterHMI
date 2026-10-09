#pragma once
#include "lvgl.h"
/* Native line geometry, independent of the symbol font. */
lv_obj_t *ui_studio_icon_create(lv_obj_t *parent, unsigned icon);
void ui_studio_icon_color(lv_obj_t *icon, lv_color_t color);
