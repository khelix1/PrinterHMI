#pragma once
#include "lvgl.h"
#include "telemetry_history.h"
typedef enum {UI_TELEMETRY_HEAT,UI_TELEMETRY_MOTION,UI_TELEMETRY_ENVIRONMENT} ui_telemetry_view_t;
void ui_telemetry_charts_create(lv_obj_t *parent);
void ui_telemetry_charts_configure(ui_telemetry_view_t view,unsigned points,const char *hotend,bool include_targets);
void ui_telemetry_charts_update_live(const telemetry_sample_t *sample,bool live,bool held);
void ui_telemetry_charts_refresh(int64_t end_us,bool held);
void ui_telemetry_charts_reset(void);
