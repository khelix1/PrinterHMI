#pragma once

#include "lvgl.h"

/*
 * Create the standard Theme A page title and subtitle.
 *
 * The title text may already include an LVGL symbol, for example:
 *
 *     LV_SYMBOL_WIFI " NETWORK"
 *
 * Banners remain separate components owned by each page.
 */
void ui_page_title_create(lv_obj_t *parent,
                          const char *title_text,
                          const char *subtitle_text);
