#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
struct cJSON;
#define MOTION_DRIVER_MAX 12
#define MOTION_DRIVER_NAME_MAX 80

typedef struct {
    char name[MOTION_DRIVER_NAME_MAX];
    bool run_valid, hold_valid, temperature_valid, status_valid;
    bool warning, fault;
    double run_current, hold_current, temperature;
    char flags[128];
} motion_driver_snapshot_t;

typedef struct {
    uint32_t owner_generation;
    bool discovered, drivers_truncated;
    bool limit_valid[4];
    double limits[4]; /* velocity, acceleration, cornering velocity, cruise ratio */
    bool rotation_valid[3];
    double rotation_distance[3]; /* stepper_x/y/z config reference, not runtime overrides */
    char kinematics[32];
    size_t driver_count;
    motion_driver_snapshot_t drivers[MOTION_DRIVER_MAX];
} motion_diagnostics_snapshot_t;

bool motion_diagnostics_controller_init(void);
void motion_diagnostics_controller_reset(void);
void motion_diagnostics_controller_update_objects(const struct cJSON *objects);
void motion_diagnostics_controller_update_config(const struct cJSON *config);
bool motion_diagnostics_controller_merge_status(const struct cJSON *status);
void motion_diagnostics_controller_snapshot(motion_diagnostics_snapshot_t *out);
/* Hardware geometry: belt pitch * pulley teeth, or screw pitch * thread starts. */
bool motion_axis_distance_calculate(const char *pitch, const char *count,
    double *distance, const char **error);
