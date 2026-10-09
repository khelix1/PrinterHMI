#pragma once
#include "lvgl.h"
#include "ui_font_fallback.h"

/* Atomic readings and short headings must never split across lines.
 * Use the largest enabled font that fits; long names use scrolling instead. */
static inline void ui_text_fit_single_line(lv_obj_t *label, const lv_font_t *preferred)
{
    if (!label || !preferred) return;
    lv_obj_update_layout(label);
    const lv_font_t *fonts[] = { preferred, &lv_font_montserrat_24,
        &lv_font_montserrat_22, &lv_font_montserrat_20,
        &lv_font_montserrat_18, &lv_font_montserrat_16, &lv_font_montserrat_14 };
    const lv_font_t *selected = &lv_font_montserrat_14;
    int32_t width = lv_obj_get_content_width(label);
    for (unsigned i = 0; i < sizeof(fonts) / sizeof(fonts[0]); ++i) {
        if (fonts[i]->line_height > preferred->line_height) continue;
        lv_point_t size;
        const lv_font_t *font = ui_font_with_fallback(fonts[i]);
        lv_text_get_size(&size, lv_label_get_text(label), font,
            lv_obj_get_style_text_letter_space(label, 0), 0,
            LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        if (size.x <= width) { selected = fonts[i]; break; }
    }
    const lv_font_t *font = ui_font_with_fallback(selected);
    if (lv_obj_get_style_text_font(label, 0) != font)
        lv_obj_set_style_text_font(label, font, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);
}
