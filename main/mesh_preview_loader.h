#pragma once
#include "mesh_preview.h"
typedef bool (*mesh_preview_ready_cb)(mesh_preview_t *owned,const char *error);
/* Local SD only; callback holds display lock. True adopts the single allocation. */
bool mesh_preview_loader_start(const char *,mesh_preview_ready_cb);
void mesh_preview_loader_cancel(void);
bool mesh_preview_loader_running(void);
void mesh_preview_loader_free(mesh_preview_t *);
