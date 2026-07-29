#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DEVICE_CATALOG_MAX_DEVICES 96
#define DEVICE_CATALOG_OBJECT_NAME_MAX 80
#define DEVICE_CATALOG_DISPLAY_NAME_MAX 64

struct cJSON;

typedef enum {
    DEVICE_KIND_THERMAL = 0,
    DEVICE_KIND_AIR,
    DEVICE_KIND_POWER,
    DEVICE_KIND_SENSOR,
    DEVICE_KIND_OUTPUT,
    DEVICE_KIND_MOTION,
    DEVICE_KIND_OTHER,
    DEVICE_KIND_COUNT
} device_kind_t;

typedef struct {
    char object_name[DEVICE_CATALOG_OBJECT_NAME_MAX];
    char display_name[DEVICE_CATALOG_DISPLAY_NAME_MAX];
    device_kind_t kind;
    bool controllable;
} device_descriptor_t;

typedef struct {
    bool discovered;
    bool truncated;
    size_t total_object_count;
    size_t stored_count;
    size_t kind_count[DEVICE_KIND_COUNT];
    uint32_t generation;
} device_catalog_status_t;

/*
 * Allocates one bounded catalog permanently in PSRAM. Internal RAM is used
 * only if PSRAM allocation fails.
 */
bool device_catalog_controller_init(void);

/*
 * Rebuild from the active printer's printer.objects.list response.
 * Recognized hardware is retained before unknown objects if the catalog
 * reaches its fixed limit.
 */
void device_catalog_controller_update_from_objects(
    const struct cJSON *objects);

/*
 * Invalidates the active-printer catalog during disconnect, profile rebind,
 * or Klipper restart.
 */
void device_catalog_controller_reset(void);

void device_catalog_controller_status(
    device_catalog_status_t *output);

bool device_catalog_controller_get(
    size_t index,
    device_descriptor_t *output);

bool device_catalog_controller_contains(
    const char *object_name);

const char *device_catalog_kind_label(
    device_kind_t kind);
