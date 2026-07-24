#pragma once

#include "lvgl.h"

void printer_controller_format_status_symbol_text(char *out,
                                                  size_t out_len,
                                                  const char *state,
                                                  bool moonraker_ok,
                                                  bool live_data_ok);

const char *printer_controller_status_text(const char *state);
const char *printer_controller_machine_banner_text(const char *state, bool moonraker_ok);


lv_color_t printer_controller_state_text_color(const char *state);

bool printer_controller_state_is(const char *state, const char *want);
bool printer_controller_is_printing(const char *state);
bool printer_controller_is_paused(const char *state);
bool printer_controller_is_ready(const char *state);
bool printer_controller_is_error(const char *state);
bool printer_controller_is_live_state(const char *state);
bool printer_controller_has_active_job(const char *state, const char *file);

void printer_controller_format_eta_clock(char *out,
                                         size_t out_len,
                                         double progress,
                                         double print_duration_seconds);

void printer_controller_format_topbar_eta(char *out,
                                           size_t out_len,
                                           double progress,
                                           double print_duration_seconds,
                                           const char *remaining_text,
                                           bool moonraker_ok);

void printer_controller_format_hhmm(char *out,
                                    size_t out_len,
                                    double seconds);

void printer_controller_format_remaining(char *out,
                                         size_t out_len,
                                         double progress,
                                         double print_duration_seconds);

void printer_controller_update_action_buttons(lv_obj_t *home_btn,
                                              lv_obj_t *pause_btn,
                                              lv_obj_t *resume_btn,
                                              lv_obj_t *cancel_btn,
                                              const char *printer_state);
