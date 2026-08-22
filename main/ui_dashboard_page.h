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
    ui_dashboard_status_t print_status;
    lv_obj_t *command_host;

    /* Future orbital Dashboard live bindings. */
    lv_obj_t *future_nozzle;
    lv_obj_t *future_bed;
    lv_obj_t *future_progress;
    lv_obj_t *future_camera;
    lv_obj_t *future_core;
} ui_dashboard_page_t;

ui_dashboard_page_t ui_dashboard_page_create(
    lv_obj_t *parent);

void ui_dashboard_page_future_update(
    ui_dashboard_page_t *page,
    const char *nozzle,
    const char *bed,
    const char *progress,
    const char *camera,
    const char *state);

void ui_dashboard_page_destroy(
    ui_dashboard_page_t *page);

#ifdef __cplusplus
}
#endif
