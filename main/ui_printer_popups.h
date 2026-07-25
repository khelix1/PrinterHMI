#pragma once

#include "lvgl.h"
#include "moonraker.h"

typedef void (*ui_printer_popups_send_gcode_cb_t)(const char *cmd);

void ui_printer_popups_show_cancel(ui_printer_popups_send_gcode_cb_t send_cb);
void ui_printer_popups_show_cancel_object(
    ui_printer_popups_send_gcode_cb_t send_cb);

void ui_printer_popups_show_part_fan(ui_printer_popups_send_gcode_cb_t send_cb,
                                     double current_fan_percent);

void ui_printer_popups_show_nozzle(ui_printer_popups_send_gcode_cb_t send_cb,
                                   double current_temp,
                                   double target_temp);

void ui_printer_popups_show_hotends(
    ui_printer_popups_send_gcode_cb_t send_cb,
    const moonraker_state_t *state);

void ui_printer_popups_show_bed(ui_printer_popups_send_gcode_cb_t send_cb,
                                double current_temp,
                                double target_temp);

void ui_printer_popups_show_filament_sensors(
    const moonraker_filament_state_t *state);

void ui_printer_popups_show_printer_status(const char *state,
                                           const char *file,
                                           const char *progress,
                                           const char *elapsed,
                                           const char *remaining,
                                           double nozzle_temp,
                                           double nozzle_target,
                                           double bed_temp,
                                           double bed_target,
                                           bool moonraker_connected);

void ui_printer_popups_close_all(void);
