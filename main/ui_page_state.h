#pragma once

#include "lvgl.h"

typedef enum {
    UI_PAGE_STATE_LOADING = 0,
    UI_PAGE_STATE_EMPTY,
    UI_PAGE_STATE_OFFLINE,
    UI_PAGE_STATE_ERROR,
} ui_page_state_kind_t;

typedef struct ui_page_state_v32 ui_page_state_v32_t;

ui_page_state_v32_t *ui_page_state_v32_create(
    lv_obj_t *parent, int x, int y, int width, int height);
void ui_page_state_v32_show(ui_page_state_v32_t *state,
                            ui_page_state_kind_t kind,
                            const char *title,
                            const char *detail);
void ui_page_state_v32_hide(ui_page_state_v32_t *state);
