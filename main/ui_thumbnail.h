#pragma once

#include "lvgl.h"

typedef struct ui_thumbnail ui_thumbnail_t;

/* Component handles are released with their box. Bind persistent handles below
 * when updates can arrive after the hierarchy is deleted. */
ui_thumbnail_t *ui_thumbnail_create(lv_obj_t *parent, int x, int y, int w, int h);
/* Optional persistent owner slot; storage must outlive the component.
 * Root deletion clears it only while it still points to this component. */
void ui_thumbnail_bind_owner(ui_thumbnail_t *thumb, ui_thumbnail_t **owner_slot);
/* Pin high-resolution loading to the file represented by this image. */
void ui_thumbnail_show_file_image(ui_thumbnail_t *thumb, const lv_image_dsc_t *dsc,
    int scale, const char *file);
lv_obj_t *ui_thumbnail_box(ui_thumbnail_t *thumb);

void ui_thumbnail_fit_object(
    lv_obj_t *object,
    lv_obj_t *box,
    int source_width,
    int source_height,
    int inset);

void ui_thumbnail_set_placeholder(ui_thumbnail_t *thumb, const char *text);
