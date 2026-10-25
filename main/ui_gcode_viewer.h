#pragma once
#include <stdint.h>
/* UI-thread only. NULL/empty file opens the sample/file-selection entry state. */
void ui_gcode_viewer_open(const char *file);
void ui_gcode_viewer_close(void);

/* Raw touch observer, on the LVGL input/UI thread, like Bed Mesh. */
void printerhmi_gcode_viewer_multitouch_update(uint8_t count, int32_t x0,
    int32_t y0, int32_t x1, int32_t y1);
