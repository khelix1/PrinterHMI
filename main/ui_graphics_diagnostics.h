#pragma once
#include <stdbool.h>
#include "lvgl.h"

/* Call once under the BSP display lock. Single appliance display only.
 * Startup buffer/renderer information is always available. Periodic timing
 * requires CONFIG_PRINTERHMI_GRAPHICS_DIAGNOSTICS (off by default).
 * Never changes buffers, rendering or acceleration. */
bool ui_graphics_diagnostics_start(lv_display_t *display);
