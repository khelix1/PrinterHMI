#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lvgl.h"

typedef enum {
    THUMBNAIL_V32_STATE_IDLE = 0,
    THUMBNAIL_V32_STATE_NO_FILE,
    THUMBNAIL_V32_STATE_PENDING,
    THUMBNAIL_V32_STATE_READY,
    THUMBNAIL_V32_STATE_ERROR,
} thumbnail_v32_state_t;

typedef enum {
    THUMBNAIL_MANAGER_V32_RESULT_IDLE = 0,
    THUMBNAIL_MANAGER_V32_RESULT_LOADING,
    THUMBNAIL_MANAGER_V32_RESULT_READY,
    THUMBNAIL_MANAGER_V32_RESULT_FAILED,
} thumbnail_manager_v32_result_t;

void thumbnail_manager_v32_init(void);
void thumbnail_manager_v32_set_file(const char *gcode_file);
void thumbnail_manager_v32_clear(void);
void thumbnail_manager_v32_set_ready_image(const char *cache_path);
void thumbnail_manager_v32_set_error(const char *status_text);

thumbnail_v32_state_t thumbnail_manager_v32_state(void);
const char *thumbnail_manager_v32_file(void);
const char *thumbnail_manager_v32_cache_path(void);
const char *thumbnail_manager_v32_status_text(void);

void thumbnail_manager_v32_mark_pending(void);
void thumbnail_manager_v32_mark_ready(void);
void thumbnail_manager_v32_mark_failed(void);
void thumbnail_manager_v32_mark_result(bool ok);

bool thumbnail_manager_v32_is_ready(void);
bool thumbnail_manager_v32_has_failed(void);

bool thumbnail_manager_v32_task_running(void);
void thumbnail_manager_v32_set_task_running(bool running);

thumbnail_manager_v32_result_t
thumbnail_manager_v32_result(void);

bool thumbnail_manager_v32_force_refresh(void);
void thumbnail_manager_v32_set_force_refresh(bool force_refresh);

bool thumbnail_manager_v32_has_ready_image(void);
bool thumbnail_manager_v32_copy_cache_path(char *out, size_t out_len);

void thumbnail_manager_v32_url_encode(const char *in, char *out, size_t out_sz);


bool thumbnail_manager_v32_cache_path_for_file(const char *gcode_file,
                                               char *out,
                                               size_t out_sz);


bool thumbnail_manager_v32_load_cache_file(const char *path,
                                           uint8_t **out_buf,
                                           size_t *out_len);


bool thumbnail_manager_v32_store_cache_file(const char *path,
                                            const uint8_t *buf,
                                            size_t len);




uint8_t *thumbnail_manager_v32_png_data(void);
size_t thumbnail_manager_v32_png_size(void);
bool thumbnail_manager_v32_has_png(void);
void thumbnail_manager_v32_clear_png(void);
bool thumbnail_manager_v32_load_selected_cache(const char *selected_file);
bool thumbnail_manager_v32_store_selected_cache(const char *selected_file);

void thumbnail_manager_v32_take_png(uint8_t *buf, size_t len);
lv_image_dsc_t *thumbnail_manager_v32_image_dsc(void);
void thumbnail_manager_v32_prepare_raw_image(void);

typedef bool (*thumbnail_manager_v32_download_cb_t)(const char *thumb_path);


bool thumbnail_manager_v32_run_download_task(void *arg,
                                             const char *host,
                                             int port,
                                             const char *selected_file,
                                             bool force_refresh,
                                             bool sd_ok);


bool thumbnail_manager_v32_download_ram(const char *host,
                                       int port,
                                       const char *selected_file,
                                       const char *thumb_path,
                                       bool force_refresh,
                                       bool sd_ok);

void thumbnail_manager_v32_free_png_buffer(uint8_t **buf, size_t *len);


void thumbnail_manager_v32_set_png_buffer(uint8_t **dst_buf,
                                          size_t *dst_len,
                                          uint8_t *src_buf,
                                          size_t src_len);

bool thumbnail_manager_v32_start_download_task(
    const char *host,
    int port,
    const char *selected_file,
    const char *thumb_path,
    bool force_refresh,
    bool sd_ok);
