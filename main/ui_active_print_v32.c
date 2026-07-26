#include "ui_active_print_v32.h"
#include "ui_preview_lightbox_v32.h"
#include "ui_theme.h"
#include "ui_thumbnail_v32.h"
#include "ui_widgets.h"
#include "esp_heap_caps.h"
#include <string.h>
#include <stdio.h>

typedef struct {
    lv_obj_t *preview_box;
    lv_obj_t *preview_label;
    lv_obj_t *footer;
} active_print_ctx_t;


static lv_obj_t *s_active_print_panel = NULL;
static lv_obj_t *s_active_print_thumb_canvas = NULL;
static uint16_t *s_active_print_thumb_canvas_buf = NULL;
static char s_active_print_thumb_canvas_file[160] = "";

static void active_print_preview_clicked_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED ||
        !s_active_print_thumb_canvas) {
        return;
    }

    ui_preview_lightbox_v32_show_object(
        s_active_print_thumb_canvas);
}

static void active_print_delete_cb(lv_event_t *event)
{
    lv_obj_t *panel = (lv_obj_t *)lv_event_get_target(event);
    active_print_ctx_t *ctx =
        (active_print_ctx_t *)lv_event_get_user_data(event);

    /*
     * The preview canvas is owned by the panel hierarchy.  If an outer
     * page rebuild deletes that hierarchy, LVGL deletes the canvas too;
     * clear our non-owning references so the replacement page creates a
     * new canvas instead of touching the freed object.
     */
    if (panel == s_active_print_panel) {
        s_active_print_panel = NULL;
        s_active_print_thumb_canvas = NULL;
        s_active_print_thumb_canvas_file[0] = 0;
    }

    if (ctx) {
        lv_free(ctx);
    }
}


lv_obj_t *ui_active_print_v32_thumb_box(lv_obj_t *card)
{
    if (!card) return NULL;

    active_print_ctx_t *ctx = (active_print_ctx_t *)lv_obj_get_user_data(card);
    if (!ctx) return NULL;

    return ctx->preview_box;
}


lv_obj_t **ui_active_print_v32_thumb_canvas_ref(void)
{
    return &s_active_print_thumb_canvas;
}

uint16_t **ui_active_print_v32_thumb_canvas_buf_ref(void)
{
    return &s_active_print_thumb_canvas_buf;
}

char *ui_active_print_v32_thumb_canvas_file(void)
{
    return s_active_print_thumb_canvas_file;
}

size_t ui_active_print_v32_thumb_canvas_file_size(void)
{
    return sizeof(s_active_print_thumb_canvas_file);
}

void ui_active_print_v32_thumb_set_placeholder(lv_obj_t *card, const char *text)
{
    if (!card) card = s_active_print_panel;
    if (!card) return;

    active_print_ctx_t *ctx = (active_print_ctx_t *)lv_obj_get_user_data(card);
    if (!ctx || !ctx->preview_label) return;

    lv_label_set_text(ctx->preview_label, text ? text : "PRINT\nTHUMBNAIL");
    lv_obj_clear_flag(ctx->preview_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_center(ctx->preview_label);
    lv_obj_move_foreground(ctx->preview_label);
}

void ui_active_print_v32_thumb_clear_placeholder(lv_obj_t *card)
{
    if (!card) card = s_active_print_panel;
    if (!card) return;

    active_print_ctx_t *ctx = (active_print_ctx_t *)lv_obj_get_user_data(card);
    if (!ctx || !ctx->preview_label) return;

    lv_obj_add_flag(ctx->preview_label, LV_OBJ_FLAG_HIDDEN);
}

bool ui_active_print_v32_thumb_canvas_matches(const char *file)
{
    return s_active_print_thumb_canvas &&
           file && file[0] &&
           strcmp(s_active_print_thumb_canvas_file, file) == 0;
}

bool ui_active_print_v32_thumb_ensure_canvas_buffer(size_t pixels)
{
    if (s_active_print_thumb_canvas_buf) return true;

    s_active_print_thumb_canvas_buf = heap_caps_malloc(pixels * sizeof(uint16_t),
                                                       MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_active_print_thumb_canvas_buf) {
        s_active_print_thumb_canvas_buf = heap_caps_malloc(pixels * sizeof(uint16_t),
                                                           MALLOC_CAP_8BIT);
    }

    return s_active_print_thumb_canvas_buf != NULL;
}

void ui_active_print_v32_thumb_delete_canvas(void)
{
    if (s_active_print_thumb_canvas) {
        lv_obj_delete(s_active_print_thumb_canvas);
        s_active_print_thumb_canvas = NULL;
    }
    s_active_print_thumb_canvas_file[0] = 0;
}

void ui_active_print_v32_thumb_forget_canvas_file(void)
{
    s_active_print_thumb_canvas_file[0] = 0;
}

static void ui_active_print_v32_thumb_copy_file(const char *file)
{
    if (!file) file = "";
    snprintf(s_active_print_thumb_canvas_file,
             sizeof(s_active_print_thumb_canvas_file),
             "%s",
             file);
}

void ui_active_print_v32_thumb_show_canvas_from_buffer(lv_obj_t *card, int w, int h, const char *file)
{
    lv_obj_t *box = ui_active_print_v32_thumb_box(card);
    if (!box || !s_active_print_thumb_canvas_buf) return;

    ui_active_print_v32_thumb_clear_placeholder(card);

    if (!s_active_print_thumb_canvas) {
        s_active_print_thumb_canvas = lv_canvas_create(box);
    }

    lv_canvas_set_buffer(s_active_print_thumb_canvas,
                         s_active_print_thumb_canvas_buf,
                         w,
                         h,
                         LV_COLOR_FORMAT_RGB565);

    ui_thumbnail_v32_fit_object(
        s_active_print_thumb_canvas,
        box,
        w,
        h,
        6);
    lv_obj_move_foreground(s_active_print_thumb_canvas);

    ui_active_print_v32_thumb_copy_file(file);
}

void ui_active_print_v32_thumb_apply_canvas_from_buffer(lv_obj_t *card, int w, int h, const char *file)
{
    lv_obj_t *box = ui_active_print_v32_thumb_box(card);
    if (!box || !s_active_print_thumb_canvas_buf) return;

    if (!s_active_print_thumb_canvas) {
        s_active_print_thumb_canvas = lv_canvas_create(box);
        lv_canvas_set_buffer(s_active_print_thumb_canvas,
                             s_active_print_thumb_canvas_buf,
                             w,
                             h,
                             LV_COLOR_FORMAT_RGB565);
        lv_obj_move_foreground(s_active_print_thumb_canvas);
    } else {
        lv_obj_invalidate(s_active_print_thumb_canvas);
        lv_obj_move_foreground(s_active_print_thumb_canvas);
    }

    ui_thumbnail_v32_fit_object(
        s_active_print_thumb_canvas,
        box,
        w,
        h,
        6);

    ui_active_print_v32_thumb_clear_placeholder(card);
    ui_active_print_v32_thumb_copy_file(file);
}


lv_obj_t *ui_active_print_v32_create(
    lv_obj_t *parent,
    int x,
    int y,
    int w,
    int h)
{
    const ui_dashboard_rect_t rect = {x, y, w, h};
    const ui_dashboard_active_print_layout_t layout = {
        .heading_x = 18,
        .heading_y = 12,
        .filename_x = 140,
        .filename_y = 14,
        .preview_x = 20,
        .preview_y = 42,
        .preview_width = 0,
        .preview_height = 0,
        .footer_x = 18,
        .footer_bottom = 31,
    };

    return ui_active_print_v32_create_profile(
        parent,
        &rect,
        &layout);
}


lv_obj_t *ui_active_print_v32_create_profile(
    lv_obj_t *parent,
    const ui_dashboard_rect_t *rect,
    const ui_dashboard_active_print_layout_t *layout)
{
    if (!parent || !rect || !layout) {
        return NULL;
    }

    const int x = rect->x;
    const int y = rect->y;
    const int w = rect->width;
    const int h = rect->height;
    int preview_width =
        layout->preview_width > 0
            ? layout->preview_width
            : w - (layout->preview_x * 2);
    int preview_height =
        layout->preview_height > 0
            ? layout->preview_height
            : h - layout->footer_bottom -
                layout->preview_y - 12;

    if (preview_width < 120) preview_width = 120;
    if (preview_height < 80) preview_height = 80;

    /*
     * The Active Print center card now uses the exact same shared
     * Operator shell as the Drybox center cards.
     *
     * Thumbnail geometry and runtime behavior remain unchanged.
     */
    lv_obj_t *panel =
        ui_create_operator_card(
            parent,
            x,
            y,
            w,
            h);

    if (!panel) {
        return NULL;
    }

    active_print_ctx_t *ctx =
        lv_malloc(sizeof(active_print_ctx_t));

    if (!ctx) {
        return panel;
    }

    memset(ctx, 0, sizeof(*ctx));

    /*
     * Shared Drybox-style card heading.
     */
    ui_create_operator_card_heading(
        panel,
        "ACTIVE PRINT",
        layout->heading_x,
        layout->heading_y);

    /*
     * The expanded Dashboard card gives the preview another 30 px
     * vertically while preserving its established width and canvas path.
     */
    ctx->preview_box = lv_obj_create(panel);

    lv_obj_clear_flag(
        ctx->preview_box,
        LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_size(
        ctx->preview_box,
        preview_width,
        preview_height);

    lv_obj_set_pos(
        ctx->preview_box,
        layout->preview_x,
        layout->preview_y);

    ui_apply_preview_style(
        ctx->preview_box);

    lv_obj_add_flag(
        ctx->preview_box,
        LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(
        ctx->preview_box,
        active_print_preview_clicked_cb,
        LV_EVENT_CLICKED,
        NULL);

    ctx->preview_label =
        lv_label_create(ctx->preview_box);

    lv_label_set_text(
        ctx->preview_label,
        "PRINT\nTHUMBNAIL");

    ui_apply_text_body_large(
        ctx->preview_label);
    ui_apply_label_dim(
        ctx->preview_label);

    lv_obj_set_style_text_align(
        ctx->preview_label,
        LV_TEXT_ALIGN_CENTER,
        0);

    lv_obj_center(ctx->preview_label);

    /*
     * Status strip overlays the lower edge of the panel without
     * changing any existing setter or formatting behavior.
     */
    ctx->footer = lv_label_create(panel);

    lv_label_set_text(
        ctx->footer,
        "LAYER --/--    ELAPSED --:--    REM --:--");

    lv_obj_set_width(
        ctx->footer,
        w - (layout->footer_x * 2));

    lv_label_set_long_mode(
        ctx->footer,
        LV_LABEL_LONG_CLIP);

    ui_apply_text_caption(
        ctx->footer);

    lv_obj_set_style_text_color(
        ctx->footer,
        UI_ACCENT_CYAN,
        0);

    lv_obj_set_style_text_align(
        ctx->footer,
        LV_TEXT_ALIGN_CENTER,
        0);

    /*
     * Keep layer and timing metadata visually integrated with the
     * Active Print card instead of placing it on a dark overlay.
     */
    lv_obj_set_style_bg_opa(
        ctx->footer,
        LV_OPA_TRANSP,
        0);

    lv_obj_set_style_pad_top(
        ctx->footer,
        ui_theme_density_metric(2, 4, 6),
        0);

    lv_obj_set_style_pad_bottom(
        ctx->footer,
        ui_theme_density_metric(2, 4, 6),
        0);

    lv_obj_set_pos(
        ctx->footer,
        layout->footer_x,
        h - layout->footer_bottom);

    lv_obj_move_foreground(ctx->footer);

    lv_obj_set_user_data(panel, ctx);

    lv_obj_add_event_cb(
        panel,
        active_print_delete_cb,
        LV_EVENT_DELETE,
        ctx);

    s_active_print_panel = panel;

    return panel;
}


void ui_active_print_v32_set(lv_obj_t *panel,
                             const char *layer,
                             const char *elapsed,
                             const char *remaining)
{
    if (!panel) return;
    active_print_ctx_t *ctx = (active_print_ctx_t *)lv_obj_get_user_data(panel);
    if (!ctx) return;

    char buf[128];
    snprintf(buf, sizeof(buf),
             "LAYER %s    ELAPSED %s    %s",
             layer ? layer : "--/--",
             elapsed ? elapsed : "--:--",
             remaining ? remaining : "REM --:--");

    lv_label_set_text(ctx->footer, buf);
}
