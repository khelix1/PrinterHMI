#pragma once
#include <stdbool.h>
#include <stddef.h>
#include "lvgl.h"
#include "macro_controller.h"
typedef bool (*ui_calibration_custom_send_cb_t)(const char *);
typedef bool (*ui_calibration_custom_ready_cb_t)(const char *);
typedef void (*ui_calibration_custom_results_cb_t)(const char *,const char *);
typedef void (*ui_calibration_custom_refresh_cb_t)(void);
typedef struct { lv_obj_t **popup; char (*names)[MACRO_CONTROLLER_NAME_MAX]; size_t names_capacity,*count,*selected; ui_calibration_custom_send_cb_t send; ui_calibration_custom_ready_cb_t ready; ui_calibration_custom_results_cb_t show_results; ui_calibration_custom_refresh_cb_t refresh_results; } ui_calibration_custom_context_t;
void ui_calibration_custom_init(ui_calibration_custom_context_t *context);
void ui_calibration_custom_event(lv_event_t *event);
void ui_calibration_custom_close(void);
