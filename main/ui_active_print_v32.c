#include "ui_active_print_v32.h"
#include "ui_theme.h"
#include "ui_widgets.h"
#include "esp_heap_caps.h"
#include <string.h>
#include <stdio.h>

typedef struct {
    lv_obj_t *preview_box;
    lv_obj_t *preview_label;
    lv_obj_t *filename;
    lv_obj_t *footer;
} active_print_ctx_t;


static lv_obj_t *s_active_print_panel = NULL;
static lv_obj_t *s_active_print_thumb_canvas = NULL;
static uint16_t *s_active_print_thumb_canvas_buf = NULL;
static char s_active_print_thumb_canvas_file[160] = "";


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

    lv_obj_center(s_active_print_thumb_canvas);
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
        lv_obj_center(s_active_print_thumb_canvas);
        lv_obj_move_foreground(s_active_print_thumb_canvas);
    } else {
        lv_obj_invalidate(s_active_print_thumb_canvas);
        lv_obj_move_foreground(s_active_print_thumb_canvas);
    }

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
    if (!parent) {
        return NULL;
    }

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
        18,
        12);

    ctx->filename = lv_label_create(panel);

    lv_label_set_text(
        ctx->filename,
        "No active file");

    lv_obj_set_width(
        ctx->filename,
        w - 160);

    lv_label_set_long_mode(
        ctx->filename,
        LV_LABEL_LONG_DOT);

    ui_apply_text_body(
        ctx->filename);
    ui_apply_label_bright(
        ctx->filename);

    lv_obj_set_style_text_align(
        ctx->filename,
        LV_TEXT_ALIGN_RIGHT,
        0);

    lv_obj_set_pos(
        ctx->filename,
        140,
        14);

    /*
     * Preserve the existing 310 x 215 preview surface. The current
     * Dashboard renderer and restored canvas already target this
     * established area.
     */
    ctx->preview_box = lv_obj_create(panel);

    lv_obj_clear_flag(
        ctx->preview_box,
        LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_size(
        ctx->preview_box,
        310,
        215);

    lv_obj_set_pos(
        ctx->preview_box,
        40,
        42);

    ui_apply_preview_style(
        ctx->preview_box);

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
        w - 36);

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

    lv_obj_set_style_bg_color(
        ctx->footer,
        UI_BG_DEEP,
        0);

    lv_obj_set_style_bg_opa(
        ctx->footer,
        LV_OPA_80,
        0);

    lv_obj_set_style_pad_top(
        ctx->footer,
        4,
        0);

    lv_obj_set_style_pad_bottom(
        ctx->footer,
        4,
        0);

    lv_obj_set_pos(
        ctx->footer,
        18,
        h - 31);

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


void ui_active_print_v32_set_filename(lv_obj_t *panel,
                                      const char *filename)
{
    if (!panel) panel = s_active_print_panel;
    if (!panel) return;

    active_print_ctx_t *ctx =
        (active_print_ctx_t *)lv_obj_get_user_data(panel);
    if (!ctx || !ctx->filename) return;

    if (!filename ||
        !filename[0] ||
        strcmp(filename, "No file") == 0 ||
        strcmp(filename, "--") == 0) {
        lv_label_set_text(ctx->filename, "No active file");
        return;
    }

    lv_label_set_text(ctx->filename, filename);
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
