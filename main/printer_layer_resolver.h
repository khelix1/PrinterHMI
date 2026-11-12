#pragma once

#include <stdbool.h>

typedef struct {
    double object_height, layer_height, first_layer_height;
    int total_layers; /* Zero means unavailable; validated metadata layer_count. */
} printer_layer_metadata_t;

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
 *   2. Active-file metadata count, then first/normal layer height estimate
 *   3. Current G-code Z, only after actual printing starts
 *   4. Print progress as the final current-layer fallback
 *
 * This module performs no I/O and owns no persistent state.
 */
printer_layer_result_t printer_layer_resolver_resolve(
    int reported_current,
    int reported_total,
    double current_z,
    const printer_layer_metadata_t *metadata,
    double progress,
    bool print_started);

/* File progress can advance while START_PRINT probes/heats. Prefer explicit
 * filament usage evidence; older producers may only supply print duration. */
bool printer_layer_resolver_print_started(
    double print_duration, bool filament_valid, double filament_used);
