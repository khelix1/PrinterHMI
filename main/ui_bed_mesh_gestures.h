#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"
#include "ui_bed_mesh_view.h"

typedef void (*ui_bed_mesh_gesture_render_cb_t)(void);

void ui_bed_mesh_gestures_init(
    ui_bed_mesh_view_transform_t *transform,
    ui_bed_mesh_gesture_render_cb_t render_cb,
    int32_t canvas_width,
    int32_t canvas_height);

void ui_bed_mesh_gestures_close(void);

void ui_bed_mesh_gestures_multitouch_update(
    uint8_t count,
    int32_t x0,
    int32_t y0,
    int32_t x1,
    int32_t y1);

void ui_bed_mesh_gestures_event_cb(
    lv_event_t *event);

void ui_bed_mesh_gestures_reset(void);
void ui_bed_mesh_gestures_zoom_in(void);
void ui_bed_mesh_gestures_zoom_out(void);

bool ui_bed_mesh_gestures_is_active(void);
