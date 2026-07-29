#pragma once

typedef void (*ui_calibration_open_bed_mesh_cb_t)(void);

void ui_calibration_v32_show(
    ui_calibration_open_bed_mesh_cb_t open_bed_mesh_cb);

void ui_calibration_v32_hide(void);
