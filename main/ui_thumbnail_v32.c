#include "ui_thumbnail_v32.h"
#include "ui_preview_lightbox_v32.h"
#include "ui_theme.h"
#include "ui_widgets.h"

struct ui_thumbnail_v32 {
    lv_obj_t *box;
    lv_obj_t *label;
    lv_obj_t *canvas;
    void *canvas_buf;
    char loaded_file[160];
};

static void thumbnail_clicked_cb(lv_event_t *event)
{
    ui_thumbnail_v32_t *thumb =
        (ui_thumbnail_v32_t *)lv_event_get_user_data(event);

    if (lv_event_get_code(event) != LV_EVENT_CLICKED ||
        !thumb ||
        !thumb->canvas) {
        return;
    }

    ui_preview_lightbox_v32_show_object(thumb->canvas);
}

static void thumbnail_enable_lightbox(ui_thumbnail_v32_t *thumb)
{
    if (!thumb || !thumb->box) {
        return;
    }

    lv_obj_add_flag(thumb->box, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(
        thumb->box,
        thumbnail_clicked_cb,
        LV_EVENT_CLICKED,
        thumb);
}

ui_thumbnail_v32_t *ui_thumbnail_v32_create(lv_obj_t *parent, int x, int y, int w, int h)
{
    ui_thumbnail_v32_t *thumb = lv_malloc(sizeof(ui_thumbnail_v32_t));
    if (!thumb) return NULL;

    thumb->canvas = NULL;
    thumb->canvas_buf = NULL;
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


ui_thumbnail_v32_t *ui_thumbnail_v32_wrap(lv_obj_t *box)
{
    if (!box) return NULL;

    ui_thumbnail_v32_t *thumb = lv_malloc(sizeof(ui_thumbnail_v32_t));
    if (!thumb) return NULL;

    thumb->box = box;
    thumb->label = NULL;
    thumb->canvas = NULL;
    thumb->canvas_buf = NULL;
    thumb->loaded_file[0] = 0;

    lv_obj_clear_flag(thumb->box, LV_OBJ_FLAG_SCROLLABLE);

    thumb->label = ui_create_card_subtitle(thumb->box, "THUMBNAIL");
    lv_obj_center(thumb->label);
    thumbnail_enable_lightbox(thumb);

    return thumb;
}



lv_obj_t *ui_thumbnail_v32_box(ui_thumbnail_v32_t *thumb)
{
    return thumb ? thumb->box : NULL;
}

int ui_thumbnail_v32_fit_scale(
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

void ui_thumbnail_v32_fit_object(
    lv_obj_t *object,
    lv_obj_t *box,
    int source_width,
    int source_height,
    int inset)
{
    if (!object || !box) return;

    lv_image_set_scale(
        object,
        ui_thumbnail_v32_fit_scale(
            box,
            source_width,
            source_height,
            inset));
    lv_obj_center(object);
}


void ui_thumbnail_v32_show_image(ui_thumbnail_v32_t *thumb, const lv_image_dsc_t *dsc, int scale)
{
    if (!thumb || !thumb->box || !dsc) return;

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
        ui_thumbnail_v32_fit_object(
            thumb->canvas,
            thumb->box,
            (int)dsc->header.w,
            (int)dsc->header.h,
            8);
    }
}


void ui_thumbnail_v32_set_placeholder(ui_thumbnail_v32_t *thumb, const char *text)
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

    lv_label_set_text(thumb->label, text ? text : "THUMBNAIL");
    lv_obj_clear_flag(thumb->label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_center(thumb->label);
}

void ui_thumbnail_v32_clear(ui_thumbnail_v32_t *thumb)
{
    ui_thumbnail_v32_set_placeholder(thumb, "THUMBNAIL");
}
