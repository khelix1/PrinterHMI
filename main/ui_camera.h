#pragma once

#include <stdbool.h>
void ui_camera_show(void);
/* Show the live Camera viewer as a full-display takeover. */
void ui_camera_show_fullscreen(void);
void ui_camera_hide(void);
/* Deletes the cached page so it will be rebuilt with the active theme. */
void ui_camera_destroy(void);
/* Re-read active printer camera names after Camera Setup saves. */
void ui_camera_refresh_catalog(void);
/* Pause background MJPEG work while a Camera Setup dialog is active. */
void ui_camera_set_setup_active(bool active);
