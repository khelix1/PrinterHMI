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
