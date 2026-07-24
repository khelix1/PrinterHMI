#pragma once

#include "lvgl.h"

typedef void (*ui_appearance_changed_cb_t)(void);

void ui_appearance_popups_show_accent(ui_appearance_changed_cb_t changed_cb);
void ui_appearance_popups_show_density(ui_appearance_changed_cb_t changed_cb);
void ui_appearance_popups_show_accessibility(ui_appearance_changed_cb_t changed_cb);
void ui_appearance_popups_close_all(void);
