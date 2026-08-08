#pragma once

#include "ui_theme.h"

void ui_toast_v32_show(ui_status_kind_t kind,
                       const char *title,
                       const char *detail);
void ui_toast_v32_close(void);
