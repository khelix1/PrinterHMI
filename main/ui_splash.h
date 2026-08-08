#pragma once

#include <stdbool.h>

void ui_splash_v32_create(void);
void ui_splash_v32_display_ready(void);
void ui_splash_v32_wifi_starting(void);
void ui_splash_v32_wifi_waiting(bool connected);
void ui_splash_v32_moonraker_ready(void);
void ui_splash_v32_dashboard_ready(void);
void ui_splash_v32_destroy(void);
