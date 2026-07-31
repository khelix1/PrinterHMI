#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "bed_mesh_controller.h"
#include "lvgl.h"
#include "ui_bed_mesh_view.h"

typedef struct {
    lv_obj_t *canvas;
    uint16_t *buf;
    const float *values;
    bed_mesh_snapshot_t mesh;
    ui_bed_mesh_view_transform_t view;
    float zscale;
    bool surface_grid_visible;
    bool interaction_active;
} ui_bed_mesh_renderer_config_t;

void ui_bed_mesh_renderer_render(
    const ui_bed_mesh_renderer_config_t *config);
