#pragma once

#include "lvgl.h"
#include "language_controller.h"

typedef enum {
    UI_TEXT_NAV_DASHBOARD = 0,
    UI_TEXT_NAV_PRINTER,
    UI_TEXT_NAV_FILES,
    UI_TEXT_NAV_BED_MESH,
    UI_TEXT_NAV_CALIBRATION,
    UI_TEXT_NAV_DEVICES,
    UI_TEXT_NAV_MACROS,
    UI_TEXT_NAV_CONSOLE,
    UI_TEXT_NAV_DRYBOX,
    UI_TEXT_NAV_SETTINGS,
    UI_TEXT_LANGUAGE_SECTION,
    UI_TEXT_LANGUAGE,
    UI_TEXT_LANGUAGE_DESCRIPTION,
    UI_TEXT_LANGUAGE_PICKER_TITLE,
    UI_TEXT_LANGUAGE_PICKER_BODY,
    UI_TEXT_CLOSE,
    UI_TEXT_COUNT
} ui_text_id_t;

const char *ui_i18n_text(ui_text_id_t text);
const lv_font_t *ui_i18n_text_font(const lv_font_t *default_font);
const lv_font_t *ui_i18n_language_font(
    ui_language_id_t language,
    const lv_font_t *default_font);
