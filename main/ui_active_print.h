#pragma once

#include "lvgl.h"
#include "ui_dashboard_layout_profile.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

lv_obj_t *ui_active_print_create(
    lv_obj_t *parent,
    int x,
    int y,
    int w,
    int h);

lv_obj_t *ui_active_print_create_profile(
    lv_obj_t *parent,
    const ui_dashboard_rect_t *rect,
    const ui_dashboard_active_print_layout_t *layout);

lv_obj_t *ui_active_print_thumb_box(lv_obj_t *card);

lv_obj_t **ui_active_print_thumb_canvas_ref(void);
uint16_t **ui_active_print_thumb_canvas_buf_ref(void);
char *ui_active_print_thumb_canvas_file(void);
size_t ui_active_print_thumb_canvas_file_size(void);

void ui_active_print_thumb_set_placeholder(lv_obj_t *card, const char *text);
void ui_active_print_thumb_clear_placeholder(lv_obj_t *card);
bool ui_active_print_thumb_canvas_matches(const char *file);
bool ui_active_print_thumb_ensure_canvas_buffer(size_t pixels);
void ui_active_print_thumb_delete_canvas(void);
void ui_active_print_thumb_forget_canvas_file(void);
void ui_active_print_thumb_show_canvas_from_buffer(lv_obj_t *card, int w, int h, const char *file);
void ui_active_print_thumb_apply_canvas_from_buffer(lv_obj_t *card, int w, int h, const char *file);
void ui_active_print_set_filename(lv_obj_t *panel,
                                      const char *filename);

void ui_active_print_set(lv_obj_t *panel,
                             const char *layer,
                             const char *elapsed,
                             const char *remaining);
