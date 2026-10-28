#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

char *thumbnail_session_selected_file(void);
size_t thumbnail_session_selected_file_size(void);

char *thumbnail_session_selected_thumbnail_path(void);
size_t thumbnail_session_selected_thumbnail_path_size(void);

char *thumbnail_session_metadata_info(void);
size_t thumbnail_session_metadata_info_size(void);

void thumbnail_session_clear_selected_file(void);
void thumbnail_session_clear_thumbnail_path(void);

bool thumbnail_session_build_metadata(
    const char *moonraker_host,
    int moonraker_port,
    const char *api_key,
    const char *file,
    char *out,
    size_t out_size);

bool thumbnail_session_get_layer_metadata(
    double *object_height,
    double *layer_height);

void thumbnail_session_clear_png_buffer(void);
void thumbnail_session_free_thumbnail(void);

void thumbnail_session_save_last_selected_file(void);
