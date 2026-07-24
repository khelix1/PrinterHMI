#include "printer_ui_controller.h"

#include <string.h>

#include "printer_controller.h"

static printer_ui_controller_send_gcode_cb_t
    s_send_gcode_cb = NULL;

static printer_ui_controller_action_cb_t
    s_show_cancel_cb = NULL;

static printer_ui_controller_action_cb_t
    s_show_object_cb = NULL;

static printer_ui_controller_action_cb_t
    s_show_motion_cb = NULL;

void printer_ui_controller_init(
    printer_ui_controller_send_gcode_cb_t send_gcode_cb,
    printer_ui_controller_action_cb_t show_cancel_cb,
    printer_ui_controller_action_cb_t show_object_cb,
    printer_ui_controller_action_cb_t show_motion_cb)
{
    s_send_gcode_cb = send_gcode_cb;
    s_show_cancel_cb = show_cancel_cb;
    s_show_object_cb = show_object_cb;
    s_show_motion_cb = show_motion_cb;
}

void printer_ui_controller_command_event_cb(lv_event_t *event)
{
    if (!event) {
        return;
    }

    const char *command =
        (const char *)lv_event_get_user_data(event);

    if (!command || !command[0]) {
        return;
    }

    if (strcmp(command, "CANCEL_PRINT") == 0) {
        if (s_show_cancel_cb) {
            s_show_cancel_cb();
        }

        return;
    }

    if (strcmp(command, "CANCEL_OBJECT") == 0) {
        if (s_show_object_cb) {
            s_show_object_cb();
        }

        return;
    }

    if (s_send_gcode_cb) {
        (void)s_send_gcode_cb(command);
    }
}

void printer_ui_controller_motion_event_cb(lv_event_t *event)
{
    if (!event ||
        lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }

    if (s_show_motion_cb) {
        s_show_motion_cb();
    }
}

void printer_ui_controller_update_action_buttons(
    lv_obj_t *home_button,
    lv_obj_t *pause_button,
    lv_obj_t *resume_button,
    lv_obj_t *object_button,
    lv_obj_t *cancel_button,
    bool object_available,
    const char *printer_state)
{
    printer_controller_update_action_buttons(
        home_button,
        pause_button,
        resume_button,
        cancel_button,
        printer_state);

    if (object_button) {
        bool enabled = object_available &&
            printer_controller_is_live_state(printer_state);

        if (enabled) {
            lv_obj_clear_state(object_button, LV_STATE_DISABLED);
            lv_obj_set_style_opa(object_button, LV_OPA_COVER, 0);
        } else {
            lv_obj_add_state(object_button, LV_STATE_DISABLED);
            lv_obj_set_style_opa(object_button, LV_OPA_50, 0);
        }
    }
}
