#pragma once

#include <stdbool.h>

void ui_splash_create(void);
void ui_splash_display_ready(void);
void ui_splash_wifi_starting(void);
void ui_splash_wifi_waiting(bool connected);
void ui_splash_moonraker_ready(void);
void ui_splash_dashboard_ready(void);
void ui_splash_destroy(void);
