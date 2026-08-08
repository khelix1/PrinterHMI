#pragma once

#include <stdbool.h>

typedef void (*file_detail_loader_v32_ready_cb_t)(
    const char *file,
    bool metadata_ok,
    const char *metadata_text,
    const char *thumbnail_path);

bool file_detail_loader_v32_start(
    const char *host,
    int port,
    const char *api_key,
    const char *file,
    file_detail_loader_v32_ready_cb_t ready_cb);
void file_detail_loader_v32_cancel(void);
