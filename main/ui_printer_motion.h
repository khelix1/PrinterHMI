#pragma once

#include "lvgl.h"
#include <stdbool.h>
#include <stddef.h>

/*
 * Compatibility boundary for existing Printer motion callers.
 *
 * The complete implementation now lives in ui_printer_toolhead.
 * New Toolhead-specific work should use ui_printer_toolhead.h directly.
 */

void ui_printer_motion_init(void);

bool ui_printer_motion_format_jog_command(const char *axis,
                                          double jog_step,
                                          char *cmd,
                                          size_t cmd_size);

bool ui_printer_motion_format_extrude_command(const char *dir,
                                              char *cmd,
                                              size_t cmd_size);

bool ui_printer_motion_format_z_offset_command(const char *adjustment,
                                               char *cmd,
                                               size_t cmd_size);

typedef void (*ui_printer_motion_send_gcode_cb_t)(const char *cmd);

void ui_printer_motion_show(lv_obj_t **step1_btn,
                            lv_obj_t **step10_btn,
                            lv_obj_t **step50_btn,
                            double *jog_step,
                            ui_printer_motion_send_gcode_cb_t send_gcode_cb);
