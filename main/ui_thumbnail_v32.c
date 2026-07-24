#include "ui_thumbnail_v32.h"
#include "ui_theme.h"
#include "ui_widgets.h"

struct ui_thumbnail_v32 {
    lv_obj_t *box;
    lv_obj_t *label;
    lv_obj_t *canvas;
    void *canvas_buf;
    char loaded_file[160];
};

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

    return thumb;
}



lv_obj_t *ui_thumbnail_v32_box(ui_thumbnail_v32_t *thumb)
{
    return thumb ? thumb->box : NULL;
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
    lv_image_set_scale(thumb->canvas, scale);
    lv_obj_center(thumb->canvas);
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
