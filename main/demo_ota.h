#pragma once
#include <stdbool.h>
#include "ota_release_catalog.h"
bool demo_ota_catalog_start(void);
void demo_ota_catalog_snapshot(ota_release_catalog_snapshot_t *out);
bool demo_ota_catalog_entry(size_t index, ota_release_entry_t *out);
bool demo_ota_running(void);
void demo_ota_open(void);
void demo_ota_stop(void);
