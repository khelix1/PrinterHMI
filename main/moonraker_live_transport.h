#pragma once

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Fetch the Moonraker live-object query response into caller-owned storage.
 *
 * Returns true only when:
 *   - the HTTP client is created,
 *   - the request completes successfully,
 *   - Moonraker returns HTTP 200,
 *   - the response fits in the supplied buffer.
 *
 * The response is always NUL-terminated when buffer_size is nonzero.
 */
bool moonraker_live_transport_fetch(
    const char *host,
    int port,
    const char *api_key,
    char *response_buffer,
    size_t buffer_size,
    int *http_status_out);

#ifdef __cplusplus
}
#endif
