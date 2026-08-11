#include "ui_i18n_common.h"

#include "language_controller.h"

static const char *const s_english[UI_I18N_COMMON_COUNT] = {
    [UI_I18N_COMMON_CLOSE] = "CLOSE",
    [UI_I18N_COMMON_CANCEL] = "CANCEL",
    [UI_I18N_COMMON_SAVE] = "SAVE",
    [UI_I18N_COMMON_RETRY] = "RETRY",
    [UI_I18N_COMMON_ERROR] = "ERROR",
    [UI_I18N_COMMON_WARNING] = "WARNING",
};

static const char *const s_spanish[UI_I18N_COMMON_COUNT] = {
    [UI_I18N_COMMON_CLOSE] = "CERRAR",
    [UI_I18N_COMMON_CANCEL] = "CANCELAR",
    [UI_I18N_COMMON_SAVE] = "GUARDAR",
    [UI_I18N_COMMON_RETRY] = "REINTENTAR",
    [UI_I18N_COMMON_ERROR] = "ERROR",
    [UI_I18N_COMMON_WARNING] = "ADVERTENCIA",
};

const char *ui_i18n_common_text(ui_i18n_common_text_t text)
{
    if (text < 0 || text >= UI_I18N_COMMON_COUNT) return "";
    const char *const *catalog = language_controller_active() == UI_LANGUAGE_SPANISH
        ? s_spanish : s_english;
    return catalog[text] ? catalog[text] : s_english[text];
}
