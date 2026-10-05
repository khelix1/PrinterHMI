#include "ui_thumbnail.h"
#include "esp_heap_caps.h"
#include "thumbnail_render.h"
#include "misc/cache/instance/lv_image_cache.h"
#include <string.h>
#include "thumbnail_session.h"
#include "ui_text.h"
#include "ui_preview_lightbox.h"
#include "ui_theme.h"
#include "ui_widgets.h"

struct ui_thumbnail {
    lv_obj_t *box;
    lv_obj_t *label;
    lv_obj_t *canvas;
    void *canvas_buf;
    lv_image_dsc_t canvas_image;
    char loaded_file[160];
};

static void thumbnail_clicked_cb(lv_event_t *event)
{
    ui_thumbnail_t *thumb =
        (ui_thumbnail_t *)lv_event_get_user_data(event);

    if (lv_event_get_code(event) != LV_EVENT_CLICKED ||
        !thumb ||
        !thumb->canvas) {
        return;
    }

    lv_event_stop_bubbling(event);
    ui_preview_lightbox_show_file_object(
        thumb->canvas, thumbnail_session_selected_file());
}

static void thumbnail_deleted_cb(lv_event_t *event)
{
    /* Ignore bubbled child deletions before touching the component's data. */
    if (lv_event_get_target(event) != lv_event_get_current_target(event)) return;
    ui_thumbnail_t *thumb = lv_event_get_user_data(event);
    if (!thumb) return;
    lv_image_cache_drop(&thumb->canvas_image);
    heap_caps_free(thumb->canvas_buf);
    lv_free(thumb);
}

static void thumbnail_enable_lightbox(ui_thumbnail_t *thumb)
{
    if (!thumb || !thumb->box) {
        return;
    }

    lv_obj_add_event_cb(thumb->box, thumbnail_deleted_cb, LV_EVENT_DELETE, thumb);
    lv_obj_add_flag(thumb->box, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(
        thumb->box,
        thumbnail_clicked_cb,
        LV_EVENT_CLICKED,
        thumb);
}

ui_thumbnail_t *ui_thumbnail_create(lv_obj_t *parent, int x, int y, int w, int h)
{
    ui_thumbnail_t *thumb = lv_malloc(sizeof(ui_thumbnail_t));
    if (!thumb) return NULL;

    thumb->canvas = NULL;
    thumb->canvas_buf = NULL;
    memset(&thumb->canvas_image, 0, sizeof(thumb->canvas_image));
    thumb->loaded_file[0] = 0;

    thumb->box = ui_create_card(parent);
    lv_obj_set_size(thumb->box, w, h);
    lv_obj_set_pos(thumb->box, x, y);
    ui_apply_preview_style(thumb->box);

    thumb->label = ui_create_card_subtitle(thumb->box, "THUMBNAIL");
    lv_obj_center(thumb->label);
    thumbnail_enable_lightbox(thumb);

    return thumb;
}


ui_thumbnail_t *ui_thumbnail_wrap(lv_obj_t *box)
{
    if (!box) return NULL;

    ui_thumbnail_t *thumb = lv_malloc(sizeof(ui_thumbnail_t));
    if (!thumb) return NULL;

    thumb->box = box;
    thumb->label = NULL;
    thumb->canvas = NULL;
    thumb->canvas_buf = NULL;
    memset(&thumb->canvas_image, 0, sizeof(thumb->canvas_image));
    thumb->loaded_file[0] = 0;

    lv_obj_clear_flag(thumb->box, LV_OBJ_FLAG_SCROLLABLE);

    thumb->label = ui_create_card_subtitle(thumb->box, "THUMBNAIL");
    lv_obj_center(thumb->label);
    thumbnail_enable_lightbox(thumb);

    return thumb;
}



lv_obj_t *ui_thumbnail_box(ui_thumbnail_t *thumb)
{
    return thumb ? thumb->box : NULL;
}

int ui_thumbnail_fit_scale(
    lv_obj_t *box,
    int source_width,
    int source_height,
    int inset)
{
    if (!box || source_width <= 0 || source_height <= 0) {
        return 256;
    }

    /*
     * Printer creates and binds its cached image in the same LVGL pass.
     * Resolve the new preview well before reading its dimensions; otherwise
     * LVGL can still report zero and clamp the image to scale 1 (1/256x).
     */
    lv_obj_update_layout(box);

    int available_width = lv_obj_get_width(box) - (inset * 2);
    int available_height = lv_obj_get_height(box) - (inset * 2);

    if (available_width < 1) available_width = 1;
    if (available_height < 1) available_height = 1;

    int scale_x = (available_width * 256) / source_width;
    int scale_y = (available_height * 256) / source_height;
    int scale = scale_x < scale_y ? scale_x : scale_y;

    if (scale < 1) scale = 1;
    if (scale > 768) scale = 768;
    return scale;
}

void ui_thumbnail_fit_object(
    lv_obj_t *object,
    lv_obj_t *box,
    int source_width,
    int source_height,
    int inset)
{
    if (!object || !box) return;

    /* Compressed PNG descriptors have zero dimensions. lv_image_set_src()
     * resolves the decoder header; fit the decoded source, not that stub.
     */
    if (source_width <= 0 || source_height <= 0) {
        source_width = lv_image_get_src_width(object);
        source_height = lv_image_get_src_height(object);
    }
    if (source_width <= 0 || source_height <= 0) return;

    lv_image_set_scale(
        object,
        ui_thumbnail_fit_scale(
            box,
            source_width,
            source_height,
            inset));
    lv_obj_center(object);
}


void ui_thumbnail_fill_object(lv_obj_t *object, lv_obj_t *box,
    int source_width, int source_height)
{
    if (!object || !box) return;
    if (source_width <= 0 || source_height <= 0) {
        source_width = lv_image_get_src_width(object);
        source_height = lv_image_get_src_height(object);
    }
    if (source_width <= 0 || source_height <= 0) return;
    lv_obj_update_layout(box);
    int width = lv_obj_get_content_width(box);
    int height = lv_obj_get_content_height(box);
    if (width <= 0 || height <= 0) return;
    /* Round upward: integer LVGL scales must cover even the final edge. */
    int64_t scale_x = ((int64_t)width * 256 + source_width - 1) / source_width;
    int64_t scale_y = ((int64_t)height * 256 + source_height - 1) / source_height;
    int64_t scale = scale_x > scale_y ? scale_x : scale_y;
    if (scale < 1) scale = 1;
    if (scale > 8192) scale = 8192;
    lv_obj_remove_flag(box, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_set_style_clip_corner(box, true, 0);
    lv_image_set_scale(object, (uint32_t)scale);
    lv_obj_center(object);
}


void ui_thumbnail_show_image(ui_thumbnail_t *thumb, const lv_image_dsc_t *dsc, int scale)
{
    if (!thumb || !thumb->box || !dsc) return;

    /* Match Dashboard and Printer: render the original PNG once into the
     * shared RGB565 canvas. The lightbox uses this stable initial source, then
     * replaces it with the complete high-resolution thumbnail.
     */
    if (!thumb->canvas_buf) {
        thumb->canvas_buf = heap_caps_malloc(
            THUMBNAIL_PREVIEW_WIDTH * THUMBNAIL_PREVIEW_HEIGHT * sizeof(uint16_t),
            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
    if (!thumb->canvas_buf || !thumbnail_render_to_rgb565(
            dsc, thumb->canvas_buf, THUMBNAIL_PREVIEW_WIDTH, THUMBNAIL_PREVIEW_HEIGHT)) {
        ui_thumbnail_set_placeholder(thumb, "PREVIEW UNAVAILABLE");
        return;
    }
    lv_image_cache_drop(&thumb->canvas_image);
    memset(&thumb->canvas_image, 0, sizeof(thumb->canvas_image));
#if defined(LV_IMAGE_HEADER_MAGIC)
    thumb->canvas_image.header.magic = LV_IMAGE_HEADER_MAGIC;
#endif
    thumb->canvas_image.header.cf = LV_COLOR_FORMAT_RGB565;
    thumb->canvas_image.header.w = THUMBNAIL_PREVIEW_WIDTH;
    thumb->canvas_image.header.h = THUMBNAIL_PREVIEW_HEIGHT;
    thumb->canvas_image.header.stride = THUMBNAIL_PREVIEW_WIDTH * sizeof(uint16_t);
    thumb->canvas_image.data_size = THUMBNAIL_PREVIEW_WIDTH * THUMBNAIL_PREVIEW_HEIGHT * sizeof(uint16_t);
    thumb->canvas_image.data = thumb->canvas_buf;
    dsc = &thumb->canvas_image;

    if (thumb->label) {
        lv_obj_delete(thumb->label);
        thumb->label = NULL;
    }

    if (!thumb->canvas) {
        thumb->canvas = lv_image_create(thumb->box);
    }

    lv_image_set_src(thumb->canvas, dsc);
    if (scale > 0) {
        lv_image_set_scale(thumb->canvas, scale);
        lv_obj_center(thumb->canvas);
    } else {
        ui_thumbnail_fill_object(
            thumb->canvas,
            thumb->box,
            (int)dsc->header.w,
            (int)dsc->header.h);
    }
}


void ui_thumbnail_set_placeholder(ui_thumbnail_t *thumb, const char *text)
{
    if (!thumb) return;

    if (thumb->canvas) {
        lv_obj_delete(thumb->canvas);
        thumb->canvas = NULL;
    }

    thumb->loaded_file[0] = 0;

    if (!thumb->label) {
        thumb->label = ui_create_card_subtitle(thumb->box, text ? text : "THUMBNAIL");
    }

    lv_label_set_text(thumb->label, text ? text : ui_text("THUMBNAIL"));
    lv_obj_clear_flag(thumb->label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_center(thumb->label);
}

void ui_thumbnail_clear(ui_thumbnail_t *thumb)
{
    ui_thumbnail_set_placeholder(thumb, "THUMBNAIL");
}
