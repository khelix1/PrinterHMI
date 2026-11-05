#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    PRINT_PROGRESS_FILE_RELATIVE, PRINT_PROGRESS_FILE_ABSOLUTE,
    PRINT_PROGRESS_SLICER, PRINT_PROGRESS_FILAMENT,
} print_progress_mode_t;

enum { PRINT_ESTIMATE_FILE = 1, PRINT_ESTIMATE_FILAMENT = 2, PRINT_ESTIMATE_SLICER = 4 };

typedef struct {
    print_progress_mode_t progress;
    uint8_t remaining_sources, eta_sources;
} print_estimate_options_t;

typedef struct {
    double file_progress, slicer_progress, file_position, filament_used;
    bool file_valid, slicer_valid, position_valid, filament_valid;
} print_estimate_sample_t;

typedef struct {
    double start_byte, end_byte, filament_total, estimated_time;
} print_estimate_metadata_t;

typedef struct { double progress, remaining, eta_remaining; } print_estimate_result_t;

print_estimate_result_t print_estimate_calculate(print_estimate_options_t options,
    const print_estimate_sample_t *sample, const print_estimate_metadata_t *metadata,
    double elapsed);

print_estimate_options_t print_estimate_options_get(void);
bool print_estimate_options_save(print_estimate_options_t options);
