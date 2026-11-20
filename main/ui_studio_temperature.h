#pragma once

#include <stdio.h>
#include <string.h>

#include "ui_studio_layout.h"
#include "ui_temperature_reading.h"
#include "ui_value_update.h"

static inline lv_obj_t *ui_studio_temperature_reading(lv_obj_t *parent, int x, int y, int width)
{
    return studio_text(parent, "--", x, y, width, &ui_studio_font_48, UI_TEXT);
}

static inline lv_obj_t *ui_studio_temperature_setpoint(lv_obj_t *parent, int x, int y, int width)
{
    return studio_text(parent, "", x, y, width, &ui_studio_font_18, UI_TEXT_DIM);
}

/* Keep the current temperature at 48 px and put /target beside it in the
 * smaller setpoint font. Long values can put the suffix below without clipping. */
static inline void ui_studio_set_temperature(lv_obj_t *reading, lv_obj_t *setpoint,
    const char *pair)
{
    if (!pair) pair = UI_TEMPERATURE_PAIR_UNKNOWN;
    const char *slash = strchr(pair, '/');
    if (!slash || !strcmp(pair, "N/A")) {
        ui_value_set_text(reading, pair);
        ui_value_set_text(setpoint, "");
        return;
    }
    char current[20];
    snprintf(current, sizeof(current), "%.*s", (int)(slash - pair), pair);
    bool changed = ui_value_set_text(reading, current);
    changed |= ui_value_set_text(setpoint, slash);
    if (!changed) return;

    const lv_font_t *large = lv_obj_get_style_text_font(reading, 0);
    const lv_font_t *small = lv_obj_get_style_text_font(setpoint, 0);
    lv_point_t width, suffix;
    lv_text_get_size(&width, current, large, lv_obj_get_style_text_letter_space(reading, 0),
        0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    lv_text_get_size(&suffix, slash, small, lv_obj_get_style_text_letter_space(setpoint, 0),
        0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    int32_t x = lv_obj_get_style_x(reading, 0);
    int32_t y = lv_obj_get_style_y(reading, 0);
    int32_t span = lv_obj_get_style_width(reading, 0);
    int32_t offset = width.x + 4;
    if (offset + suffix.x > span) {
        lv_obj_set_pos(setpoint, x, y + large->line_height);
        lv_obj_set_width(setpoint, span);
    } else {
        lv_obj_set_pos(setpoint, x + offset,
            y + large->line_height - large->base_line - small->line_height + small->base_line);
        lv_obj_set_width(setpoint, span - offset);
    }
}
