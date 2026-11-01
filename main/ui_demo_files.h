#pragma once
#include "lvgl.h"
#include <stdbool.h>
/* UI-thread only. Uses the real Files widgets with a process-local sample
 * library. */
void ui_demo_files_show(bool (*send)(const char *));
void ui_demo_files_close(void);
const lv_image_dsc_t *ui_demo_files_image(const char *path);
