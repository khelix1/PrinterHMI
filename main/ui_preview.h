#pragma once

#include "lvgl.h"

typedef struct ui_preview_v32 ui_preview_v32_t;

ui_preview_v32_t *ui_preview_v32_create(lv_obj_t *parent, int x, int y, int w, int h);

void ui_preview_v32_set_placeholder(ui_preview_v32_t *p, const char *text);
void ui_preview_v32_clear(ui_preview_v32_t *p);

/* Future image target API. For now this only proves the object path safely. */
bool ui_preview_v32_show_image_src(ui_preview_v32_t *p, const void *src);
void ui_preview_v32_show_manager_status(ui_preview_v32_t *p);

