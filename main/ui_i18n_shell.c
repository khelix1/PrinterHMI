#include "ui_i18n_shell.h"

#include "language_controller.h"

static const char *const s_english[UI_I18N_SHELL_COUNT] = {
    [UI_I18N_SHELL_NAV_DASHBOARD] = "Dashboard",
    [UI_I18N_SHELL_NAV_PRINTER] = "Printer",
    [UI_I18N_SHELL_NAV_FILES] = "Files",
    [UI_I18N_SHELL_NAV_BED_MESH] = "Bed Mesh",
    [UI_I18N_SHELL_NAV_CALIBRATION] = "Calibration",
    [UI_I18N_SHELL_NAV_DEVICES] = "Devices",
    [UI_I18N_SHELL_NAV_MACROS] = "Macros",
    [UI_I18N_SHELL_NAV_CONSOLE] = "Console",
    [UI_I18N_SHELL_NAV_DRYBOX] = "Drybox",
    [UI_I18N_SHELL_NAV_SETTINGS] = "Settings",
};

static const char *const s_spanish[UI_I18N_SHELL_COUNT] = {
    [UI_I18N_SHELL_NAV_DASHBOARD] = "Panel",
    [UI_I18N_SHELL_NAV_PRINTER] = "Impresora",
    [UI_I18N_SHELL_NAV_FILES] = "Archivos",
    [UI_I18N_SHELL_NAV_BED_MESH] = "Malla de cama",
    [UI_I18N_SHELL_NAV_CALIBRATION] = "Calibración",
    [UI_I18N_SHELL_NAV_DEVICES] = "Dispositivos",
    [UI_I18N_SHELL_NAV_MACROS] = "Macros",
    [UI_I18N_SHELL_NAV_CONSOLE] = "Consola",
    [UI_I18N_SHELL_NAV_DRYBOX] = "Secador",
    [UI_I18N_SHELL_NAV_SETTINGS] = "Ajustes",
};

const char *ui_i18n_shell_text(ui_i18n_shell_text_t text)
{
    if (text < 0 || text >= UI_I18N_SHELL_COUNT) return "";
    const char *const *catalog = language_controller_active() == UI_LANGUAGE_SPANISH
        ? s_spanish : s_english;
    return catalog[text] ? catalog[text] : s_english[text];
}
