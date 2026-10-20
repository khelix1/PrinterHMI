#pragma once
#include "ui_theme.h"
/* Touch feedback changes color, never button geometry or an external halo.
 * LVGL's default button theme otherwise grows pressed surfaces by 3 px. */
static inline void ui_button_stable_press(lv_obj_t *button)
{
    if(!button)return;
    lv_obj_set_style_transform_width(button,0,LV_STATE_PRESSED);
    lv_obj_set_style_transform_height(button,0,LV_STATE_PRESSED);
    lv_obj_set_style_translate_y(button,0,LV_STATE_PRESSED);
    lv_obj_set_style_outline_width(button,0,LV_STATE_FOCUSED);
    lv_obj_set_style_outline_width(button,0,LV_STATE_PRESSED);
    lv_obj_set_style_outline_width(button,2,LV_STATE_FOCUS_KEY);
    lv_obj_set_style_outline_pad(button,2,LV_STATE_FOCUS_KEY);
    lv_obj_set_style_outline_color(button,UI_ACCENT_BRIGHT,LV_STATE_FOCUS_KEY);
    lv_obj_set_style_outline_width(button,0,LV_STATE_DISABLED);
}
