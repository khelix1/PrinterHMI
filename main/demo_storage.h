#pragma once
#include "nvs.h"
/* RAM-only NVS subset. Caller serializes access; close discards uncommitted
 * data. */
#include <stdbool.h>
bool demo_storage_init(void);
esp_err_t demo_storage_open(const char *, nvs_open_mode_t, nvs_handle_t *);
void demo_storage_close(nvs_handle_t);
esp_err_t demo_storage_commit(nvs_handle_t);
esp_err_t demo_storage_read(nvs_handle_t, const char *, unsigned, void *,
                            size_t *);
esp_err_t demo_storage_write(nvs_handle_t, const char *, unsigned, const void *,
                             size_t);
#define DEMO_VALUE_U8 1
#define DEMO_VALUE_I32 2
#define DEMO_VALUE_STR 3
#define DEMO_VALUE_BLOB 4
