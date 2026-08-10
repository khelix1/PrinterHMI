#pragma once
#include <stdbool.h>
#include <stddef.h>
#include "moonraker_config_controller.h"

#define MOONRAKER_TLS_CA_PEM_MAX 4096

bool moonraker_transport_security_controller_init(void);
bool moonraker_transport_security_import_ca_file_for_profile(int profile_index, const char *path);
const char *moonraker_transport_security_ca_pem_for_profile(int profile_index);
bool moonraker_transport_security_profile_secure(const moonraker_profile_t *profile);
bool moonraker_transport_security_build_http_url(const moonraker_profile_t *profile, const char *path, char *out, size_t out_size);
bool moonraker_transport_security_build_websocket_uri(const moonraker_profile_t *profile, char *out, size_t out_size);
bool moonraker_transport_security_build_http_url_for_endpoint(const char *host, int port, const char *path, char *out, size_t out_size, const char **out_ca_pem);
bool moonraker_transport_security_build_websocket_uri_for_endpoint(const char *host, int port, char *out, size_t out_size, const char **out_ca_pem);
