#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "cJSON.h"
#define BED_MESH_MAX_ROWS 33
#define BED_MESH_MAX_COLS 33
#define BED_MESH_PROFILE_NAME_MAX 64
typedef struct {
    bool valid;
    bool truncated;
    uint16_t rows;
    uint16_t cols;
    double mesh_min_x, mesh_min_y, mesh_max_x, mesh_max_y;
    double minimum, maximum, average, range;
    char profile_name[BED_MESH_PROFILE_NAME_MAX];
    const float *values;
} bed_mesh_snapshot_t;
void bed_mesh_controller_init(void);
void bed_mesh_controller_reset(void);
bool bed_mesh_controller_merge_status(cJSON *status);
bool bed_mesh_controller_snapshot(bed_mesh_snapshot_t *out, float *values, size_t values_capacity);
