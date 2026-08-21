#pragma once

#include "lvgl.h"

/* Returns a cached descriptor whose missing glyphs come from Montserrat 14. */
const lv_font_t *ui_font_with_fallback(const lv_font_t *base);
