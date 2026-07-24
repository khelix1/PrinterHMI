#pragma once

#include "lvgl.h"

/*
 * Create a standard Theme A information card.
 *
 * The returned object is the card's value label.
 * Its card container is available through:
 *
 *     lv_obj_get_parent(value_label)
 *
 * Page-specific icons, callbacks, and layout adjustments remain with
 * the consuming page module.
 */
lv_obj_t *ui_info_card_create(lv_obj_t *parent,
                              const char *title,
                              const char *value,
                              int32_t x,
                              int32_t y,
                              int32_t width,
                              int32_t height);
