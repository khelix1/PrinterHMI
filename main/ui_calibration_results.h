#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "calibration_session_controller.h"
#include "lvgl.h"

typedef bool (*ui_calibration_results_send_gcode_cb_t)(const char *command);

typedef struct {
    lv_obj_t **save_confirm_popup;
    lv_obj_t **results_popup;
    lv_obj_t **results_label;
    lv_obj_t **apply_restart_button;
    calibration_session_snapshot_t *session_snapshot;
    uint32_t *session_generation;
    char *display;
    size_t display_size;
    ui_calibration_results_send_gcode_cb_t send_gcode;
} ui_calibration_results_context_t;

void ui_calibration_results_init(ui_calibration_results_context_t *context);
void ui_calibration_results_show(const char *title, const char *waiting_text);
void ui_calibration_results_refresh(void);
void ui_calibration_results_close(void);
