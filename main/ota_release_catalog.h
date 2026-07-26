#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define OTA_RELEASE_CATALOG_MAX_PER_CHANNEL 8
#define OTA_RELEASE_CATALOG_MAX \
    (OTA_RELEASE_CATALOG_MAX_PER_CHANNEL * 2)

typedef enum {
    OTA_RELEASE_CHANNEL_STABLE = 0,
    OTA_RELEASE_CHANNEL_NIGHTLY,
} ota_release_channel_t;

typedef enum {
    OTA_RELEASE_OLDER = -1,
    OTA_RELEASE_INSTALLED = 0,
    OTA_RELEASE_NEWER = 1,
    OTA_RELEASE_DEVELOPMENT = 2,
    OTA_RELEASE_STABLE = 3,
} ota_release_relation_t;

typedef struct {
    char version[40];
    char name[64];
    char published[16];
    char asset_url[192];
    char notes[192];
    int64_t asset_size;
    ota_release_channel_t channel;
    ota_release_relation_t relation;
} ota_release_entry_t;

typedef enum {
    OTA_RELEASE_CATALOG_IDLE = 0,
    OTA_RELEASE_CATALOG_LOADING,
    OTA_RELEASE_CATALOG_READY,
    OTA_RELEASE_CATALOG_ERROR,
} ota_release_catalog_state_t;

typedef struct {
    ota_release_catalog_state_t state;
    size_t count;
    char error[128];
} ota_release_catalog_snapshot_t;

bool ota_release_catalog_start(void);
void ota_release_catalog_snapshot(
    ota_release_catalog_snapshot_t *snapshot);
bool ota_release_catalog_entry(
    size_t index,
    ota_release_entry_t *entry);
