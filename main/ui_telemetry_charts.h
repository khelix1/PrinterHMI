#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

void ui_telemetry_charts_create(lv_obj_t *parent);

void ui_telemetry_charts_load_history(void);

void ui_telemetry_charts_push_sample(
    double nozzle_temp,
    double bed_temp,
    double chamber_temp,
    double humidity);

void ui_telemetry_charts_reset(void);

#ifdef __cplusplus
}
#endif
