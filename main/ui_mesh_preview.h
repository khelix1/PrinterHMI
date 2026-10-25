#pragma once
#include <stdbool.h>
#include <stdint.h>
void ui_mesh_preview_open(void);
void ui_mesh_preview_close(void);
bool ui_mesh_preview_active(void);
void ui_mesh_preview_multitouch_update(uint8_t,int32_t,int32_t,int32_t,int32_t);
