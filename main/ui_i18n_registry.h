#pragma once

#include <stdbool.h>

#include "language_controller.h"

/*
 * Every operator-facing feature owns a catalog module.  A language can be
 * selected only when all required modules are complete; untranslated text is
 * never presented as a half-localized production interface.
 */
typedef enum {
    UI_I18N_MODULE_COMMON = 0,
    UI_I18N_MODULE_SHELL,
    UI_I18N_MODULE_SETTINGS,
    UI_I18N_MODULE_DASHBOARD,
    UI_I18N_MODULE_PRINTER,
    UI_I18N_MODULE_FILES,
    UI_I18N_MODULE_BED_MESH,
    UI_I18N_MODULE_CALIBRATION,
    UI_I18N_MODULE_DEVICES,
    UI_I18N_MODULE_MACROS,
    UI_I18N_MODULE_CONSOLE,
    UI_I18N_MODULE_DRYBOX,
    UI_I18N_MODULE_NETWORK,
    UI_I18N_MODULE_POPUPS,
    UI_I18N_MODULE_COUNT
} ui_i18n_module_t;

bool ui_i18n_language_module_is_complete(
    ui_language_id_t language,
    ui_i18n_module_t module);
bool ui_i18n_language_is_selectable(ui_language_id_t language);
