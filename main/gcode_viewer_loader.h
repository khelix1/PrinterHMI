#pragma once
#include "gcode_toolpath.h"
typedef bool (*gcode_viewer_ready_cb_t)(gcode_toolpath_t *owned, const char *error);
/* Called on the UI thread. Callback runs under the display lock; true adopts
 * the model, false leaves cleanup to the worker. Only one load runs at a time. */
bool gcode_viewer_loader_start(const char *file, gcode_viewer_ready_cb_t);
void gcode_viewer_loader_cancel(void);
bool gcode_viewer_loader_running(void);
void gcode_viewer_loader_free(gcode_toolpath_t *);

typedef struct { size_t bytes; uint32_t paths; uint16_t layers; bool running; unsigned phase,percent; float cell_mm; } gcode_viewer_progress_t;
/* Request-fenced snapshot; workers never touch progress labels directly. */
gcode_viewer_progress_t gcode_viewer_loader_progress(void);

/* Read a selected layer from this preview session, without network access. */
bool gcode_viewer_loader_detail(const gcode_toolpath_t *,unsigned,gcode_viewer_ready_cb_t);

struct mesh_preview;
typedef bool (*gcode_viewer_refined_cb)(struct mesh_preview *owned,const char *error);
/* Automatic whole-file refinement: SD cache, or fingerprint-checked HTTP replay. */
bool gcode_viewer_loader_refine(const char *,const gcode_toolpath_t *,gcode_viewer_refined_cb);
