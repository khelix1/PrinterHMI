#include "ui_i18n_settings.h"

#include "language_controller.h"

static const char *const s_english[UI_I18N_SETTINGS_COUNT] = {
    [UI_I18N_SETTINGS_LANGUAGE_SECTION] = "LANGUAGE",
    [UI_I18N_SETTINGS_LANGUAGE] = "Language",
    [UI_I18N_SETTINGS_LANGUAGE_DESCRIPTION] = "Choose the interface language",
    [UI_I18N_SETTINGS_PICKER_TITLE] = "LANGUAGE",
    [UI_I18N_SETTINGS_PICKER_BODY] =
        "Choose the interface language. Language names always remain in their own language.",
};

static const char *const s_spanish[UI_I18N_SETTINGS_COUNT] = {
    [UI_I18N_SETTINGS_LANGUAGE_SECTION] = "IDIOMA",
    [UI_I18N_SETTINGS_LANGUAGE] = "Idioma",
    [UI_I18N_SETTINGS_LANGUAGE_DESCRIPTION] = "Elija el idioma de la interfaz",
    [UI_I18N_SETTINGS_PICKER_TITLE] = "IDIOMA",
    [UI_I18N_SETTINGS_PICKER_BODY] =
        "Elija el idioma de la interfaz. Los nombres de idioma siempre aparecen en su propio idioma.",
};

const char *ui_i18n_settings_text(ui_i18n_settings_text_t text)
{
    if (text < 0 || text >= UI_I18N_SETTINGS_COUNT) return "";
    const char *const *catalog = language_controller_active() == UI_LANGUAGE_SPANISH
        ? s_spanish : s_english;
    return catalog[text] ? catalog[text] : s_english[text];
}
