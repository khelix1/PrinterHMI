#pragma once
#include "gcode_toolpath.h"
#include <stdio.h>
#define MESH_PREVIEW_VERTICES_MAX 65535u
#define MESH_PREVIEW_TRIANGLES_MAX 100000u
#define MESH_PREVIEW_HEADER_SIZE 80u
#ifndef MESH_PREVIEW_DIRECTORY
#define MESH_PREVIEW_DIRECTORY "/sdcard/PrinterHMI/meshes"
#endif
typedef struct {float x,y,z,nx,ny,nz;} mesh_vertex_t;
typedef struct {uint16_t a,b,c;} mesh_triangle_t;
typedef struct {float x,y,z,light;} mesh_projected_t;
typedef struct mesh_preview {
    uint32_t vertices,triangles,source_triangles,payload_bytes;
    float min[3],max[3],cell_mm,radius;
    bool cull_backfaces; /* Only generated outward-wound exterior meshes. */
    mesh_vertex_t *vertex;
    mesh_triangle_t *triangle;
    mesh_projected_t *projected;
    struct mesh_preview *interaction; /* Optional owned gesture mesh. */
} mesh_preview_t;
/* Explicit little-endian PHM v1; bounded allocation, CRCs and validated indices.
 * Reader returns one owned allocation. mesh_preview_free also releases an
 * optional gesture child using the allocator's matching free. */
mesh_preview_t *mesh_preview_read(FILE *,void *(*allocate)(size_t),void (*release)(void *),bool (*cancelled)(void *),void *,char *,size_t);
bool mesh_preview_name_valid(const char *);
/* Start at cursor 0. Camera/buffers must stay unchanged between steps.
 * Return 0 on completion. Allocation-free RGB565 + float depth raster. */
size_t mesh_preview_render_step(mesh_preview_t *,const gcode_view_t *,uint16_t *,float *,int,int,size_t,size_t);
/* Sparse wireframe during gesture interaction; no depth buffer required. */
void mesh_preview_render_wire(const mesh_preview_t *,const gcode_view_t *,uint16_t *,int,int,size_t);

size_t mesh_preview_render_edges_step(mesh_preview_t *,const gcode_view_t *,uint16_t *,float *,int,int,size_t,size_t);

/* Best-effort bounded vertex clustering for gestures. Full geometry is unchanged.
 * The root owns its optional child; always use mesh_preview_free for owned meshes. */
bool mesh_preview_prepare_interaction(mesh_preview_t *,void *(*)(size_t),void (*)(void *),bool (*)(void *),void *);
void mesh_preview_free(mesh_preview_t *,void (*)(void *));

/* Orthographic exterior surface cut; no synthetic cap is added at z_max. */
size_t mesh_preview_render_clipped_step(mesh_preview_t *,const gcode_view_t *,uint16_t *,float *,int,int,size_t,size_t,float);
bool mesh_preview_screen_bounds(const mesh_preview_t *,const gcode_view_t *,int,int,int [4]);

/* Closed generated meshes get an even/odd cut cap; scratch needs ((w+31)/32)*h uint32_t words.
 * Scratch is reset at cursor 0 and must be retained between calls. NULL gives an open cut. */
size_t mesh_preview_render_section_step(mesh_preview_t *,const gcode_view_t *,uint16_t *,float *,int,int,size_t,size_t,float,uint32_t *);

/* Cache a rotation-independent fit sphere after setting mesh positions/bounds. */
void mesh_preview_update_radius(mesh_preview_t *);

/* Single height interval: shaded top cap, lower exterior cut; same cap scratch as section. */
size_t mesh_preview_render_slab_step(mesh_preview_t *,const gcode_view_t *,uint16_t *,float *,int,int,size_t,size_t,float,float,uint32_t *);
/* Overlay retained travel paths in the mesh camera without clearing pixels/depth. */
size_t mesh_preview_render_travel_step(const mesh_preview_t *,const gcode_toolpath_t *,const gcode_view_t *,uint16_t *,float *,int,int,size_t,size_t);

/* Actual selected-layer solid extrusion faces in the mesh camera; no clear or synthetic fill. */
size_t mesh_preview_render_layer_step(const mesh_preview_t *,const gcode_toolpath_t *,const gcode_view_t *,uint16_t *,float *,int,int,size_t,size_t,int [4]);

/* Generated-shell contact band only, intersected with the section mask so holes
 * stay empty. Same mask sizing/lifetime as section; NULL keeps an open cut. */
size_t mesh_preview_render_joined_section_step(mesh_preview_t *,const gcode_view_t *,uint16_t *,float *,int,int,size_t,size_t,float,float,uint32_t *);

/* Movement-only rectangular solid beads: every source path, width, height, depth
 * and gold face shading remain; detailed bevels return through layer_step. */
size_t mesh_preview_render_layer_gesture_step(const mesh_preview_t *,const gcode_toolpath_t *,const gcode_view_t *,uint16_t *,float *,int,int,size_t,size_t,int [4]);
