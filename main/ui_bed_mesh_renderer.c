#include "ui_bed_mesh_renderer.h"

#include <math.h>
#include <stdlib.h>

#include "esp_heap_caps.h"
#include "ui_theme.h"

#define CW 620
#define CH 340

typedef struct {
    float x;
    float y;
    float z;
    float sx;
    float sy;
    float d;
} vertex_t;

typedef struct {
    uint16_t a;
    uint16_t b;
    uint16_t c;
    float d;
    float z;
} tri_t;

static ui_bed_mesh_renderer_config_t s;


static void *alloc_psram_first(
    size_t count,
    size_t size)
{
    void *memory =
        heap_caps_calloc(
            count,
            size,
            MALLOC_CAP_SPIRAM |
                MALLOC_CAP_8BIT);

    if (!memory) {
        memory =
            heap_caps_calloc(
                count,
                size,
                MALLOC_CAP_8BIT);
    }

    return memory;
}

static lv_color_t heat(float z)
{
    /*
     * Keep zero visually stable between meshes. The previous min-to-max
     * normalization made the middle color mean the midpoint of the current
     * range, which could make two equally level beds look very different.
     */
    double extent = fmax(fabs(s.mesh.minimum), fabs(s.mesh.maximum));
    double normalized = extent > 1e-8 ? z / extent : 0.0;

    if (normalized < -1.0) {
        normalized = -1.0;
    }
    if (normalized > 1.0) {
        normalized = 1.0;
    }

    if (normalized < 0.0) {
        return lv_color_mix(
            UI_BED_MESH_LOW,
            UI_BED_MESH_LEVEL,
            (uint8_t)(-normalized * 255.0));
    }

    return lv_color_mix(
        UI_BED_MESH_HIGH,
        UI_BED_MESH_LEVEL,
        (uint8_t)(normalized * 255.0));
}

static int cmp(const void *A, const void *B)
{
    float a = ((const tri_t *)A)->d;
    float b = ((const tri_t *)B)->d;

    return a < b ? -1 : a > b ? 1 : 0;
}

static void project(vertex_t *v, float scale)
{
    float cy = cosf(s.view.yaw);
    float sy = sinf(s.view.yaw);
    float cp = cosf(s.view.pitch);
    float sp = sinf(s.view.pitch);
    float rx = v->x * cy - v->y * sy;
    float ry = v->x * sy + v->y * cy;
    float rz = v->z;
    float py = ry * cp - rz * sp;
    float pz = ry * sp + rz * cp;

    v->sx = CW * .5f + s.view.pan_x + rx * scale * s.view.zoom;
    v->sy = CH * .56f + s.view.pan_y + py * scale * s.view.zoom;
    v->d = pz;
}

static void draw_line(
    lv_layer_t *layer,
    const vertex_t *a,
    const vertex_t *b,
    lv_color_t color,
    int32_t width,
    lv_opa_t opa)
{
    lv_draw_line_dsc_t d;

    lv_draw_line_dsc_init(&d);
    d.p1.x = lroundf(a->sx);
    d.p1.y = lroundf(a->sy);
    d.p2.x = lroundf(b->sx);
    d.p2.y = lroundf(b->sy);
    d.color = color;
    d.width = width;
    d.opa = opa;
    d.round_start = 1;
    d.round_end = 1;
    lv_draw_line(layer, &d);
}

static void draw_screen_cross(
    lv_layer_t *layer,
    const vertex_t *center,
    lv_color_t color)
{
    vertex_t a = *center;
    vertex_t b = *center;

    a.sx -= 6.0f;
    b.sx += 6.0f;
    draw_line(layer, &a, &b, color, 3, LV_OPA_COVER);

    a = *center;
    b = *center;
    a.sy -= 6.0f;
    b.sy += 6.0f;
    draw_line(layer, &a, &b, color, 3, LV_OPA_COVER);
}

static void draw_height_grid(
    lv_layer_t *layer,
    float width,
    float height,
    float scale)
{
    enum {
        AXIS_DIVISIONS = 6,
        HEIGHT_DIVISIONS = 5,
    };

    float z_low = (float)s.mesh.minimum * s.zscale;
    float z_high = (float)s.mesh.maximum * s.zscale;
    float z_center = (z_low + z_high) * .5f;
    float z_span = z_high - z_low;

    /*
     * A very flat bed still needs a readable height backdrop. Preserve the
     * real mesh scale while guaranteeing roughly 112 pixels of vertical grid
     * at the default view.
     */
    float minimum_span = scale > .001f ? 112.0f / scale : 112.0f;

    if (z_span < minimum_span) {
        z_low = z_center - minimum_span * .5f;
        z_high = z_center + minimum_span * .5f;
    } else {
        float padding = z_span * .12f;
        z_low -= padding;
        z_high += padding;
    }

    /* Keep Z=0 inside both background height walls. */
    if (z_low > 0.0f) {
        z_low = 0.0f;
    }
    if (z_high < 0.0f) {
        z_high = 0.0f;
    }

    vertex_t a = {0};
    vertex_t b = {0};

    /*
     * Select the two far edges from the current yaw. This keeps the X/Z and
     * Y/Z walls behind the measured surface instead of rotating them onto
     * the viewer-facing edges.
     */
    float rear_y =
        cosf(s.view.yaw) >= 0.0f ? -height * .5f : height * .5f;
    float rear_x =
        sinf(s.view.yaw) >= 0.0f ? -width * .5f : width * .5f;

    /*
     * X/Z rear wall: vertical divisions run along X at the rear Y edge.
     */
    for (int i = 0; i <= AXIS_DIVISIONS; ++i) {
        float fraction = (float)i / AXIS_DIVISIONS;
        float x = -width * .5f + width * fraction;

        a.x = x;
        a.y = rear_y;
        a.z = z_low;
        b.x = x;
        b.y = rear_y;
        b.z = z_high;
        project(&a, scale);
        project(&b, scale);
        bool zero_axis = i == 0;
        draw_line(
            layer,
            &a,
            &b,
            zero_axis ? UI_ACCENT_CYAN : UI_BORDER_BRIGHT,
            zero_axis ? 3 : 2,
            zero_axis ? LV_OPA_COVER : LV_OPA_70);
    }

    /*
     * Y/Z side wall: vertical divisions run along Y at the left X edge.
     */
    for (int i = 0; i <= AXIS_DIVISIONS; ++i) {
        float fraction = (float)i / AXIS_DIVISIONS;
        float y = -height * .5f + height * fraction;

        a.x = rear_x;
        a.y = y;
        a.z = z_low;
        b.x = rear_x;
        b.y = y;
        b.z = z_high;
        project(&a, scale);
        project(&b, scale);
        bool zero_axis = i == AXIS_DIVISIONS;
        draw_line(
            layer,
            &a,
            &b,
            zero_axis ? UI_ACCENT_PURPLE : UI_BORDER_BRIGHT,
            zero_axis ? 3 : 2,
            zero_axis ? LV_OPA_COVER : LV_OPA_70);
    }

    /*
     * Shared height levels cross both perpendicular walls.
     */
    for (int i = 0; i <= HEIGHT_DIVISIONS; ++i) {
        float fraction = (float)i / HEIGHT_DIVISIONS;
        float z = z_low + (z_high - z_low) * fraction;
        lv_opa_t opacity =
            i == 0 || i == HEIGHT_DIVISIONS ? LV_OPA_80 : LV_OPA_50;

        a.x = -width * .5f;
        a.y = rear_y;
        a.z = z;
        b.x = width * .5f;
        b.y = rear_y;
        b.z = z;
        project(&a, scale);
        project(&b, scale);
        draw_line(
            layer,
            &a,
            &b,
            UI_BORDER_BRIGHT,
            2,
            opacity);

        a.x = rear_x;
        a.y = -height * .5f;
        a.z = z;
        b.x = rear_x;
        b.y = height * .5f;
        b.z = z;
        project(&a, scale);
        project(&b, scale);
        draw_line(
            layer,
            &a,
            &b,
            UI_BORDER_BRIGHT,
            2,
            opacity);
    }

    /*
     * Mark the lower-left X=0 and Y=0 wall positions where they meet Z=0.
     */
    vertex_t x_zero = {
        .x = -width * .5f,
        .y = rear_y,
        .z = 0.0f,
    };
    vertex_t y_zero = {
        .x = rear_x,
        .y = height * .5f,
        .z = 0.0f,
    };

    project(&x_zero, scale);
    project(&y_zero, scale);
    draw_screen_cross(layer, &x_zero, UI_ACCENT_CYAN);
    draw_screen_cross(layer, &y_zero, UI_ACCENT_PURPLE);
}

static uint16_t surface_grid_step(uint16_t count)
{
    enum { MAX_SURFACE_GRID_LINES = 7 };

    if (count <= MAX_SURFACE_GRID_LINES) {
        return 1;
    }

    return (uint16_t)(
        ((count - 1) + (MAX_SURFACE_GRID_LINES - 2)) /
        (MAX_SURFACE_GRID_LINES - 1));
}

static void draw_surface_grid_row(
    lv_layer_t *layer,
    const vertex_t *vertices,
    uint16_t cols,
    uint16_t row)
{
    size_t base = (size_t)row * cols;

    for (uint16_t x = 0; x + 1 < cols; ++x) {
        draw_line(
            layer,
            &vertices[base + x],
            &vertices[base + x + 1],
            UI_BED_MESH_WIREFRAME,
            1,
            LV_OPA_40);
    }
}

static void draw_surface_grid_col(
    lv_layer_t *layer,
    const vertex_t *vertices,
    uint16_t rows,
    uint16_t cols,
    uint16_t col)
{
    for (uint16_t y = 0; y + 1 < rows; ++y) {
        draw_line(
            layer,
            &vertices[(size_t)y * cols + col],
            &vertices[(size_t)(y + 1) * cols + col],
            UI_BED_MESH_WIREFRAME,
            1,
            LV_OPA_40);
    }
}

static void draw_surface_grid(
    lv_layer_t *layer,
    const vertex_t *vertices,
    uint16_t rows,
    uint16_t cols)
{
    uint16_t row_step = surface_grid_step(rows);
    uint16_t col_step = surface_grid_step(cols);
    uint16_t last_row = 0;
    uint16_t last_col = 0;

    for (uint16_t y = 0; y < rows; y += row_step) {
        draw_surface_grid_row(layer, vertices, cols, y);
        last_row = y;
    }
    if (last_row != rows - 1) {
        draw_surface_grid_row(layer, vertices, cols, rows - 1);
    }

    for (uint16_t x = 0; x < cols; x += col_step) {
        draw_surface_grid_col(layer, vertices, rows, cols, x);
        last_col = x;
    }
    if (last_col != cols - 1) {
        draw_surface_grid_col(layer, vertices, rows, cols, cols - 1);
    }
}

static void draw_surface_origin_marker(
    lv_layer_t *layer,
    const vertex_t *vertices,
    uint16_t rows,
    uint16_t cols)
{
    if (rows == 0 || cols == 0) {
        return;
    }

    /*
     * The stored Y rows run opposite the screen-facing bed view. The final
     * row and first column are therefore the visual lower-left mesh origin.
     * Keep this as a point marker only: no surface axis or grid lines.
     */
    size_t origin = (size_t)(rows - 1) * cols;
    draw_screen_cross(layer, &vertices[origin], UI_BORDER_BRIGHT);
}

static float triangle_edge(
    float ax,
    float ay,
    float bx,
    float by,
    float px,
    float py)
{
    return (px - ax) * (by - ay) -
           (py - ay) * (bx - ax);
}

static uint16_t rgb565_from_rgb(
    uint8_t red,
    uint8_t green,
    uint8_t blue)
{
    return (uint16_t)(
        ((uint16_t)(red >> 3) << 11) |
        ((uint16_t)(green >> 2) << 5) |
        ((uint16_t)(blue >> 3)));
}

static void rasterize_smooth_triangle(
    uint16_t *pixels,
    const vertex_t *a,
    const vertex_t *b,
    const vertex_t *c)
{
    float area =
        triangle_edge(
            a->sx,
            a->sy,
            b->sx,
            b->sy,
            c->sx,
            c->sy);

    if (fabsf(area) < 0.01f) {
        return;
    }

    int32_t min_x =
        (int32_t)floorf(
            fminf(a->sx, fminf(b->sx, c->sx)));
    int32_t max_x =
        (int32_t)ceilf(
            fmaxf(a->sx, fmaxf(b->sx, c->sx)));
    int32_t min_y =
        (int32_t)floorf(
            fminf(a->sy, fminf(b->sy, c->sy)));
    int32_t max_y =
        (int32_t)ceilf(
            fmaxf(a->sy, fmaxf(b->sy, c->sy)));

    if (min_x < 0) {
        min_x = 0;
    }
    if (max_x >= CW) {
        max_x = CW - 1;
    }
    if (min_y < 0) {
        min_y = 0;
    }
    if (max_y >= CH) {
        max_y = CH - 1;
    }
    if (min_x > max_x || min_y > max_y) {
        return;
    }

    lv_color_t color_a = heat(a->z / s.zscale);
    lv_color_t color_b = heat(b->z / s.zscale);
    lv_color_t color_c = heat(c->z / s.zscale);
    uint32_t packed_a = lv_color_to_u32(color_a);
    uint32_t packed_b = lv_color_to_u32(color_b);
    uint32_t packed_c = lv_color_to_u32(color_c);
    float inverse_area = 1.0f / area;

    for (int32_t y = min_y; y <= max_y; ++y) {
        float py = (float)y + 0.5f;

        for (int32_t x = min_x; x <= max_x; ++x) {
            float px = (float)x + 0.5f;
            float edge_a =
                triangle_edge(
                    b->sx,
                    b->sy,
                    c->sx,
                    c->sy,
                    px,
                    py);
            float edge_b =
                triangle_edge(
                    c->sx,
                    c->sy,
                    a->sx,
                    a->sy,
                    px,
                    py);
            float edge_c =
                triangle_edge(
                    a->sx,
                    a->sy,
                    b->sx,
                    b->sy,
                    px,
                    py);

            bool inside =
                area > 0.0f
                    ? edge_a >= 0.0f &&
                      edge_b >= 0.0f &&
                      edge_c >= 0.0f
                    : edge_a <= 0.0f &&
                      edge_b <= 0.0f &&
                      edge_c <= 0.0f;

            if (!inside) {
                continue;
            }

            float weight_a = edge_a * inverse_area;
            float weight_b = edge_b * inverse_area;
            float weight_c = edge_c * inverse_area;
            uint8_t red =
                (uint8_t)lroundf(
                    ((packed_a >> 16) & 0xff) * weight_a +
                    ((packed_b >> 16) & 0xff) * weight_b +
                    ((packed_c >> 16) & 0xff) * weight_c);
            uint8_t green =
                (uint8_t)lroundf(
                    ((packed_a >> 8) & 0xff) * weight_a +
                    ((packed_b >> 8) & 0xff) * weight_b +
                    ((packed_c >> 8) & 0xff) * weight_c);
            uint8_t blue =
                (uint8_t)lroundf(
                    (packed_a & 0xff) * weight_a +
                    (packed_b & 0xff) * weight_b +
                    (packed_c & 0xff) * weight_c);

            pixels[(size_t)y * CW + x] =
                rgb565_from_rgb(red, green, blue);
        }
    }
}

void ui_bed_mesh_renderer_render(
    const ui_bed_mesh_renderer_config_t *config)
{
    if (!config) {
        return;
    }

    s = *config;

    if (!s.canvas || !s.mesh.valid) {
        return;
    }

    uint16_t rows = s.mesh.rows;
    uint16_t cols = s.mesh.cols;
    size_t nv = (size_t)rows * cols;
    size_t nt = (size_t)(rows - 1) * (cols - 1) * 2;
    vertex_t *v = alloc_psram_first(nv, sizeof(*v));
    tri_t *t = alloc_psram_first(nt, sizeof(*t));

    if (!v || !t) {
        if (v) {
            heap_caps_free(v);
        }
        if (t) {
            heap_caps_free(t);
        }
        return;
    }

    float width = s.mesh.mesh_max_x - s.mesh.mesh_min_x;
    float height = s.mesh.mesh_max_y - s.mesh.mesh_min_y;

    if (fabsf(width) < .001f) {
        width = cols - 1;
    }
    if (fabsf(height) < .001f) {
        height = rows - 1;
    }

    float scale = 250.0f / (width > height ? width : height);

    for (uint16_t y = 0; y < rows; ++y) {
        for (uint16_t x = 0; x < cols; ++x) {
            size_t i = (size_t)y * cols + x;

            v[i].x = -width * .5f + width * x / (cols - 1);
            v[i].y = -height * .5f + height * y / (rows - 1);
            v[i].z = s.values[i] * s.zscale;
            project(&v[i], scale);
        }
    }

    size_t k = 0;

    for (uint16_t y = 0; y + 1 < rows; ++y) {
        for (uint16_t x = 0; x + 1 < cols; ++x) {
            uint16_t a = y * cols + x;
            uint16_t b = a + 1;
            uint16_t c = (y + 1) * cols + x;
            uint16_t d = c + 1;

            /*
             * Both triangles belong to one mesh cell. Give them the same
             * four-corner average color so the internal diagonal cannot
             * appear as a false wireframe line.
             */
            float cell_z =
                (v[a].z + v[b].z + v[c].z + v[d].z) /
                (4.0f * s.zscale);

            t[k++] = (tri_t){
                a,
                b,
                d,
                (v[a].d + v[b].d + v[d].d) / 3,
                cell_z,
            };
            t[k++] = (tri_t){
                a,
                d,
                c,
                (v[a].d + v[d].d + v[c].d) / 3,
                cell_z,
            };
        }
    }

    qsort(t, nt, sizeof(*t), cmp);
    lv_canvas_fill_bg(s.canvas, UI_BED_MESH_BG, LV_OPA_COVER);

    /*
     * Commit the rear X/Y/Z grid first. The smooth mesh rasterizer then
     * writes directly into the existing static RGB565 canvas buffer.
     */
    lv_layer_t layer;
    lv_canvas_init_layer(s.canvas, &layer);
    draw_height_grid(&layer, width, height, scale);
    lv_canvas_finish_layer(s.canvas, &layer);

    /*
     * Interpolate the three vertex colors for every covered pixel. Shared
     * vertices produce identical colors on both sides of every triangle
     * edge, so the fill primitives are no longer visible.
     */
    for (size_t i = 0; i < nt; ++i) {
        rasterize_smooth_triangle(
            s.buf,
            &v[t[i].a],
            &v[t[i].b],
            &v[t[i].c]);
    }

    /*
     * Draw optional surface detail after the color surface. The origin
     * marker remains visible even while the surface grid is disabled.
     */
    lv_canvas_init_layer(s.canvas, &layer);
    if (!s.interaction_active) {
        if (s.surface_grid_visible) {
            draw_surface_grid(&layer, v, rows, cols);
        }
        draw_surface_origin_marker(&layer, v, rows, cols);
    }
    lv_canvas_finish_layer(s.canvas, &layer);
    lv_obj_invalidate(s.canvas);
    heap_caps_free(v);
    heap_caps_free(t);
}
