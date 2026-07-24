#pragma once

#include <stdbool.h>
#include <stddef.h>

const char *ota_manager_get_url(void);
size_t ota_manager_url_capacity(void);
void ota_manager_set_url(const char *url);

bool ota_manager_start(const char *url);
bool ota_manager_is_running(void);
void ota_manager_pump_ui(void);
