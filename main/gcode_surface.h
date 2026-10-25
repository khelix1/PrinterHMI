#pragma once
#include "gcode_toolpath.h"
#define GCODE_SURFACE_PANELS_MAX 32768u
#define GCODE_SURFACE_SLOTS 65536u
typedef struct {
    float ax,ay,bx,by,bottom,top;
    uint16_t first_layer,last_layer;
    uint8_t tool;
} gcode_panel_t;
struct gcode_surface {
    gcode_panel_t *panels;
    uint32_t *slots;
    size_t count,capacity,merged,inputs;
    bool overflow,nonplanar,pending;
    gcode_segment_t tail;
};
/* Caller owns storage; builder and renderer allocate nothing. */
void gcode_surface_init(struct gcode_surface *,gcode_panel_t *,size_t,uint32_t *);
void gcode_surface_feed(struct gcode_surface *,const gcode_segment_t *);
void gcode_surface_finish(struct gcode_surface *);
bool gcode_surface_usable(const struct gcode_surface *);
size_t gcode_surface_render_step(const gcode_toolpath_t *,const gcode_view_t *,uint16_t *,float *,int,int,size_t,size_t);
