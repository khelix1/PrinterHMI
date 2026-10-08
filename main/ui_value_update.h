#pragma once

#include <stdbool.h>
#include <string.h>
#include "lvgl.h"

/* Call under the display lock. Compare against the widget, so rebuilt pages,
 * accessibility and theme changes cannot leave a detached value cache behind.
 * A NULL text argument means an empty string (never LVGL's refresh-current API).
 */
static inline bool ui_value_set_text(lv_obj_t *label, const char *text)
{
    if (!label) return false;
    const char *next = text ? text : "";
    if (strcmp(lv_label_get_text(label), next) == 0) return false;
    lv_label_set_text(label, next);
    return true;
}

/* Preserve a local color override even if it currently equals an inherited
 * theme color; otherwise a later theme change could silently alter the value.
 */
static inline void ui_value_set_color(lv_obj_t *label, lv_color_t color,
    lv_style_selector_t selector)
{
    if (!label) return;
    lv_style_value_t current;
    if (lv_obj_get_local_style_prop(label, LV_STYLE_TEXT_COLOR, &current, selector) == LV_STYLE_RES_FOUND &&
        lv_color_eq(current.color, color)) return;
    lv_obj_set_style_text_color(label, color, selector);
}

/* Compare local properties, rather than resolved/inherited values, so later
 * theme changes retain the same explicit overrides as the original setters.
 */
static inline void ui_value_set_bg_color(lv_obj_t *obj, lv_color_t color,
    lv_style_selector_t selector)
{
    if (!obj) return;
    lv_style_value_t current;
    if (lv_obj_get_local_style_prop(obj, LV_STYLE_BG_COLOR, &current, selector) == LV_STYLE_RES_FOUND &&
        lv_color_eq(current.color, color)) return;
    lv_obj_set_style_bg_color(obj, color, selector);
}

static inline void ui_value_set_border_color(lv_obj_t *obj, lv_color_t color,
    lv_style_selector_t selector)
{
    if (!obj) return;
    lv_style_value_t current;
    if (lv_obj_get_local_style_prop(obj, LV_STYLE_BORDER_COLOR, &current, selector) == LV_STYLE_RES_FOUND &&
        lv_color_eq(current.color, color)) return;
    lv_obj_set_style_border_color(obj, color, selector);
}

static inline void ui_value_set_border_width(lv_obj_t *obj, int32_t width,
    lv_style_selector_t selector)
{
    if (!obj) return;
    lv_style_value_t current;
    if (lv_obj_get_local_style_prop(obj, LV_STYLE_BORDER_WIDTH, &current, selector) == LV_STYLE_RES_FOUND &&
        current.num == width) return;
    lv_obj_set_style_border_width(obj, width, selector);
}

static inline void ui_value_set_bg_opa(lv_obj_t *obj, lv_opa_t opa,
    lv_style_selector_t selector)
{
    if (!obj) return;
    lv_style_value_t current;
    if (lv_obj_get_local_style_prop(obj, LV_STYLE_BG_OPA, &current, selector) == LV_STYLE_RES_FOUND &&
        current.num == opa) return;
    lv_obj_set_style_bg_opa(obj, opa, selector);
}

/* Scalar local-style properties, including layout values and text alignment. */
static inline void ui_value_set_style_num(lv_obj_t *obj, lv_style_prop_t prop,
    int32_t value, lv_style_selector_t selector)
{
    if (!obj) return;
    lv_style_value_t current;
    if (lv_obj_get_local_style_prop(obj, prop, &current, selector) == LV_STYLE_RES_FOUND &&
        current.num == value) return;
    lv_obj_set_local_style_prop(obj, prop, (lv_style_value_t){.num = value}, selector);
}

/* Instantaneous bars only; do not suppress an animated update. */
static inline void ui_value_set_bar(lv_obj_t *bar, int32_t value)
{
    if (!bar) return;
    int32_t min = lv_bar_get_min_value(bar), max = lv_bar_get_max_value(bar);
    if (value < min) value = min;
    if (value > max) value = max;
    if (lv_bar_get_value(bar) != value) lv_bar_set_value(bar, value, LV_ANIM_OFF);
}

/* Shift-mode series only. Mutate through LVGL's public array/start APIs;
 * the caller refreshes each shared chart once after the entire batch.
 * Equal successive values still advance the time axis.
 */
static inline void ui_value_chart_append(lv_obj_t *chart,
    lv_chart_series_t *series, int32_t value)
{
    if (!chart || !series) return;
    uint32_t count = lv_chart_get_point_count(chart);
    if (!count) return;
    int32_t *values = lv_chart_get_series_y_array(chart, series);
    uint32_t start = lv_chart_get_x_start_point(chart, series);
    values[start] = value;
    lv_chart_set_x_start_point(chart, series, (start + 1) % count);
}
