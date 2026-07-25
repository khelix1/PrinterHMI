#pragma once

#include "lvgl.h"
#include "ui_dashboard_status_v32.h"

/*
 * PrinterHMI Dashboard
 * Clean Theme A dashboard built beside the existing v3.1 UI.
 */

ui_dashboard_status_v32_t ui_dashboard_v32_create_status(
    lv_obj_t *parent);
void ui_dashboard_v32_create(void);
void ui_dashboard_v32_update(void);
void ui_dashboard_v32_set_active_print_file(const char *filename);
void ui_dashboard_v32_set_active_print(const char *layer, const char *elapsed, const char *remaining);
void ui_dashboard_v32_destroy(void);

void ui_dashboard_v32_set_machine_connection(
    bool online);

void ui_dashboard_v32_set_active_hotend(
    const char *name,
    const char *value);

void ui_dashboard_v32_set_machine(
    const char *nozzle,
    const char *bed,
    const char *chamber,
    const char *humidity,
    const char *speed,
    const char *flow,
    const char *fan
);

void ui_dashboard_v32_set_banner(
    const char *state,
    const char *file,
    const char *eta,
    const char *progress
);
void ui_dashboard_v32_status_popup_show(const char *title_text, const char *body);
void ui_dashboard_v32_status_popup_close(void);


lv_obj_t *ui_dashboard_v32_thumb_box(void);
bool ui_dashboard_v32_thumb_ready(void);
void ui_dashboard_v32_thumb_set_placeholder(const char *text);
void ui_dashboard_v32_thumb_clear_placeholder(void);
lv_obj_t **ui_dashboard_v32_thumb_canvas_ref(void);
uint16_t **ui_dashboard_v32_thumb_canvas_buf_ref(void);
char *ui_dashboard_v32_thumb_canvas_file(void);
size_t ui_dashboard_v32_thumb_canvas_file_size(void);
bool ui_dashboard_v32_thumb_canvas_matches(const char *file);
bool ui_dashboard_v32_thumb_ensure_canvas_buffer(size_t pixels);
void ui_dashboard_v32_thumb_delete_canvas(void);
void ui_dashboard_v32_thumb_forget_canvas_file(void);
void ui_dashboard_v32_thumb_show_canvas_from_buffer(int w, int h, const char *file);
void ui_dashboard_v32_thumb_apply_canvas_from_buffer(int w, int h, const char *file);
