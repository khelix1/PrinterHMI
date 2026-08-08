#include "ui_preview.h"
#include "ui_theme.h"
#include "ui_widgets.h"
#include "thumbnail_manager.h"

#include <stdbool.h>
#include <string.h>

struct ui_preview_v32 {
    lv_obj_t *box;
    lv_obj_t *label;
    lv_obj_t *image;
    int w;
    int h;
};

static void preview_show_placeholder(ui_preview_v32_t *p, const char *text)
{
    if (!p) return;

    if (p->image) {
        lv_obj_add_flag(p->image, LV_OBJ_FLAG_HIDDEN);
    }

    if (p->label) {
        lv_label_set_text(p->label, text ? text : "PREVIEW");
        lv_obj_clear_flag(p->label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_center(p->label);
    }
}

ui_preview_v32_t *ui_preview_v32_create(lv_obj_t *parent, int x, int y, int w, int h)
{
    ui_preview_v32_t *p = lv_malloc(sizeof(ui_preview_v32_t));
    if (!p) return NULL;

    memset(p, 0, sizeof(*p));
    p->w = w;
    p->h = h;

    p->box = ui_create_card(parent);
    lv_obj_set_size(p->box, w, h);
    lv_obj_set_pos(p->box, x, y);
    ui_apply_preview_style(p->box);
    lv_obj_clear_flag(p->box, LV_OBJ_FLAG_SCROLLABLE);

    p->image = lv_image_create(p->box);
    lv_obj_center(p->image);
    lv_obj_add_flag(p->image, LV_OBJ_FLAG_HIDDEN);

    p->label = ui_create_card_subtitle(p->box, "PREVIEW");
    lv_obj_center(p->label);

    return p;
}

void ui_preview_v32_set_placeholder(ui_preview_v32_t *p, const char *text)
{
    preview_show_placeholder(p, text);
}

void ui_preview_v32_clear(ui_preview_v32_t *p)
{
    preview_show_placeholder(p, "PREVIEW");
}

bool ui_preview_v32_show_image_src(ui_preview_v32_t *p, const void *src)
{
    if (!p || !p->image || !src) {
        return false;
    }

    lv_image_set_src(p->image, src);
    lv_obj_center(p->image);
    lv_obj_clear_flag(p->image, LV_OBJ_FLAG_HIDDEN);

    if (p->label) {
        lv_obj_add_flag(p->label, LV_OBJ_FLAG_HIDDEN);
    }

    return true;
}


void ui_preview_v32_show_manager_status(ui_preview_v32_t *p)
{
    if (!p) return;

    if (thumbnail_manager_v32_has_ready_image()) {
        ui_preview_v32_set_placeholder(p, thumbnail_manager_v32_cache_path());
    } else {
        ui_preview_v32_set_placeholder(p, thumbnail_manager_v32_status_text());
    }
}
