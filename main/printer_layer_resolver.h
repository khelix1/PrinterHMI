#pragma once

#include <stdbool.h>

typedef struct {
    int current;
    int total;
    bool valid;
} printer_layer_result_t;

/*
 * Resolve the best available current/total layer pair.
 *
 * Priority:
 *   1. Klipper/Moonraker reported layer values
 *   2. Active-file object/layer metadata
 *   3. Current G-code Z
 *   4. Print progress as the final current-layer fallback
 *
 * This module performs no I/O and owns no persistent state.
 */
printer_layer_result_t printer_layer_resolver_resolve(
    int reported_current,
    int reported_total,
    double current_z,
    double object_height,
    double layer_height,
    double progress);
