#include "lvgl.h"
#include "ui_thumbnail.h"
#pragma once

void ui_files_show(void);
void ui_files_hide(void);
void ui_files_refresh(void);

typedef void (*ui_files_refresh_cb_t)(void);
typedef void (*ui_files_select_cb_t)(const char *path);
typedef void (*ui_files_preview_cb_t)(const char *path);
typedef void (*ui_files_search_cb_t)(const char *query);
typedef void (*ui_files_action_cb_t)(void);
typedef void (*ui_files_folder_cb_t)(const char *path);

void ui_files_add_file_button(const char *path, int y);
void ui_files_set_status(const char *text);
lv_obj_t *ui_files_get_popup(void);
void ui_files_set_callbacks(ui_files_refresh_cb_t refresh_cb,
                                ui_files_select_cb_t select_cb,
                                ui_files_preview_cb_t preview_cb);
void ui_files_set_file_thumbnail(const char *path,
                                     const lv_image_dsc_t *image);
void ui_files_clear_rows(void);
void ui_files_add_file_entry(const char *path,
                                 double size,
                                 double modified,
                                 int y);
void ui_files_add_folder_button(const char *name,
                                    const char *path,
                                    int y);
void ui_files_set_breadcrumb(const char *path);
void ui_files_set_sort_text(const char *text);
void ui_files_set_search_text(const char *text);
void ui_files_set_browser_callbacks(
    ui_files_search_cb_t search_cb,
    ui_files_action_cb_t sort_cb,
    ui_files_folder_cb_t folder_cb,
    ui_files_action_cb_t up_cb);

typedef void (*ui_files_detail_cb_t)(void);

void ui_files_show_detail_popup(const char *filename_text,
                                    const char *metadata_text,
                                    lv_obj_t **thumb_box_out,
                                    ui_thumbnail_t **thumb_view_out,
                                    ui_files_detail_cb_t cancel_cb,
                                    ui_files_detail_cb_t start_cb);
void ui_files_close_detail_popup(void);
bool ui_files_detail_is_open(void);
void ui_files_update_detail_metadata(const char *metadata_text,
                                         bool ready);
