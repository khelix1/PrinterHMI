#pragma once

#include "lvgl.h"
#include <stdbool.h>
#include <stddef.h>

/*
 * ui_printer_motion
 *
 * Owns Printer motion popup UI.
 *
 * Current phase:
 * - Boundary file only.
 *
 * Future ownership:
 * - Motion popup creation/cleanup
 * - Jog step buttons
 * - X/Y/Z movement buttons
 * - Home controls
 */

void ui_printer_motion_init(void);


lv_obj_t *ui_printer_motion_button(lv_obj_t *parent,
                                   const char *text,
                                   int x,
                                   int y,
                                   int w,
                                   int h,
                                   lv_event_cb_t cb,
                                   const char *user);

void ui_printer_motion_update_step_highlight(lv_obj_t *step1_btn,
                                             lv_obj_t *step10_btn,
                                             lv_obj_t *step50_btn,
                                             double jog_step);

bool ui_printer_motion_format_jog_command(const char *axis,
                                          double jog_step,
                                          char *cmd,
                                          size_t cmd_size);

bool ui_printer_motion_format_extrude_command(const char *dir,
                                              char *cmd,
                                              size_t cmd_size);

void ui_printer_motion_show_popup(lv_obj_t **step1_btn,
                                  lv_obj_t **step10_btn,
                                  lv_obj_t **step50_btn,
                                  double jog_step,
                                  lv_event_cb_t jog_cb,
                                  lv_event_cb_t step_cb,
                                  lv_event_cb_t extrude_cb,
                                  lv_event_cb_t close_cb);


typedef void (*ui_printer_motion_send_gcode_cb_t)(const char *cmd);

void ui_printer_motion_show(lv_obj_t **step1_btn,
                            lv_obj_t **step10_btn,
                            lv_obj_t **step50_btn,
                            double *jog_step,
                            ui_printer_motion_send_gcode_cb_t send_gcode_cb);
