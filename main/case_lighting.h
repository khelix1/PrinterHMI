#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
struct cJSON;
typedef struct {
    uint32_t generation;
    bool discovered, available, configured, pwm, known, pending;
    double scale, value;
    int target;
    uint32_t request;
    int64_t started_us;
    char error[96];
} case_lighting_snapshot_t;
/* WebSocket owner supplies only messages from its current connection. */
void case_lighting_reset(uint32_t generation);
void case_lighting_objects(const struct cJSON *objects, uint32_t generation);
void case_lighting_config(const struct cJSON *config, uint32_t generation);
void case_lighting_status_json(const char *json, size_t length, uint32_t generation);
bool case_lighting_response(const struct cJSON *root, uint32_t generation);
void case_lighting_snapshot(case_lighting_snapshot_t *out, int64_t now_us);
uint32_t case_lighting_begin(uint32_t generation, int percent, int64_t now_us,
                             char *command, size_t size);
void case_lighting_failed(uint32_t request, uint32_t generation);
