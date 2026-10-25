#pragma once
#include "gcode_toolpath.h"
#include <stdio.h>
#define GCODE_CACHE_BYTES_MAX (128u*1024u*1024u)
struct gcode_cache {
    unsigned refs;
    FILE *writer;
    char file[128];
    uint32_t count,last_extrusion;
    uint32_t start[GCODE_TOOLPATH_LAYERS_MAX],end[GCODE_TOOLPATH_LAYERS_MAX];
    uint16_t layers;
    bool good,complete,extrusion_seen;
    char buffer[4096];
};
/* Cache is session-only, owned by loader references. No source/auth data. */
bool gcode_cache_begin(struct gcode_cache *,uint32_t session);
#define GCODE_CACHE_RECORD_BYTES 44u
void gcode_cache_feed(struct gcode_cache *,const gcode_segment_t *);
bool gcode_cache_finish(struct gcode_cache *);
void gcode_cache_destroy(struct gcode_cache *);
FILE *gcode_cache_open_layer(const struct gcode_cache *,unsigned,uint32_t *);
bool gcode_cache_read(FILE *,uint32_t sequence,unsigned layer,gcode_segment_t *);
