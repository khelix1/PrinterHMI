#pragma once

#include <stdbool.h>

#include "lvgl.h"

typedef bool (*printer_ui_controller_send_gcode_cb_t)(
    const char *command);

typedef void (*printer_ui_controller_action_cb_t)(void);

void printer_ui_controller_init(
    printer_ui_controller_send_gcode_cb_t send_gcode_cb,
    printer_ui_controller_action_cb_t show_cancel_cb,
    printer_ui_controller_action_cb_t show_object_cb,
    printer_ui_controller_action_cb_t show_motion_cb);

void printer_ui_controller_command_event_cb(lv_event_t *event);
void printer_ui_controller_motion_event_cb(lv_event_t *event);

void printer_ui_controller_update_action_buttons(
    lv_obj_t *home_button,
    lv_obj_t *pause_button,
    lv_obj_t *resume_button,
    lv_obj_t *object_button,
    lv_obj_t *cancel_button,
    bool object_available,
    const char *printer_state);
