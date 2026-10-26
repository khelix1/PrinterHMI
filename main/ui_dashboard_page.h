#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Theme-aware Dashboard layout; runtime state remains owned by ui_dashboard. */
typedef struct {
    lv_obj_t *root;

    lv_obj_t *banner_host;
    lv_obj_t *active_print_host;
    lv_obj_t *machine_status_host;
    lv_obj_t *command_host;
} ui_dashboard_page_t;

ui_dashboard_page_t ui_dashboard_page_create(
    lv_obj_t *parent);

void ui_dashboard_page_destroy(
    ui_dashboard_page_t *page);

#ifdef __cplusplus
}
#endif
