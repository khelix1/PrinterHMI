#include "ui_bed_mesh_v32.h"
#include "esp_log.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bed_mesh_controller.h"
#include "esp_heap_caps.h"
#include "ui_button.h"
#include "ui_popup.h"
#include "ui_theme.h"
#include "ui_widgets.h"
#define CW 720
#define CH 360
#define VALUE_CAP ((size_t)BED_MESH_MAX_ROWS*BED_MESH_MAX_COLS)
typedef struct {
    lv_obj_t *popup;
    lv_obj_t *canvas;
    lv_obj_t *stats;
    lv_obj_t *surface_grid_button;
    lv_obj_t *surface_grid_label;
    lv_obj_t *calibrate_confirm;
    lv_obj_t *profile_popup;
    lv_obj_t *profile_list;
    lv_obj_t *profile_editor;
    lv_obj_t *profile_name_input;
    lv_obj_t *profile_confirm;
    char *pending_profile_name;
    int16_t selected_profile;
    uint16_t *buf;
    float *values;
    bed_mesh_profile_name_t *profile_names;
    bed_mesh_snapshot_t mesh;
    float yaw;
    float pitch;
    float zoom;
    float zscale;
    float pinch_base_zoom;
    uint32_t pinch_last_render_tick;
    float pan_x;
    float pan_y;
    uint32_t pan_last_render_tick;
    uint32_t drag_last_render_tick;
    lv_point_t two_touch_center;
    lv_point_t two_touch_start;
    uint8_t touch_count;
    bool pan_active;
    bool block_one_finger;
    lv_point_t last;
    lv_indev_t *gesture_indev;
    bool drag;
    bool pinch_active;
    bool surface_grid_visible;
    ui_bed_mesh_command_cb_t command;
} ui_t;
typedef struct{float x,y,z,sx,sy,d;} vertex_t;
typedef struct{uint16_t a,b,c;float d,z;} tri_t;
static ui_t s;

void printerhmi_bed_mesh_multitouch_update(
    uint8_t count,
    int32_t x0,
    int32_t y0,
    int32_t x1,
    int32_t y1)
{
    if (!s.popup) {
        return;
    }

    uint8_t previous_count = s.touch_count;
    s.touch_count = count;

    if (count >= 2) {
        s.two_touch_center.x = (x0 + x1) / 2;
        s.two_touch_center.y = (y0 + y1) / 2;
        s.block_one_finger = true;

        if (previous_count < 2) {
            s.two_touch_start = s.two_touch_center;
            s.last = s.two_touch_center;
            s.pan_active = false;
            s.pan_last_render_tick = 0;
            s.drag = false;
        }
    } else if (count == 0) {
        /*
         * Do not let the remaining finger rotate the graph after a
         * two-finger gesture. Re-enable one-finger rotation only after all
         * fingers have lifted.
         */
        s.block_one_finger = false;
    }
}

static void *alloc_psram_first(size_t n,size_t z){void*p=heap_caps_calloc(n,z,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);if(!p)p=heap_caps_calloc(n,z,MALLOC_CAP_8BIT);return p;}
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
    float cy = cosf(s.yaw);
    float sy = sinf(s.yaw);
    float cp = cosf(s.pitch);
    float sp = sinf(s.pitch);
    float rx = v->x * cy - v->y * sy;
    float ry = v->x * sy + v->y * cy;
    float rz = v->z;
    float py = ry * cp - rz * sp;
    float pz = ry * sp + rz * cp;

    v->sx = CW * .5f + s.pan_x + rx * scale * s.zoom;
    v->sy = CH * .56f + s.pan_y + py * scale * s.zoom;
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
        cosf(s.yaw) >= 0.0f ? -height * .5f : height * .5f;
    float rear_x =
        sinf(s.yaw) >= 0.0f ? -width * .5f : width * .5f;

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

static void render(void)
{
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
    if (!s.drag && !s.pinch_active && !s.pan_active) {
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
static void stats(void){if(!s.stats)return;if(!s.mesh.valid){lv_label_set_text(s.stats,"No active bed mesh. Calibrate or load a profile.");return;}char b[256];snprintf(
        b,
        sizeof(b),
        "PROFILE %s   GRID %u x %u   Z %.0fx%s\n"
        "MIN %+.3f mm   MAX %+.3f mm   RANGE %.3f mm",
        s.mesh.profile_name,
        s.mesh.cols,
        s.mesh.rows,
        s.zscale,
        s.mesh.truncated ? "   TRUNCATED" : "",
        s.mesh.minimum,
        s.mesh.maximum,
        s.mesh.range);lv_label_set_text(s.stats,b);}
void ui_bed_mesh_v32_refresh(void)
{
    if (!s.popup) {
        return;
    }

    if (!s.values) {
        s.values =
            alloc_psram_first(
                VALUE_CAP,
                sizeof(float));
    }

    if (!s.profile_names) {
        s.profile_names =
            alloc_psram_first(
                BED_MESH_MAX_PROFILES,
                sizeof(*s.profile_names));
    }

    if (!s.values ||
        !bed_mesh_controller_snapshot(
            &s.mesh,
            s.values,
            VALUE_CAP,
            s.profile_names,
            s.profile_names
                ? BED_MESH_MAX_PROFILES
                : 0)) {
        memset(&s.mesh, 0, sizeof(s.mesh));
        stats();

        if (s.canvas) {
            lv_canvas_fill_bg(
                s.canvas,
                UI_BED_MESH_BG,
                LV_OPA_COVER);
        }

        return;
    }

    stats();
    render();
}

static void update_surface_grid_button(void)
{
    if (s.surface_grid_label) {
        lv_label_set_text(
            s.surface_grid_label,
            s.surface_grid_visible
                ? "SURFACE GRID ON"
                : "SURFACE GRID OFF");
    }

    if (s.surface_grid_button) {
        ui_button_apply_kind(
            s.surface_grid_button,
            s.surface_grid_visible
                ? UI_BUTTON_PRIMARY
                : UI_BUTTON_SECONDARY);
    }
}

static void surface_grid_cb(lv_event_t *e)
{
    (void)e;
    s.surface_grid_visible = !s.surface_grid_visible;
    update_surface_grid_button();
    render();
}

static void close_cb(lv_event_t *e)
{
    (void)e;
    ui_bed_mesh_v32_close();
}

static void reset_cb(lv_event_t *e)
{
    (void)e;
    s.yaw = -0.72f;
    s.pitch = 0.88f;
    s.zoom = 1.0f;
    s.zscale = 20.0f;
    s.pan_x = 0.0f;
    s.pan_y = 0.0f;
    stats();
    render();
}

static void plus_cb(lv_event_t *e)
{
    (void)e;
    s.zoom = fminf(3.5f, s.zoom * 1.15f);
    render();
}

static void minus_cb(lv_event_t *e)
{
    (void)e;
    s.zoom = fmaxf(0.45f, s.zoom / 1.15f);
    render();
}

static void close_calibrate_confirm_cb(lv_event_t *e)
{
    (void)e;

    if (s.calibrate_confirm) {
        lv_obj_t *popup = s.calibrate_confirm;
        s.calibrate_confirm = NULL;
        lv_obj_delete(popup);
    }
}

static void confirm_calibrate_cb(lv_event_t *e)
{
    (void)e;

    /*
     * Preserve the command callback before closing the modal. The mesh
     * viewer remains open so its live snapshot can refresh after probing.
     */
    ui_bed_mesh_command_cb_t command = s.command;
    close_calibrate_confirm_cb(NULL);

    if (command) {
        command("BED_MESH_CALIBRATE");
    }
}

static void calibrate_cb(lv_event_t *e)
{
    (void)e;

    if (s.calibrate_confirm) {
        lv_obj_move_foreground(s.calibrate_confirm);
        return;
    }

    /*
     * Calibration belongs to the bed-mesh viewer, while all modal geometry
     * and styling comes from the shared popup subsystem.
     */
    s.calibrate_confirm =
        ui_popup_create(
            s.popup ? s.popup : lv_screen_active(),
            500,
            280,
            UI_POPUP_DANGER);

    if (!s.calibrate_confirm) {
        return;
    }

    ui_popup_add_title(
        s.calibrate_confirm,
        "START BED MESH?",
        true,
        4);
    ui_popup_add_header_divider(
        s.calibrate_confirm,
        44);
    ui_popup_add_body(
        s.calibrate_confirm,
        "This will move the toolhead across the bed and probe multiple "
        "points.\n\nMake sure the bed is clear and the printer is ready.",
        24,
        70,
        452);
    ui_popup_add_standard_footer_divider(
        s.calibrate_confirm);

    ui_popup_add_footer_action(
        s.calibrate_confirm,
        UI_POPUP_ACTION_CANCEL,
        LV_SYMBOL_LEFT " BACK",
        160,
        UI_POPUP_FOOTER_LEFT,
        close_calibrate_confirm_cb,
        NULL,
        NULL);

    ui_popup_add_footer_action(
        s.calibrate_confirm,
        UI_POPUP_ACTION_DANGER,
        LV_SYMBOL_PLAY " START",
        190,
        UI_POPUP_FOOTER_RIGHT,
        confirm_calibrate_cb,
        NULL,
        NULL);
}

static void free_pending_profile_name(void)
{
    if (s.pending_profile_name) {
        heap_caps_free(s.pending_profile_name);
        s.pending_profile_name = NULL;
    }
}

static bool valid_profile_name_ui(const char *name)
{
    if (!name || !name[0]) {
        return false;
    }

    size_t length =
        strnlen(name, BED_MESH_PROFILE_NAME_MAX);

    if (length == 0 ||
        length >= BED_MESH_PROFILE_NAME_MAX) {
        return false;
    }

    for (size_t i = 0; i < length; ++i) {
        char character = name[i];
        bool valid =
            (character >= 'a' && character <= 'z') ||
            (character >= 'A' && character <= 'Z') ||
            (character >= '0' && character <= '9') ||
            character == '_' ||
            character == '-' ||
            character == '.';

        if (!valid) {
            return false;
        }
    }

    return true;
}

static bool set_pending_profile_name(const char *name)
{
    if (!valid_profile_name_ui(name)) {
        return false;
    }

    size_t length = strlen(name);
    char *copy =
        heap_caps_calloc(
            length + 1,
            sizeof(char),
            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (!copy) {
        copy =
            heap_caps_calloc(
                length + 1,
                sizeof(char),
                MALLOC_CAP_8BIT);
    }

    if (!copy) {
        return false;
    }

    memcpy(copy, name, length + 1);
    free_pending_profile_name();
    s.pending_profile_name = copy;
    return true;
}

static void close_profile_editor_cb(lv_event_t *e)
{
    (void)e;

    s.profile_name_input = NULL;

    if (s.profile_editor) {
        lv_obj_t *popup = s.profile_editor;
        s.profile_editor = NULL;
        lv_obj_delete(popup);
    }
}

static void close_profile_confirm_cb(lv_event_t *e)
{
    (void)e;

    if (s.profile_confirm) {
        lv_obj_t *popup = s.profile_confirm;
        s.profile_confirm = NULL;
        lv_obj_delete(popup);
    }

    free_pending_profile_name();
}

static void close_profile_popup_cb(lv_event_t *e)
{
    (void)e;

    close_profile_editor_cb(NULL);
    close_profile_confirm_cb(NULL);

    s.profile_list = NULL;
    s.selected_profile = -1;

    if (s.profile_popup) {
        lv_obj_t *popup = s.profile_popup;
        s.profile_popup = NULL;
        lv_obj_delete(popup);
    }
}

static const char *selected_profile_name(void)
{
    if (s.selected_profile < 0 ||
        (size_t)s.selected_profile >= s.mesh.profile_count ||
        !s.mesh.profile_names) {
        return NULL;
    }

    return s.mesh.profile_names[s.selected_profile];
}

static void profile_row_cb(lv_event_t *e)
{
    uintptr_t encoded =
        (uintptr_t)lv_event_get_user_data(e);

    if (encoded == 0) {
        return;
    }

    size_t index = (size_t)(encoded - 1);

    if (index >= s.mesh.profile_count) {
        return;
    }

    s.selected_profile = (int16_t)index;

    lv_obj_t *selected = lv_event_get_target(e);

    if (s.profile_list) {
        uint32_t count =
            lv_obj_get_child_count(s.profile_list);

        for (uint32_t i = 0; i < count; ++i) {
            lv_obj_t *row =
                lv_obj_get_child(s.profile_list, i);

            ui_popup_set_selectable_row_selected(
                row,
                row == selected);
        }
    }
}

static void load_selected_profile_cb(lv_event_t *e)
{
    (void)e;

    const char *name = selected_profile_name();

    if (!name || !s.command) {
        return;
    }

    char command[128];
    int written = snprintf(
        command,
        sizeof(command),
        "BED_MESH_PROFILE LOAD=%s",
        name);

    if (written <= 0 ||
        (size_t)written >= sizeof(command)) {
        return;
    }

    ui_bed_mesh_command_cb_t send_command = s.command;
    close_profile_popup_cb(NULL);
    send_command(command);
}

static void confirmed_profile_command(
    const char *operation)
{
    if (!operation ||
        !s.pending_profile_name ||
        !s.command) {
        return;
    }

    char command[128];
    int written = snprintf(
        command,
        sizeof(command),
        "BED_MESH_PROFILE %s=%s",
        operation,
        s.pending_profile_name);

    if (written <= 0 ||
        (size_t)written >= sizeof(command)) {
        return;
    }

    ui_bed_mesh_command_cb_t send_command = s.command;

    /*
     * Close all viewer-owned modal surfaces before SAVE_CONFIG restarts
     * Klipper. The local command buffer remains valid for both requests.
     */
    ui_bed_mesh_v32_close();
    send_command(command);
    send_command("SAVE_CONFIG");
}

static void confirm_save_profile_cb(lv_event_t *e)
{
    (void)e;
    confirmed_profile_command("SAVE");
}

static void confirm_remove_profile_cb(lv_event_t *e)
{
    (void)e;
    confirmed_profile_command("REMOVE");
}

static void show_profile_confirmation(
    bool removing)
{
    if (!s.pending_profile_name) {
        return;
    }

    char message[256];

    if (removing) {
        snprintf(
            message,
            sizeof(message),
            "Remove profile \"%s\"?\n\n"
            "SAVE_CONFIG will restart Klipper.",
            s.pending_profile_name);
    } else {
        snprintf(
            message,
            sizeof(message),
            "Save the current mesh as \"%s\"?\n\n"
            "An existing profile with this name will be replaced. "
            "SAVE_CONFIG will restart Klipper.",
            s.pending_profile_name);
    }

    s.profile_confirm =
        ui_popup_create(
            s.profile_popup
                ? s.profile_popup
                : s.popup,
            560,
            300,
            removing
                ? UI_POPUP_DANGER
                : UI_POPUP_STANDARD);

    if (!s.profile_confirm) {
        free_pending_profile_name();
        return;
    }

    ui_popup_add_title(
        s.profile_confirm,
        removing
            ? "REMOVE BED MESH?"
            : "SAVE BED MESH?",
        removing,
        4);
    ui_popup_add_header_divider(
        s.profile_confirm,
        44);
    ui_popup_add_body(
        s.profile_confirm,
        message,
        28,
        76,
        504);
    ui_popup_add_standard_footer_divider(
        s.profile_confirm);
    ui_popup_add_footer_action(
        s.profile_confirm,
        UI_POPUP_ACTION_CANCEL,
        LV_SYMBOL_LEFT " BACK",
        160,
        UI_POPUP_FOOTER_LEFT,
        close_profile_confirm_cb,
        NULL,
        NULL);
    ui_popup_add_footer_action(
        s.profile_confirm,
        removing
            ? UI_POPUP_ACTION_DANGER
            : UI_POPUP_ACTION_CONFIRM,
        removing
            ? LV_SYMBOL_TRASH " REMOVE & RESTART"
            : LV_SYMBOL_SAVE " SAVE & RESTART",
        240,
        UI_POPUP_FOOTER_RIGHT,
        removing
            ? confirm_remove_profile_cb
            : confirm_save_profile_cb,
        NULL,
        NULL);
}

static void save_name_continue_cb(lv_event_t *e)
{
    (void)e;

    if (!s.profile_name_input) {
        return;
    }

    const char *name =
        lv_textarea_get_text(s.profile_name_input);

    if (!set_pending_profile_name(name)) {
        return;
    }

    close_profile_editor_cb(NULL);
    show_profile_confirmation(false);
}

static void save_as_profile_cb(lv_event_t *e)
{
    (void)e;

    if (s.profile_editor) {
        lv_obj_move_foreground(s.profile_editor);
        return;
    }

    s.profile_editor =
        ui_popup_create(
            s.profile_popup
                ? s.profile_popup
                : s.popup,
            760,
            520,
            UI_POPUP_STANDARD);

    if (!s.profile_editor) {
        return;
    }

    ui_popup_add_title(
        s.profile_editor,
        "SAVE BED MESH PROFILE",
        false,
        4);
    ui_popup_add_header_divider(
        s.profile_editor,
        44);
    ui_popup_add_caption(
        s.profile_editor,
        "PROFILE NAME",
        54,
        68,
        220);

    s.profile_name_input =
        ui_popup_add_textarea(
            s.profile_editor,
            652,
            52,
            LV_ALIGN_TOP_MID,
            0,
            88,
            true,
            false,
            BED_MESH_PROFILE_NAME_MAX - 1,
            "example: textured_plate",
            "",
            "abcdefghijklmnopqrstuvwxyz"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "0123456789_.-");

    ui_popup_add_keyboard(
        s.profile_editor,
        s.profile_name_input,
        652,
        270,
        LV_ALIGN_TOP_MID,
        0,
        154,
        LV_KEYBOARD_MODE_TEXT_LOWER);
    ui_popup_add_standard_footer_divider(
        s.profile_editor);
    ui_popup_add_footer_action(
        s.profile_editor,
        UI_POPUP_ACTION_CANCEL,
        LV_SYMBOL_CLOSE " CANCEL",
        170,
        UI_POPUP_FOOTER_LEFT,
        close_profile_editor_cb,
        NULL,
        NULL);
    ui_popup_add_footer_action(
        s.profile_editor,
        UI_POPUP_ACTION_CONFIRM,
        "CONTINUE",
        180,
        UI_POPUP_FOOTER_RIGHT,
        save_name_continue_cb,
        NULL,
        NULL);
}

static void remove_selected_profile_cb(lv_event_t *e)
{
    (void)e;

    const char *name = selected_profile_name();

    if (!name ||
        !set_pending_profile_name(name)) {
        return;
    }

    show_profile_confirmation(true);
}

static void profiles_cb(lv_event_t *e)
{
    (void)e;

    if (s.profile_popup) {
        lv_obj_move_foreground(s.profile_popup);
        return;
    }

    s.profile_popup =
        ui_popup_create(
            s.popup ? s.popup : lv_screen_active(),
            720,
            470,
            UI_POPUP_STANDARD);

    if (!s.profile_popup) {
        return;
    }

    s.selected_profile = -1;

    ui_popup_add_title(
        s.profile_popup,
        "BED MESH PROFILES",
        false,
        4);
    ui_popup_add_header_divider(
        s.profile_popup,
        44);

    s.profile_list =
        ui_popup_add_list(
            s.profile_popup,
            24,
            64,
            672,
            318);

    if (s.profile_list) {
        if (s.mesh.profile_count == 0) {
            lv_obj_t *empty =
                ui_popup_add_status_label(
                    s.profile_popup,
                    "No saved bed-mesh profiles reported.",
                    48,
                    104,
                    624);

            if (empty) {
                lv_obj_set_style_text_color(
                    empty,
                    UI_TEXT_DIM,
                    0);
            }
        } else {
            for (size_t i = 0;
                 i < s.mesh.profile_count;
                 ++i) {
                const char *name =
                    s.mesh.profile_names[i];
                bool active =
                    strcmp(
                        name,
                        s.mesh.profile_name) == 0;

                if (active) {
                    s.selected_profile = (int16_t)i;
                }

                char row_text[96];
                snprintf(
                    row_text,
                    sizeof(row_text),
                    "%s%s",
                    name,
                    active ? "   ACTIVE" : "");

                lv_obj_t *row =
                    ui_popup_add_selectable_row(
                        s.profile_list,
                        row_text,
                        0,
                        (int32_t)i * 50,
                        648,
                        46,
                        profile_row_cb,
                        (void *)(uintptr_t)(i + 1));

                if (row) {
                    ui_popup_set_selectable_row_selected(
                        row,
                        active);
                }
            }
        }
    }

    ui_popup_add_standard_footer_divider(
        s.profile_popup);
    ui_popup_add_action_aligned(
        s.profile_popup,
        UI_POPUP_ACTION_SECONDARY,
        LV_SYMBOL_SAVE " SAVE AS",
        150,
        48,
        LV_ALIGN_BOTTOM_LEFT,
        24,
        -12,
        save_as_profile_cb,
        NULL,
        NULL);
    ui_popup_add_action_aligned(
        s.profile_popup,
        UI_POPUP_ACTION_PRIMARY,
        LV_SYMBOL_PLAY " LOAD",
        120,
        48,
        LV_ALIGN_BOTTOM_LEFT,
        184,
        -12,
        load_selected_profile_cb,
        NULL,
        NULL);
    ui_popup_add_action_aligned(
        s.profile_popup,
        UI_POPUP_ACTION_DANGER,
        LV_SYMBOL_TRASH " REMOVE",
        150,
        48,
        LV_ALIGN_BOTTOM_LEFT,
        314,
        -12,
        remove_selected_profile_cb,
        NULL,
        NULL);
    ui_popup_add_footer_action(
        s.profile_popup,
        UI_POPUP_ACTION_CLOSE,
        LV_SYMBOL_CLOSE " CLOSE",
        130,
        UI_POPUP_FOOTER_RIGHT,
        close_profile_popup_cb,
        NULL,
        NULL);
}

static void canvas_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_indev_t *indev = lv_indev_active();

    if (!indev) {
        return;
    }

    /*
     * The touch adapter supplies the two-point centroid. Handle translation
     * on the normal LVGL pressing event so drawing stays inside the LVGL
     * input/event context.
     */
    if (code == LV_EVENT_PRESSING && s.touch_count >= 2) {
        if (s.pinch_active) {
            return;
        }

        if (!s.pan_active) {
            int32_t start_dx =
                s.two_touch_center.x - s.two_touch_start.x;
            int32_t start_dy =
                s.two_touch_center.y - s.two_touch_start.y;

            /*
             * Hold the gesture undecided until the centroid has moved far
             * enough to be an intentional pan. Pinch recognition can win
             * during this short dead zone.
             */
            if (start_dx * start_dx + start_dy * start_dy < 144) {
                return;
            }

            s.pan_active = true;
            s.pan_last_render_tick = 0;
            s.last = s.two_touch_center;
            s.drag = false;
            return;
        } else {
            int32_t dx = s.two_touch_center.x - s.last.x;
            int32_t dy = s.two_touch_center.y - s.last.y;

            /*
             * Reject implausible single-sample jumps caused by touch-ID
             * reassignment while preserving normal deliberate movement.
             */
            if (abs(dx) <= 80 && abs(dy) <= 80) {
                s.pan_x += dx;
                s.pan_y += dy;

                if (s.pan_x < -CW * .45f) {
                    s.pan_x = -CW * .45f;
                }
                if (s.pan_x > CW * .45f) {
                    s.pan_x = CW * .45f;
                }
                if (s.pan_y < -CH * .45f) {
                    s.pan_y = -CH * .45f;
                }
                if (s.pan_y > CH * .45f) {
                    s.pan_y = CH * .45f;
                }

                uint32_t now = lv_tick_get();

                if (s.pan_last_render_tick == 0 ||
                    now - s.pan_last_render_tick >= 33U) {
                    s.pan_last_render_tick = now;
                    render();
                }
            }

            s.last = s.two_touch_center;
        }

        return;
    }

    if (s.pan_active && s.touch_count < 2) {
        s.pan_active = false;
        s.pan_last_render_tick = 0;
        render();
    }

#if LV_USE_GESTURE_RECOGNITION
    if (code == LV_EVENT_GESTURE) {
        lv_indev_gesture_type_t type =
            lv_event_get_gesture_type(e);

        if (type == LV_INDEV_GESTURE_PINCH) {
            lv_indev_gesture_state_t state =
                lv_event_get_gesture_state(
                    e,
                    LV_INDEV_GESTURE_PINCH);

            if (state == LV_INDEV_GESTURE_STATE_RECOGNIZED) {
                /*
                 * Once centroid motion has committed to pan, keep that
                 * two-finger gesture in pan mode until release.
                 */
                if (s.pan_active) {
                    return;
                }

                if (!s.pinch_active) {
                    s.pinch_base_zoom = s.zoom;
                    s.pinch_last_render_tick = 0;
                    s.pinch_active = true;
                    s.drag = false;
                }

                float raw_scale = lv_event_get_pinch_scale(e);

                if (raw_scale > 0.01f) {
                    /*
                     * Reduce sensitivity while keeping the zoom direction
                     * natural. A 20% finger-distance change becomes a 12%
                     * zoom change.
                     */
                    float adjusted_scale =
                        1.0f + ((raw_scale - 1.0f) * 0.60f);
                    float target_zoom =
                        s.pinch_base_zoom * adjusted_scale;

                    if (target_zoom < 0.45f) {
                        target_zoom = 0.45f;
                    }
                    if (target_zoom > 3.5f) {
                        target_zoom = 3.5f;
                    }

                    /*
                     * Low-pass filtering removes GT911 coordinate jitter.
                     * Keep the state current even when a redraw is skipped.
                     */
                    float delta = target_zoom - s.zoom;

                    if (fabsf(delta) < 0.002f) {
                        s.zoom = target_zoom;
                    } else {
                        s.zoom += delta * 0.45f;
                    }

                    uint32_t now = lv_tick_get();

                    if (s.pinch_last_render_tick == 0 ||
                        now - s.pinch_last_render_tick >= 33U) {
                        s.pinch_last_render_tick = now;
                        render();
                    }
                }
            } else if (state == LV_INDEV_GESTURE_STATE_ENDED) {
                /*
                 * Always draw the final filtered value so throttling cannot
                 * leave the canvas one event behind.
                 */
                s.pinch_base_zoom = s.zoom;
                s.pinch_last_render_tick = 0;
                s.pinch_active = false;
                s.drag = false;
                render();
            }

            return;
        }

        /*
         * Native two-finger rotation is intentionally disabled while this
         * popup is open. One-finger drag remains the rotation control.
         */
        return;
    }
#endif

    if (code == LV_EVENT_PRESSED) {
        lv_indev_get_point(indev, &s.last);
        s.drag_last_render_tick = 0;
        s.drag = !s.pinch_active &&
                 !s.block_one_finger &&
                 s.touch_count < 2;
    } else if (code == LV_EVENT_PRESSING &&
               s.drag &&
               !s.pinch_active &&
               !s.block_one_finger &&
               s.touch_count < 2) {
        lv_point_t point;
        lv_indev_get_point(indev, &point);

        s.yaw -= (point.x - s.last.x) * 0.012f;
        s.pitch -= (point.y - s.last.y) * 0.01f;

        if (s.pitch < 0.15f) {
            s.pitch = 0.15f;
        }
        if (s.pitch > 1.45f) {
            s.pitch = 1.45f;
        }

        s.last = point;

        uint32_t now = lv_tick_get();

        if (s.drag_last_render_tick == 0 ||
            now - s.drag_last_render_tick >= 33U) {
            s.drag_last_render_tick = now;
            render();
        }
    } else if (code == LV_EVENT_RELEASED ||
               code == LV_EVENT_PRESS_LOST) {
        s.drag = false;
        s.drag_last_render_tick = 0;
        render();
    }
}
bool ui_bed_mesh_v32_is_open(void)
{
    return s.popup != NULL;
}

void ui_bed_mesh_v32_close(void)
{
    /*
     * The shared confirmation is a top-layer sibling, not a child of the
     * mesh viewer. Close it first so no modal surface can outlive its owner.
     */
    close_calibrate_confirm_cb(NULL);
    close_profile_popup_cb(NULL);

#if LV_USE_GESTURE_RECOGNITION
    if (s.gesture_indev) {
        /*
         * Restore LVGL 9.5 defaults when leaving this specialized viewer.
         */
        lv_indev_set_pinch_up_threshold(s.gesture_indev, 1.50f);
        lv_indev_set_pinch_down_threshold(s.gesture_indev, 0.75f);
        lv_indev_set_rotation_rad_threshold(s.gesture_indev, 0.20f);
    }
#endif

    if (s.popup) {
        lv_obj_delete(s.popup);
    }
    if (s.buf) {
        heap_caps_free(s.buf);
    }
    if (s.values) {
        heap_caps_free(s.values);
    }
    if (s.profile_names) {
        heap_caps_free(s.profile_names);
    }

    memset(&s, 0, sizeof(s));
}

void ui_bed_mesh_v32_show(ui_bed_mesh_command_cb_t command)
{
    ui_bed_mesh_v32_close();
    s.command = command;
    s.yaw = -0.72f;
    s.pitch = 0.88f;
    s.zoom = 1.0f;
    s.zscale = 20.0f;

#if LV_USE_GESTURE_RECOGNITION
    s.gesture_indev = lv_indev_active();

    if (s.gesture_indev) {
        /*
         * LVGL 9.5 defaults require a 1.50x/0.75x distance change before a
         * pinch is recognized. Recognize at 1.02x/0.98x instead so zoom
         * engages with minimal finger travel, while keeping the competing
         * native rotation recognizer from winning first.
         */
        lv_indev_set_pinch_up_threshold(s.gesture_indev, 1.02f);
        lv_indev_set_pinch_down_threshold(s.gesture_indev, 0.98f);
        lv_indev_set_rotation_rad_threshold(s.gesture_indev, 3.20f);
    }
#endif

    s.popup =
        ui_popup_create(
            lv_screen_active(),
            950,
            540,
            UI_POPUP_STANDARD);

    if (!s.popup) {
        return;
    }

    ui_popup_add_title(
        s.popup,
        "BED MESH 3D",
        false,
        8);
    ui_popup_add_header_divider(
        s.popup,
        52);

    s.stats =
        ui_popup_add_caption(
            s.popup,
            "",
            24,
            58,
            902);

    s.buf =
        alloc_psram_first(
            (size_t)CW * CH,
            sizeof(uint16_t));

    if (s.buf) {
        s.canvas = lv_canvas_create(s.popup);
        lv_canvas_set_buffer(
            s.canvas,
            s.buf,
            CW,
            CH,
            LV_COLOR_FORMAT_RGB565);
        lv_obj_set_pos(
            s.canvas,
            24,
            98);
        lv_obj_add_flag(
            s.canvas,
            LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(
            s.canvas,
            canvas_cb,
            LV_EVENT_ALL,
            NULL);
    }

    s.surface_grid_button =
        ui_popup_add_action_aligned(
            s.popup,
            UI_POPUP_ACTION_SECONDARY,
            "SURFACE GRID OFF",
            170,
            46,
            LV_ALIGN_TOP_RIGHT,
            -24,
            98,
            surface_grid_cb,
            NULL,
            &s.surface_grid_label);
    update_surface_grid_button();

    ui_popup_add_standard_footer_divider(
        s.popup);
    ui_popup_add_caption(
        s.popup,
        "DRAG ROTATE   •   2-FINGER PAN   •   PINCH ZOOM",
        24,
        497,
        350);

    ui_popup_add_close_button(
        s.popup,
        110,
        42,
        LV_ALIGN_TOP_RIGHT,
        -24,
        8,
        UI_BUTTON_CLOSE,
        close_cb,
        NULL);
    ui_popup_add_action_aligned(
        s.popup,
        UI_POPUP_ACTION_SECONDARY,
        "RESET VIEW",
        132,
        48,
        LV_ALIGN_BOTTOM_RIGHT,
        -24,
        -12,
        reset_cb,
        NULL,
        NULL);
    ui_popup_add_action_aligned(
        s.popup,
        UI_POPUP_ACTION_SECONDARY,
        "+",
        52,
        48,
        LV_ALIGN_BOTTOM_RIGHT,
        -166,
        -12,
        plus_cb,
        NULL,
        NULL);
    ui_popup_add_action_aligned(
        s.popup,
        UI_POPUP_ACTION_SECONDARY,
        "-",
        52,
        48,
        LV_ALIGN_BOTTOM_RIGHT,
        -228,
        -12,
        minus_cb,
        NULL,
        NULL);
    ui_popup_add_action_aligned(
        s.popup,
        UI_POPUP_ACTION_PRIMARY,
        "CALIBRATE",
        130,
        48,
        LV_ALIGN_BOTTOM_RIGHT,
        -290,
        -12,
        calibrate_cb,
        NULL,
        NULL);
    ui_popup_add_action_aligned(
        s.popup,
        UI_POPUP_ACTION_SECONDARY,
        "PROFILES",
        140,
        48,
        LV_ALIGN_BOTTOM_RIGHT,
        -430,
        -12,
        profiles_cb,
        NULL,
        NULL);

    ui_bed_mesh_v32_refresh();
}
