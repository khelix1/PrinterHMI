#pragma once

#include <stdbool.h>

#include "lvgl.h"

void ui_preview_lightbox_show(const lv_image_dsc_t *image);
void ui_preview_lightbox_show_object(lv_obj_t *image_object);
void ui_preview_lightbox_close(void);
bool ui_preview_lightbox_is_open(void);
