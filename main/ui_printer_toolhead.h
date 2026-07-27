#pragma once

#include "lvgl.h"
#include <stdbool.h>
#include <stddef.h>

/*
 * Focused owner for the Printer Toolhead controls.
 *
 * Reuses:
 * - ui_popup for modal structure and footer actions
 * - ui_button for all operator buttons and semantic button kinds
 * - ui_theme for typography and semantic colors
 * - moonraker_state_snapshot for synchronized live state
 *
 * Owns only Toolhead-specific layout, interaction state, and G-code formatting.
 */

void ui_printer_toolhead_init(void);

bool ui_printer_toolhead_format_jog_command(const char *axis,
                                            double jog_step,
                                            char *cmd,
                                            size_t cmd_size);

bool ui_printer_toolhead_format_extrude_command(const char *dir,
                                                char *cmd,
                                                size_t cmd_size);

bool ui_printer_toolhead_format_z_offset_command(const char *adjustment,
                                                 char *cmd,
                                                 size_t cmd_size);

typedef void (*ui_printer_toolhead_send_gcode_cb_t)(const char *cmd);

void ui_printer_toolhead_show(lv_obj_t **step1_btn,
                              lv_obj_t **step10_btn,
                              lv_obj_t **step50_btn,
                              double *jog_step,
                              ui_printer_toolhead_send_gcode_cb_t send_gcode_cb);
