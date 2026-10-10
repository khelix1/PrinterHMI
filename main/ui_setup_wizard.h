#pragma once

#include <stdbool.h>

typedef bool (*ui_setup_wizard_wifi_connect_cb_t)(const char *ssid, const char *password);

/* First-run flow shares saved configuration/controllers with Settings.
 * Printer setup opens the shared profile editor; Wi-Fi/camera flows stay here. */
void ui_setup_wizard_show(ui_setup_wizard_wifi_connect_cb_t wifi_connect_cb);
void ui_setup_wizard_close(void);
