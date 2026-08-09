#pragma once

#include <stdbool.h>
#include <stddef.h>

bool wifi_credentials_store_load(char *ssid, size_t ssid_size,
                                 char *password, size_t password_size);
bool wifi_credentials_store_save(const char *ssid, const char *password);
