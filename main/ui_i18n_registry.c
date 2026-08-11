#include "ui_i18n_registry.h"

#include <stdint.h>

#define UI_I18N_MODULE_BIT(module) (UINT32_C(1) << (module))
#define UI_I18N_REQUIRED_MODULES ((UINT32_C(1) << UI_I18N_MODULE_COUNT) - 1U)

/*
 * Grow this mask when a Spanish catalog module is complete and target-tested.
 * The existing Spanish shell/settings/common strings are deliberately tracked
 * here, but the pack remains unavailable until every operational module is.
 */
#define UI_I18N_SPANISH_COMPLETE_MODULES ( \
    UI_I18N_MODULE_BIT(UI_I18N_MODULE_COMMON) | \
    UI_I18N_MODULE_BIT(UI_I18N_MODULE_SHELL) | \
    UI_I18N_MODULE_BIT(UI_I18N_MODULE_SETTINGS))

static uint32_t completed_modules(ui_language_id_t language)
{
    switch (language) {
        case UI_LANGUAGE_ENGLISH:
            return UI_I18N_REQUIRED_MODULES;
        case UI_LANGUAGE_SPANISH:
            return UI_I18N_SPANISH_COMPLETE_MODULES;
        default:
            return 0;
    }
}

bool ui_i18n_language_module_is_complete(
    ui_language_id_t language,
    ui_i18n_module_t module)
{
    if (language >= UI_LANGUAGE_COUNT || module >= UI_I18N_MODULE_COUNT) {
        return false;
    }
    return (completed_modules(language) & UI_I18N_MODULE_BIT(module)) != 0;
}

bool ui_i18n_language_is_selectable(ui_language_id_t language)
{
    if (language >= UI_LANGUAGE_COUNT) return false;
    return completed_modules(language) == UI_I18N_REQUIRED_MODULES;
}
