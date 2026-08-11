#pragma once

typedef enum {
    UI_I18N_SETTINGS_LANGUAGE_SECTION = 0,
    UI_I18N_SETTINGS_LANGUAGE,
    UI_I18N_SETTINGS_LANGUAGE_DESCRIPTION,
    UI_I18N_SETTINGS_PICKER_TITLE,
    UI_I18N_SETTINGS_PICKER_BODY,
    UI_I18N_SETTINGS_COUNT
} ui_i18n_settings_text_t;

const char *ui_i18n_settings_text(ui_i18n_settings_text_t text);
