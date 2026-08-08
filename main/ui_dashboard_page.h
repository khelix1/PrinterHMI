#pragma once

#include "lvgl.h"
#include "ui_dashboard_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * New Theme B Dashboard page.
 *
 * This module is being built beside the legacy Dashboard until the
 * replacement page is complete and verified.
 *
 * Page layout remains Dashboard-specific. Visual styling comes from
 * the shared Operator theme.
 */
typedef struct {
    lv_obj_t *root;

    lv_obj_t *banner_host;
    lv_obj_t *active_print_host;
    lv_obj_t *machine_status_host;
    lv_obj_t *print_status_host;
    ui_dashboard_status_v32_t print_status;
    lv_obj_t *command_host;
} ui_dashboard_page_v32_t;

ui_dashboard_page_v32_t ui_dashboard_page_v32_create(
    lv_obj_t *parent);

void ui_dashboard_page_v32_destroy(
    ui_dashboard_page_v32_t *page);

#ifdef __cplusplus
}
#endif
