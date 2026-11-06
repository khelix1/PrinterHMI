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
static bool fetch_ok = true, lane_available = true;
static const char *expected_key = "owner-key";
static unsigned acquired, released;
#define pdMS_TO_TICKS(ms) (ms)
static bool network_activity_controller_acquire_exclusive(unsigned timeout)
{assert(timeout==5000);if(!lane_available)return false;acquired++;return true;}
static void network_activity_controller_release_exclusive(void){released++;}
static void vTaskDelay(unsigned ticks){assert(ticks==250);}

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
    (void)host; (void)port; assert(expected_key ? key && !strcmp(key,expected_key) : !key); (void)path; (void)timeout; (void)exclusive;
    assert(capacity == 512 * 1024);
    memset(buf, 0x42, response_size); *size = response_size; *code = 200; *error = 0;
    return fetch_ok;
}
/* PRODUCTION_DOWNLOADER */
/* PRODUCTION_WRAPPERS */
int main(void)
{
    uint8_t *buf; size_t len;
    assert(moonraker_fetch_thumbnail_encoded_internal("printer", 7125, "owner-key", "a.png", &buf, &len, false));
    assert(len == response_size && retained == len && live == 1 && buf[0] == 0x42);
    heap_caps_free(buf);
    fail_realloc = true;
    assert(moonraker_fetch_thumbnail_encoded_internal("printer", 7125, "owner-key", "a.png", &buf, &len, true));
    assert(len == response_size && retained == 512 * 1024 && live == 1);
    heap_caps_free(buf); fail_realloc = false;
    fetch_ok = false;
    assert(!moonraker_fetch_thumbnail_encoded_internal("printer", 7125, "owner-key", "a.png", &buf, &len, false));
    assert(!buf && !len && !live);
    fetch_ok = true; response_size = 8;
    assert(!moonraker_fetch_thumbnail_encoded_internal("printer", 7125, "owner-key", "a.png", &buf, &len, false));
    assert(!buf && !len && !live);
    response_size = 512 * 1024;
    assert(moonraker_fetch_thumbnail_encoded_internal("printer", 7125, "owner-key", "a.png", &buf, &len, false));
    assert(retained == response_size); heap_caps_free(buf);
    char large[896]; memset(large, 'a', sizeof(large)); large[511] = 0;
    assert(moonraker_fetch_thumbnail_encoded_internal("printer", 7125, "owner-key", large, &buf, &len, false));
    heap_caps_free(buf);
    large[511] = 'a'; large[sizeof(large)-1] = 0;
    assert(!moonraker_fetch_thumbnail_encoded_internal("printer", 7125, "owner-key", large, &buf, &len, false));
    assert(!live && !buf && !len);
    response_size=1234;
    assert(moonraker_fetch_thumbnail_encoded_with_api_key("printer",7125,"owner-key","a.png",&buf,&len));
    heap_caps_free(buf);
    assert(moonraker_fetch_thumbnail_encoded_quiet_with_api_key("printer",7125,"owner-key","a.png",&buf,&len));
    heap_caps_free(buf);assert(acquired==1&&released==1);
    lane_available=false;buf=(uint8_t *)1;len=99;
    assert(!moonraker_fetch_thumbnail_encoded_quiet_with_api_key("printer",7125,"owner-key","a.png",&buf,&len));
    assert(!buf&&!len&&!live&&acquired==released);
    assert(!moonraker_fetch_thumbnail_encoded_quiet("printer",7125,"a.png",&buf,&len)&&!buf&&!len);
    lane_available=true;expected_key=NULL;
    assert(moonraker_fetch_thumbnail_encoded("printer",7125,"a.png",&buf,&len));heap_caps_free(buf);
    assert(moonraker_fetch_thumbnail_encoded_quiet("printer",7125,"a.png",&buf,&len));heap_caps_free(buf);
    assert(acquired==released&&!live);
    puts("PASS: authenticated/anonymous thumbnail forwarding, quiet busy-output clearing, balanced exclusive lane, compact buffers and failure cleanup");
}
