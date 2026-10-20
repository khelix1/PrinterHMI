#include "moonraker_transport_security_controller.h"
#include "moonraker_tls_trust_store.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

static int profile_index_for_endpoint(const char *host, int port)
{
    if (!host || !host[0]) return -1;
    int selected = -1;
    int active = moonraker_config_active_profile_index();
    for (int index = 0; index < MOONRAKER_CONFIG_MAX_PROFILES; ++index) {
        const moonraker_profile_t *profile = moonraker_config_profile(index);
        if (!profile || !profile->configured || !profile->host[0] ||
            profile->port != port || strcasecmp(profile->host, host) != 0) continue;
        const moonraker_profile_t *previous = selected >= 0 ? moonraker_config_profile(selected) : NULL;
        /* Endpoint-only callers cannot distinguish duplicate profiles. Never
         * downgrade an endpoint that is also configured to require TLS. */
        if (!previous || (profile->secure_transport && !previous->secure_transport) ||
            (index == active && profile->secure_transport == previous->secure_transport)) {
            selected = index;
        }
    }
    return selected;
}

static bool valid_endpoint(const char *host, int port, const char *path,
                           char *out, size_t out_size)
{
    if (out && out_size) out[0] = '\0';
    if (!out || !out_size || !host || !host[0] || port < 1 || port > 65535 ||
        !path || path[0] != '/') return false;
    size_t length = strnlen(host, MOONRAKER_CONFIG_HOST_LENGTH);
    if (!length || length >= MOONRAKER_CONFIG_HOST_LENGTH) return false;
    for (size_t i = 0; i < length; ++i) {
        unsigned char ch = (unsigned char)host[i];
        bool alnum = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
            (ch >= '0' && ch <= '9');
        if (!alnum && (i == 0 || i == length - 1 || (ch != '.' && ch != '-' && ch != '_'))) return false;
    }
    for (const unsigned char *p = (const unsigned char *)path; *p; ++p) {
        if (*p <= 0x20 || *p == 0x7f || *p == '#') return false;
    }
    return true;
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
    if (!profile || !profile->configured ||
        !valid_endpoint(profile->host, profile->port, path, out, out_size)) return false;
    if (profile->secure_transport && !moonraker_tls_trust_store_pem_for_profile(index)) return false;
    int written = snprintf(out, out_size, "%s://%s:%d%s", profile->secure_transport ? "https" : "http", profile->host, profile->port, path);
    return written > 0 && (size_t)written < out_size;
}

bool moonraker_transport_security_build_websocket_uri(const moonraker_profile_t *profile,
                                                       char *out, size_t out_size)
{
    int index = profile_index_for_profile(profile);
    if (!profile || !profile->configured ||
        !valid_endpoint(profile->host, profile->port, "/websocket", out, out_size)) return false;
    if (profile->secure_transport && !moonraker_tls_trust_store_pem_for_profile(index)) return false;
    int written = snprintf(out, out_size, "%s://%s:%d/websocket", profile->secure_transport ? "wss" : "ws", profile->host, profile->port);
    return written > 0 && (size_t)written < out_size;
}

bool moonraker_transport_security_build_http_url_for_endpoint(const char *host, int port,
    const char *path, char *out, size_t out_size, const char **out_ca_pem)
{
    if (out_ca_pem) *out_ca_pem = NULL;
    if (!valid_endpoint(host, port, path, out, out_size)) return false;
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
    if (out_ca_pem) *out_ca_pem = NULL;
    if (!valid_endpoint(host, port, "/websocket", out, out_size)) return false;
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
