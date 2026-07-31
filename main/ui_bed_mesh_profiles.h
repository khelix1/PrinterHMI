#pragma once

#include "bed_mesh_controller.h"
#include "lvgl.h"

typedef void (*ui_bed_mesh_profiles_command_cb_t)(
    const char *command);

void ui_bed_mesh_profiles_init(
    lv_obj_t *owner,
    ui_bed_mesh_profiles_command_cb_t command_cb);

void ui_bed_mesh_profiles_update(
    const bed_mesh_snapshot_t *mesh);

void ui_bed_mesh_profiles_show_cb(
    lv_event_t *event);

void ui_bed_mesh_profiles_close(void);
