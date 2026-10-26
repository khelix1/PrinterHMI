#pragma once

#include <stdbool.h>

#include "lvgl.h"
#include "printer_files.h"

typedef void (*files_row_preview_ready_cb_t)(
    const char *file,
    const lv_image_dsc_t *image);

/*
 * Begin a new Files-page preview generation. Unchanged endpoint/revision RGB565 images are
 * reusable across list reloads; stale jobs are discarded
 * by generation number. Begin/request are called under the display lock.
 * Worker publication takes the display lock before the slot mutex.
 */
void files_row_preview_begin(
    const char *host,
    int port,
    const char *api_key,
    bool sd_available,
    const printer_file_entry_t *entries, size_t count,
    files_row_preview_ready_cb_t ready_cb);

/* Queue one visible file. Duplicate, ready, and in-flight requests are cheap. */
void files_row_preview_request(const char *file);
