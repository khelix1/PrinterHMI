#pragma once

#include "lvgl.h"
#include "ui_page_geometry.h"

/* Shared page-relative geometry for every primary live-status bar. */
#define UI_STATUS_BAR_X       UI_PAGE_RAIL_X
#define UI_STATUS_BAR_Y       52
#define UI_STATUS_BAR_WIDTH   UI_PAGE_RAIL_WIDTH
#define UI_STATUS_BAR_HEIGHT  54

lv_obj_t *ui_status_banner_create(lv_obj_t *parent, int x, int y, int w, int h);
void ui_status_banner_set(lv_obj_t *banner,
                              const char *state,
                              const char *file,
                              const char *eta,
                              const char *progress);
void ui_status_banner_set_simple(lv_obj_t *banner,
                                     const char *state,
                                     const char *message);

/* Compatibility accessors for page adapters that retain label handles. */
lv_obj_t *ui_status_banner_state_label(lv_obj_t *banner);
