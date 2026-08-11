#pragma once

#include "lvgl.h"
#include "language_controller.h"

const lv_font_t *ui_i18n_text_font(const lv_font_t *default_font);
const lv_font_t *ui_i18n_language_font(
    ui_language_id_t language,
    const lv_font_t *default_font);
