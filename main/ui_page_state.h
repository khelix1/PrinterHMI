#pragma once

#include "lvgl.h"

typedef enum {
    UI_PAGE_STATE_LOADING = 0,
    UI_PAGE_STATE_EMPTY,
    UI_PAGE_STATE_OFFLINE,
    UI_PAGE_STATE_ERROR,
} ui_page_state_kind_t;

typedef struct ui_page_state ui_page_state_t;

/* owner_slot storage must outlive the panel. Deletion clears it only while it
 * still refers to this panel, preserving a replacement bound to the same slot. */
ui_page_state_t *ui_page_state_create_owned(
    lv_obj_t *parent, int x, int y, int width, int height, ui_page_state_t **owner_slot);
void ui_page_state_show(ui_page_state_t *state,
                            ui_page_state_kind_t kind,
                            const char *title,
                            const char *detail);
void ui_page_state_hide(ui_page_state_t *state);
