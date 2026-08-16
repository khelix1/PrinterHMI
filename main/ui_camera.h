#pragma once
void ui_camera_show(void);
void ui_camera_hide(void);
/* Deletes the cached page so it will be rebuilt with the active theme. */
void ui_camera_destroy(void);
/* Re-read active printer camera names after Camera Setup saves. */
void ui_camera_refresh_catalog(void);
