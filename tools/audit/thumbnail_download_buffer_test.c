/* Harness prepended to the production downloader by thumbnail_storage_test.py. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
#define ESP_FAIL -1
typedef int esp_err_t;
static size_t retained;
static unsigned live;
static bool fail_realloc;
static bool fetch_ok = true;
static size_t response_size = 1234;
static void *heap_caps_malloc(size_t n, unsigned caps)
{
    assert(caps == (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    void *p = malloc(n); if (p) { retained = n; ++live; } return p;
}
static void heap_caps_free(void *p)
{
    if (p) { assert(live); --live; retained = 0; free(p); }
}
static void *heap_caps_realloc(void *p, size_t n, unsigned caps)
{
    assert(caps == (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (fail_realloc) return NULL;
    void *out = realloc(p, n); if (out) retained = n; return out;
}
static bool moonraker_http_get_raw(const char *host, int port, const char *key,
    const char *path, int timeout, uint8_t *buf, size_t capacity, size_t *size,
    int *code, esp_err_t *error, bool exclusive)
{
    (void)host; (void)port; (void)key; (void)path; (void)timeout; (void)exclusive;
    assert(capacity == 512 * 1024);
    memset(buf, 0x42, response_size); *size = response_size; *code = 200; *error = 0;
    return fetch_ok;
}
/* PRODUCTION_DOWNLOADER */
int main(void)
{
    uint8_t *buf; size_t len;
    assert(moonraker_fetch_thumbnail_encoded_internal("printer", 7125, "a.png", &buf, &len, false));
    assert(len == response_size && retained == len && live == 1 && buf[0] == 0x42);
    heap_caps_free(buf);
    fail_realloc = true;
    assert(moonraker_fetch_thumbnail_encoded_internal("printer", 7125, "a.png", &buf, &len, true));
    assert(len == response_size && retained == 512 * 1024 && live == 1);
    heap_caps_free(buf); fail_realloc = false;
    fetch_ok = false;
    assert(!moonraker_fetch_thumbnail_encoded_internal("printer", 7125, "a.png", &buf, &len, false));
    assert(!buf && !len && !live);
    fetch_ok = true; response_size = 8;
    assert(!moonraker_fetch_thumbnail_encoded_internal("printer", 7125, "a.png", &buf, &len, false));
    assert(!buf && !len && !live);
    response_size = 512 * 1024;
    assert(moonraker_fetch_thumbnail_encoded_internal("printer", 7125, "a.png", &buf, &len, false));
    assert(retained == response_size); heap_caps_free(buf);
    char large[512]; memset(large, 'a', sizeof(large)); large[511] = 0;
    assert(!moonraker_fetch_thumbnail_encoded_internal("printer", 7125, large, &buf, &len, false));
    assert(!live && !buf && !len);
    puts("PASS: actual downloader trims success, preserves realloc fallback and frees every failure");
}
