#include "ui_printer_actions.h"
#include "ui_i18n_common.h"

#include "ui_button.h"
#include "ui_theme.h"


/*
 * Printer action buttons use the same Theme B component as
 * Dashboard and Drybox.
 *
 * Shared widget owns:
 *
 *   - dark control surface
 *   - outlined frame
 *   - radius
 *   - pressed state
 *   - separate icon and text labels
 *
 * Printer owns:
 *
 *   - geometry
 *   - command data
 *   - event callback
 *   - semantic icon accent
 */
static lv_obj_t *make_action_button(
    lv_obj_t *parent,
    const char *symbol,
    const char *text,
    const char *cmd,
    int x,
    int y,
    int width,
    int height,
    lv_color_t icon_color,
    lv_event_cb_t callback)
{
    lv_obj_t *button =
        ui_button_create_icon(
            parent,
            UI_BUTTON_OUTLINED,
            symbol,
            text,
            icon_color,
            UI_BUTTON_ICON_HORIZONTAL);

    if (!button) {
        return NULL;
    }

    lv_obj_set_size(
        button,
        width,
        height);

    lv_obj_set_pos(
        button,
        x,
        y);

    if (callback) {
        lv_obj_add_event_cb(
            button,
            callback,
            LV_EVENT_CLICKED,
            (void *)cmd);
    }

    return button;
}


void ui_printer_actions_create(
    lv_obj_t *parent,
    ui_printer_actions_t *actions,
    lv_event_cb_t command_cb,
    lv_event_cb_t motion_cb)
{
    if (!parent || !actions) {
        return;
    }

    actions->motion =
        make_action_button(
            parent,
            LV_SYMBOL_SETTINGS,
            "MOTION",
            "MOTION",
            0,
            2,
            125,
            50,
            UI_ACCENT_CYAN,
            motion_cb);

    actions->home =
        make_action_button(
            parent,
            LV_SYMBOL_HOME,
            "HOME",
            "HOME_ALL",
            135,
            2,
            125,
            50,
            UI_ACCENT_INFO,
            command_cb);

    actions->pause =
        make_action_button(
            parent,
            LV_SYMBOL_PAUSE,
            "PAUSE",
            "PAUSE",
            270,
            2,
            125,
            50,
            UI_WARN,
            command_cb);

    actions->resume =
        make_action_button(
            parent,
            LV_SYMBOL_PLAY,
            "RESUME",
            "RESUME",
            405,
            2,
            125,
            50,
            UI_OK_BRIGHT,
            command_cb);

    actions->object =
        make_action_button(
            parent,
            LV_SYMBOL_LIST,
            "OBJECT",
            "CANCEL_OBJECT",
            540,
            2,
            125,
            50,
            UI_WARN,
            command_cb);

    actions->cancel =
        make_action_button(
            parent,
            LV_SYMBOL_STOP,
            ui_i18n_common_text(UI_I18N_COMMON_CANCEL),
            "CANCEL_PRINT",
            675,
            2,
            125,
            50,
            UI_DANGER_BRIGHT,
            command_cb);
}
