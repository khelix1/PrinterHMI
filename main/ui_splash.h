#pragma once

#include <stdbool.h>

void ui_splash_create(void);
void ui_splash_display_ready(void);
void ui_splash_wifi_starting(void);
void ui_splash_wifi_waiting(bool connected);
void ui_splash_moonraker_ready(void);
void ui_splash_dashboard_ready(void);
/* Call under the display lock after startup pages are constructed. */
void ui_splash_present_and_freeze(void);
void ui_splash_destroy(void);
/* Called after destroy, outside the display lock. Startup backlight is 100%.
 * The callbacks adapt hardware brightness and a yielding millisecond delay.
 */
void ui_splash_restore_brightness(int saved_percent,
    void (*set_percent)(int), void (*wait_ms)(unsigned));
