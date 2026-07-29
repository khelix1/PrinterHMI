#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MACRO_CONTROLLER_MAX_MACROS 64
#define MACRO_CONTROLLER_NAME_MAX 64

struct cJSON;

typedef struct {
    bool discovered;
    bool truncated;
    size_t count;
    size_t total_count;
    uint32_t generation;
} macro_controller_status_t;

/*
 * Allocates one bounded permanent macro catalog in PSRAM, with internal RAM
 * as a fallback. The allocation is retained for the application lifetime.
 */
bool macro_controller_init(void);

/* Refresh the catalog from Moonraker's printer.objects.list result array. */
void macro_controller_update_from_objects(
    const struct cJSON *objects);

void macro_controller_status(
    macro_controller_status_t *out);

bool macro_controller_get(
    size_t index,
    char *output,
    size_t output_size);
