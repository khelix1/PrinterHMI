#pragma once

#include <stdbool.h>

typedef bool (*ui_setup_wizard_wifi_connect_cb_t)(const char *ssid, const char *password);

/* Dedicated first-run flow. It shares the saved configuration/controllers with
 * Settings but never opens the normal Network, Printer Profile, or Camera UI. */
void ui_setup_wizard_show(ui_setup_wizard_wifi_connect_cb_t wifi_connect_cb);
void ui_setup_wizard_close(void);
