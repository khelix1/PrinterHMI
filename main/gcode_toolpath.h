#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define GCODE_VIEW_ZOOM_MAX 16.0f
#define GCODE_TOOLPATH_TOOLS_MAX 16
#define GCODE_TOOLPATH_ARC_MAX 4096
#define GCODE_TOOLPATH_STATES_MAX 4
#define GCODE_TOOLPATH_LINE_MAX 512
#define GCODE_TOOLPATH_LAYERS_MAX 2048
#define GCODE_TOOLPATH_SEGMENTS_MAX 262144
#define GCODE_TOOLPATH_SEGMENTS_MIN 32768
#define GCODE_TOOLPATH_BYTES_MAX (64u * 1024u * 1024u)

typedef struct { float x, y, z; } gcode_point_t;
typedef struct {
    gcode_point_t a, b;
    uint16_t layer;
    bool extrusion;
    uint8_t tool;
    uint32_t sequence;
    float width,height;
} gcode_segment_t;
struct gcode_surface;
struct gcode_cache;
struct mesh_preview;
/* Large bounded model: allocate in PSRAM/static storage, never a task stack. */
typedef struct {
    gcode_segment_t *segments;
    struct gcode_surface *surface;
    struct gcode_cache *cache;
    struct mesh_preview *overview_mesh;
    uint64_t source_hash;
    bool source_hash_valid;
    gcode_point_t extrusion_min,extrusion_max;
    float max_width,max_height;
    uint32_t dimension_tags;
    uint32_t profile;
    int cached_layer;
    size_t count, capacity, bytes, lines;
    uint16_t layer_count;
    float layer_z[GCODE_TOOLPATH_LAYERS_MAX];
    uint16_t tool_mask, tool_anchor_mask;
    uint32_t input_segments, sample_stride, last_extrusion_sequence;
    bool extrusion_seen, coarse_arcs;
    gcode_segment_t layer_anchor[GCODE_TOOLPATH_LAYERS_MAX];
    gcode_segment_t tool_anchor[GCODE_TOOLPATH_TOOLS_MAX];
    gcode_segment_t last_segment;
    size_t arcs, tool_changes, tool_line, retracts, homings, homing_line;
    gcode_point_t min, max;
    bool bounded, unsupported, malformed;
    size_t stop_line, omitted_line, omitted_count;
    char stop_reason[48], stop_command[48], omitted_command[48];
} gcode_toolpath_t;
typedef struct {
    bool used, relative_xyz, relative_e, absolute_center;
    uint8_t tool, plane;
    unsigned known_xy;
    char name[32];
    gcode_point_t position,offset;
    double home[4], e, units;
} gcode_saved_state_t;
typedef struct {
    gcode_toolpath_t *path;
    void (*segment_sink)(void *,const gcode_segment_t *);
    void *sink_context;
    gcode_point_t position, offset;
    double e, units, tool_e[GCODE_TOOLPATH_TOOLS_MAX], home[4];
    uint8_t tool, plane;
    bool absolute_center;
    gcode_saved_state_t saved[GCODE_TOOLPATH_STATES_MAX];
    unsigned known_xy;
    bool relative_xyz, relative_e, dropping, comment, halted, last_cr,comment_overflow;
    size_t used, line_number;
    char line[GCODE_TOOLPATH_LINE_MAX];
    char comment_text[96];size_t comment_used;
    float extrusion_width,extrusion_height;
} gcode_parser_t;

void gcode_parser_init(gcode_parser_t *, gcode_toolpath_t *, gcode_segment_t *, size_t);
/* False means a hard limit/unsupported geometry was reached; stop streaming. */
bool gcode_parser_feed(gcode_parser_t *, const void *, size_t);
void gcode_parser_finish(gcode_parser_t *);
/* Record a bounded partial result, including a worker's load-time budget. */
void gcode_parser_limit(gcode_parser_t *, const char *reason);

typedef struct { float yaw, pitch, zoom; int layer; bool travel, cumulative; float pan_x,pan_y; float raster_ratio; /* Internal staging/display ratio; zero defaults to 1. */ } gcode_view_t;
void gcode_view_reset(gcode_view_t *);
/* RGB565, opaque background, caller-owned width*height pixels. No allocation. */
void gcode_toolpath_render(const gcode_toolpath_t *, const gcode_view_t *, uint16_t *, int, int);

/* Shaded, approximate 0.45 mm round extrusion beads, with a depth test.
 * Caller owns width*height floats as well as RGB565 pixels. No allocation.
 * Travels remain thin; no solid surface is invented between missing paths. */
void gcode_toolpath_render_solid(const gcode_toolpath_t *, const gcode_view_t *, uint16_t *, float *, int, int);

/* Incremental version: first=0 clears the buffers; return 0 means complete.
 * Keep model/view/buffers unchanged between steps; restart at 0 after changes. */
size_t gcode_toolpath_render_solid_step(const gcode_toolpath_t *, const gcode_view_t *, uint16_t *, float *, int, int, size_t first, size_t budget);

/* Append an independent cached segment with bounded sampling; metadata/bounds
 * are supplied by the loader snapshot. */
void gcode_toolpath_collect(gcode_toolpath_t *,gcode_segment_t);

/* Internal allocation-free capsule raster shared by mesh layer overlays. */
void gcode_toolpath_raster_bead(uint16_t *,float *,int,int,float,float,float,float,float,float,float,float,uint16_t,bool);
