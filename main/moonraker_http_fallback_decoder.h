#pragma once

#include <stdbool.h>

#include "moonraker.h"

/*
 * Decodes one legacy HTTP /printer/objects/query response.  It performs no
 * transport, UI, task, or synchronized-state work; main.c remains the app
 * lifecycle and publication coordinator.
 */
typedef struct {
    const char *objects;
    moonraker_http_fallback_update_t previous;
    int previous_drybox_selected_program;
    int previous_drybox_active_program;
    int cached_current_layer;
    int cached_total_layer;
    double cached_current_z;
    double cached_metadata_object_height;
    double cached_metadata_layer_height;
    bool file_metadata_valid;
    double file_object_height;
    double file_layer_height;
} moonraker_http_fallback_decoder_input_t;

typedef struct {
    moonraker_http_fallback_update_t update;
    int drybox_selected_program;
    int drybox_active_program;
    double current_z;
    double metadata_object_height;
    double metadata_layer_height;
    char printer_state[32];
    char printer_file[256];
} moonraker_http_fallback_decoder_output_t;

bool moonraker_http_fallback_decoder_decode(
    const moonraker_http_fallback_decoder_input_t *input,
    moonraker_http_fallback_decoder_output_t *output);
