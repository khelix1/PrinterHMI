#include "ui_i18n.h"

#include "ui_font_spanish_16.h"

const lv_font_t *ui_i18n_language_font(
    ui_language_id_t language,
    const lv_font_t *default_font)
{
    return language == UI_LANGUAGE_SPANISH
        ? &ui_font_spanish_16
        : default_font;
}

const lv_font_t *ui_i18n_text_font(const lv_font_t *default_font)
{
    return ui_i18n_language_font(language_controller_active(), default_font);
}
