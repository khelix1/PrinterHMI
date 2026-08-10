#include "moonraker_transport_security_controller.h"
#include "moonraker_tls_trust_store.h"

#include <stdio.h>
#include <string.h>

static int profile_index_for_endpoint(const char *host, int port)
{
    if (!host || !host[0]) return -1;
    for (int index = 0; index < MOONRAKER_CONFIG_MAX_PROFILES; ++index) {
        const moonraker_profile_t *profile = moonraker_config_profile(index);
        if (profile && profile->host[0] && profile->port == port &&
            strcmp(profile->host, host) == 0) return index;
    }
    return -1;
}

static int profile_index_for_profile(const moonraker_profile_t *wanted)
{
    for (int index = 0; index < MOONRAKER_CONFIG_MAX_PROFILES; ++index) {
        if (moonraker_config_profile(index) == wanted) return index;
    }
    return -1;
}

bool moonraker_transport_security_controller_init(void)
{
    return moonraker_tls_trust_store_init();
}

bool moonraker_transport_security_import_ca_file_for_profile(int profile_index, const char *path)
{
    return moonraker_tls_trust_store_import_file_for_profile(profile_index, path);
}

const char *moonraker_transport_security_ca_pem_for_profile(int profile_index)
{
    return moonraker_tls_trust_store_pem_for_profile(profile_index);
}

bool moonraker_transport_security_profile_secure(const moonraker_profile_t *profile)
{
    return profile && profile->secure_transport;
}

bool moonraker_transport_security_build_http_url(const moonraker_profile_t *profile,
                                                  const char *path, char *out, size_t out_size)
{
    int index = profile_index_for_profile(profile);
    if (!profile || !profile->configured || !path || path[0] != '/' || !out || !out_size) return false;
    if (profile->secure_transport && !moonraker_tls_trust_store_pem_for_profile(index)) return false;
    int written = snprintf(out, out_size, "%s://%s:%d%s", profile->secure_transport ? "https" : "http", profile->host, profile->port, path);
    return written > 0 && (size_t)written < out_size;
}

bool moonraker_transport_security_build_websocket_uri(const moonraker_profile_t *profile,
                                                       char *out, size_t out_size)
{
    int index = profile_index_for_profile(profile);
    if (!profile || !profile->configured || !out || !out_size) return false;
    if (profile->secure_transport && !moonraker_tls_trust_store_pem_for_profile(index)) return false;
    int written = snprintf(out, out_size, "%s://%s:%d/websocket", profile->secure_transport ? "wss" : "ws", profile->host, profile->port);
    return written > 0 && (size_t)written < out_size;
}

bool moonraker_transport_security_build_http_url_for_endpoint(const char *host, int port,
    const char *path, char *out, size_t out_size, const char **out_ca_pem)
{
    int index = profile_index_for_endpoint(host, port);
    const moonraker_profile_t *profile = index >= 0 ? moonraker_config_profile(index) : NULL;
    if (out_ca_pem) *out_ca_pem = NULL;
    if (profile && profile->secure_transport) {
        const char *pem = moonraker_tls_trust_store_pem_for_profile(index);
        if (!pem) return false;
        if (out_ca_pem) *out_ca_pem = pem;
        return moonraker_transport_security_build_http_url(profile, path, out, out_size);
    }
    int written = snprintf(out, out_size, "http://%s:%d%s", host, port, path);
    return written > 0 && (size_t)written < out_size;
}

bool moonraker_transport_security_build_websocket_uri_for_endpoint(const char *host, int port,
    char *out, size_t out_size, const char **out_ca_pem)
{
    int index = profile_index_for_endpoint(host, port);
    const moonraker_profile_t *profile = index >= 0 ? moonraker_config_profile(index) : NULL;
    if (out_ca_pem) *out_ca_pem = NULL;
    if (profile && profile->secure_transport) {
        const char *pem = moonraker_tls_trust_store_pem_for_profile(index);
        if (!pem) return false;
        if (out_ca_pem) *out_ca_pem = pem;
        return moonraker_transport_security_build_websocket_uri(profile, out, out_size);
    }
    int written = snprintf(out, out_size, "ws://%s:%d/websocket", host, port);
    return written > 0 && (size_t)written < out_size;
}
