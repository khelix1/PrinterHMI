#pragma once

#include <stdbool.h>
#include <stddef.h>

#define CAMERA_CATALOG_MAX_CAMERAS 4
#define CAMERA_CATALOG_NAME_LENGTH 32

typedef struct {
    char name[CAMERA_CATALOG_NAME_LENGTH];
    char stream_url[192];
    unsigned rotation;       /* 0, 90, 180, or 270 degrees */
    bool mirror_horizontal;
    bool mirror_vertical;
    bool configured;
} camera_catalog_entry_t;

/* Camera 0 mirrors the existing per-profile stream URL for compatibility. */
size_t camera_catalog_count(int profile_index);
bool camera_catalog_get(int profile_index, size_t camera_index,
                        camera_catalog_entry_t *entry);
bool camera_catalog_set(int profile_index, size_t camera_index,
                        const char *name, const char *stream_url);
bool camera_catalog_clear(int profile_index, size_t camera_index);

/* View settings are stored per printer profile and per camera slot. */
bool camera_catalog_set_view(int profile_index, size_t camera_index,
                             unsigned rotation,
                             bool mirror_horizontal,
                             bool mirror_vertical);
