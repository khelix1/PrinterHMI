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
 * On success the buffer is NUL-terminated and response_length_out reports
 * the exact byte count excluding the terminator. On failure the count is zero.
 */
bool moonraker_live_transport_fetch(
    const char *host,
    int port,
    const char *api_key,
    char *response_buffer,
    size_t buffer_size,
    size_t *response_length_out,
    int *http_status_out);

#ifdef __cplusplus
}
#endif
