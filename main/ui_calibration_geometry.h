#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "lvgl.h"
#include "calibration_session_controller.h"
typedef bool (*ui_calibration_geometry_send_gcode_cb_t)(const char *command);
typedef struct { lv_obj_t **screws_popup, **gantry_popup, **screws_results_label; bool *screws_home_required,*gantry_home_required,*gantry_use_qgl; char *screws_display; size_t screws_display_size; calibration_session_snapshot_t *session_snapshot; uint32_t *session_generation; ui_calibration_geometry_send_gcode_cb_t send_gcode; } ui_calibration_geometry_context_t;
void ui_calibration_geometry_init(ui_calibration_geometry_context_t *context);
void ui_calibration_geometry_screws_event(lv_event_t *event);
void ui_calibration_geometry_gantry_event(lv_event_t *event);
void ui_calibration_geometry_refresh(void);
void ui_calibration_geometry_close(void);
