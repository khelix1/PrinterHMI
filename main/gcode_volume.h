#pragma once
#include "mesh_preview.h"
typedef struct gcode_volume gcode_volume_t;
typedef void *(*gcode_volume_alloc)(size_t);
typedef void (*gcode_volume_free)(void *);
typedef bool (*gcode_volume_cancel)(void *);
typedef void (*gcode_volume_progress)(void *,unsigned,float);
/* Automatic, bounded extrusion-union volume. Resolution is reported in mm;
 * this is a sampled deposition envelope, not reconstructed CAD. */
gcode_volume_t *gcode_volume_create(const gcode_toolpath_t *,unsigned,gcode_volume_alloc,gcode_volume_free,gcode_volume_cancel,gcode_volume_progress,void *);
bool gcode_volume_feed(gcode_volume_t *,const gcode_segment_t *);
mesh_preview_t *gcode_volume_finish(gcode_volume_t *);
void gcode_volume_destroy(gcode_volume_t *);
