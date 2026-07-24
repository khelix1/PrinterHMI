#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"
#include "ui_theme.h"

/*
 * Creates a self-contained miniature operator interface for one theme.
 * Preview colors are explicit so rendering a card never changes the active
 * global theme. The whole card is one selectable button.
 */
lv_obj_t *ui_theme_preview_create(
    lv_obj_t *parent,
    ui_theme_id_t theme,
    bool selected,
    int32_t x,
    int32_t y,
    int32_t width,
    int32_t height,
    lv_event_cb_t event_cb,
    void *user_data);
