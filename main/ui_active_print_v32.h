#pragma once

#include "lvgl.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

lv_obj_t *ui_active_print_v32_create(lv_obj_t *parent, int x, int y, int w, int h);
lv_obj_t *ui_active_print_v32_thumb_box(lv_obj_t *card);

lv_obj_t **ui_active_print_v32_thumb_canvas_ref(void);
uint16_t **ui_active_print_v32_thumb_canvas_buf_ref(void);
char *ui_active_print_v32_thumb_canvas_file(void);
size_t ui_active_print_v32_thumb_canvas_file_size(void);

void ui_active_print_v32_thumb_set_placeholder(lv_obj_t *card, const char *text);
void ui_active_print_v32_thumb_clear_placeholder(lv_obj_t *card);
bool ui_active_print_v32_thumb_canvas_matches(const char *file);
bool ui_active_print_v32_thumb_ensure_canvas_buffer(size_t pixels);
void ui_active_print_v32_thumb_delete_canvas(void);
void ui_active_print_v32_thumb_forget_canvas_file(void);
void ui_active_print_v32_thumb_show_canvas_from_buffer(lv_obj_t *card, int w, int h, const char *file);
void ui_active_print_v32_thumb_apply_canvas_from_buffer(lv_obj_t *card, int w, int h, const char *file);
void ui_active_print_v32_set_filename(lv_obj_t *panel,
                                      const char *filename);

void ui_active_print_v32_set(lv_obj_t *panel,
                             const char *layer,
                             const char *elapsed,
                             const char *remaining);
