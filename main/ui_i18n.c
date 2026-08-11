#include "ui_i18n.h"

#include "language_controller.h"
#include "ui_font_spanish_16.h"

static const char *const s_english[UI_TEXT_COUNT] = {
    [UI_TEXT_NAV_DASHBOARD] = "Dashboard",
    [UI_TEXT_NAV_PRINTER] = "Printer",
    [UI_TEXT_NAV_FILES] = "Files",
    [UI_TEXT_NAV_BED_MESH] = "Bed Mesh",
    [UI_TEXT_NAV_CALIBRATION] = "Calibration",
    [UI_TEXT_NAV_DEVICES] = "Devices",
    [UI_TEXT_NAV_MACROS] = "Macros",
    [UI_TEXT_NAV_CONSOLE] = "Console",
    [UI_TEXT_NAV_DRYBOX] = "Drybox",
    [UI_TEXT_NAV_SETTINGS] = "Settings",
    [UI_TEXT_LANGUAGE_SECTION] = "LANGUAGE",
    [UI_TEXT_LANGUAGE] = "Language",
    [UI_TEXT_LANGUAGE_DESCRIPTION] = "Choose the interface language",
    [UI_TEXT_LANGUAGE_PICKER_TITLE] = "LANGUAGE",
    [UI_TEXT_LANGUAGE_PICKER_BODY] = "Choose the interface language. Language names always remain in their own language.",
    [UI_TEXT_CLOSE] = "CLOSE",
};

static const char *const s_spanish[UI_TEXT_COUNT] = {
    [UI_TEXT_NAV_DASHBOARD] = "Panel",
    [UI_TEXT_NAV_PRINTER] = "Impresora",
    [UI_TEXT_NAV_FILES] = "Archivos",
    [UI_TEXT_NAV_BED_MESH] = "Malla de cama",
    [UI_TEXT_NAV_CALIBRATION] = "Calibración",
    [UI_TEXT_NAV_DEVICES] = "Dispositivos",
    [UI_TEXT_NAV_MACROS] = "Macros",
    [UI_TEXT_NAV_CONSOLE] = "Consola",
    [UI_TEXT_NAV_DRYBOX] = "Secador",
    [UI_TEXT_NAV_SETTINGS] = "Ajustes",
    [UI_TEXT_LANGUAGE_SECTION] = "IDIOMA",
    [UI_TEXT_LANGUAGE] = "Idioma",
    [UI_TEXT_LANGUAGE_DESCRIPTION] = "Elija el idioma de la interfaz",
    [UI_TEXT_LANGUAGE_PICKER_TITLE] = "IDIOMA",
    [UI_TEXT_LANGUAGE_PICKER_BODY] = "Elija el idioma de la interfaz. Los nombres de idioma siempre aparecen en su propio idioma.",
    [UI_TEXT_CLOSE] = "CERRAR",
};

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

const char *ui_i18n_text(ui_text_id_t text)
{
    if (text < 0 || text >= UI_TEXT_COUNT) return "";
    const char *const *catalog = language_controller_active() == UI_LANGUAGE_SPANISH
        ? s_spanish : s_english;
    return catalog[text] ? catalog[text] : s_english[text];
}
