#pragma once

typedef enum {
    UI_I18N_COMMON_CLOSE = 0,
    UI_I18N_COMMON_CANCEL,
    UI_I18N_COMMON_SAVE,
    UI_I18N_COMMON_RETRY,
    UI_I18N_COMMON_ERROR,
    UI_I18N_COMMON_WARNING,
    UI_I18N_COMMON_COUNT
} ui_i18n_common_text_t;

const char *ui_i18n_common_text(ui_i18n_common_text_t text);
