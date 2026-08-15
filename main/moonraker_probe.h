#pragma once

#include <stdbool.h>
#include <stddef.h>

#define MOONRAKER_PROBE_IDENTITY_LENGTH 64

typedef struct {
    bool reachable;
    bool klippy_ready;
    char identity[MOONRAKER_PROBE_IDENTITY_LENGTH];
} moonraker_probe_result_t;

/*
 * Tests every requested TCP port in one bounded select() wait.  It is used
 * only by discovery; Moonraker identity and readiness remain HTTP-verified.
 */
size_t moonraker_probe_open_ports(
    const char *host,
    const int *ports,
    size_t port_count,
    int timeout_ms,
    bool *open_ports,
    size_t open_ports_count);


/*
 * Verifies one endpoint with /server/info. A successful endpoint then gets a
 * best-effort /printer/info identity lookup. Failure of that optional lookup
 * never converts a verified Moonraker endpoint into a failed result.
 */
bool moonraker_probe_endpoint(
    const char *host,
    int port,
    moonraker_probe_result_t *result);

/* api_key is optional. Discovery always calls the unauthenticated form. */
bool moonraker_probe_endpoint_with_api_key(
    const char *host,
    int port,
    const char *api_key,
    moonraker_probe_result_t *result);

bool moonraker_probe_host(const char *host, int port);

#define MOONRAKER_WEBCAM_NAME_LENGTH 64
#define MOONRAKER_WEBCAM_URL_LENGTH 192

typedef struct {
    char name[MOONRAKER_WEBCAM_NAME_LENGTH];
    char stream_url[MOONRAKER_WEBCAM_URL_LENGTH];
    char snapshot_url[MOONRAKER_WEBCAM_URL_LENGTH];
} moonraker_webcam_t;

/* Gets the first configured Moonraker webcam without changing any profile. */
bool moonraker_probe_first_webcam_with_api_key(
    const char *host,
    int port,
    const char *api_key,
    moonraker_webcam_t *webcam);
