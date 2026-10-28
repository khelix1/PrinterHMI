#pragma once

#include <stdbool.h>

#include "lvgl.h"
#include "ui_printer_actions.h"
#include "ui_printer_info_cards.h"
#include "moonraker.h"

typedef bool (*printer_ui_controller_send_gcode_cb_t)(
    const char *command);

typedef void (*printer_ui_controller_action_cb_t)(void);

typedef struct {
    lv_obj_t *printer_panel;
    lv_obj_t *banner_label;
    lv_obj_t *speed_label;
    lv_obj_t *flow_label;
    lv_obj_t *layer_label;
    lv_obj_t *filament_label;

    lv_obj_t *motion_button;
    lv_obj_t *home_button;
    lv_obj_t *pause_button;
    lv_obj_t *resume_button;
    lv_obj_t *object_button;
    lv_obj_t *cancel_button;

    ui_printer_info_cards_t *info_cards;

    const char *banner_text;
    const char *printer_state;
    const char *printer_file;
    const char *selected_preview_file;

    double live_velocity;
    double live_flow;
    double speed_factor;
    double flow_factor;
    int current_layer;
    int total_layer;
    double progress;
    double nozzle_temp;
    double nozzle_target;
    double bed_temp;
    double bed_target;
    double part_fan_speed;
    double print_duration;

    bool moonraker_ok;
    bool live_data_ok;
    bool exclude_objects_available;

    const moonraker_capabilities_t *capabilities;
    const moonraker_filament_state_t *filament_state;
} printer_ui_controller_refresh_ctx_t;

void printer_ui_controller_refresh(
    const printer_ui_controller_refresh_ctx_t *ctx);

void printer_ui_controller_init(
    printer_ui_controller_send_gcode_cb_t send_gcode_cb,
    printer_ui_controller_action_cb_t show_cancel_cb,
    printer_ui_controller_action_cb_t show_object_cb,
    printer_ui_controller_action_cb_t show_motion_cb);

/* Bind action targets to the printer displayed when this page was created. */
void printer_ui_controller_bind_actions(const ui_printer_actions_t *actions);

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
