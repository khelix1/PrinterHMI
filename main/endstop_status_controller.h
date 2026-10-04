#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
struct cJSON;
#define ENDSTOP_STATUS_MAX 12
#define ENDSTOP_STATUS_NAME_MAX 40
#define ENDSTOP_REQUEST_FIRST 0x40000000U

typedef struct {
    bool waiting, valid, truncated;
    size_t count;
    uint32_t request_id, owner_generation;
    int64_t updated_us;
    char error[112];
    struct { char name[ENDSTOP_STATUS_NAME_MAX]; bool triggered; } items[ENDSTOP_STATUS_MAX];
} endstop_status_snapshot_t;
void endstop_status_controller_reset(void);
uint32_t endstop_status_controller_begin(uint32_t owner_generation);
void endstop_status_controller_failed(const char *message);
bool endstop_status_controller_merge(const struct cJSON *root, uint32_t owner_generation);
void endstop_status_controller_snapshot(endstop_status_snapshot_t *out);
