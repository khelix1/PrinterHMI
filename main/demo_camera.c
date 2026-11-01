#include "demo_camera.h"
#include "camera_catalog_controller.h"
#include "moonraker_config_controller.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include <string.h>

static uint16_t *sample;
static bool streaming, test_pending, test_ok;
static int64_t last_frame;
static bool sample_url(const char *url)
{
    return url && (!strcmp(url, DEMO_CAMERA_FRONT_URL) || !strcmp(url, DEMO_CAMERA_SIDE_URL));
}
bool demo_camera_seed(void)
{
    return camera_catalog_set(0, 0, "Front camera", DEMO_CAMERA_FRONT_URL) &&
           camera_catalog_set(0, 1, "Side camera", DEMO_CAMERA_SIDE_URL) &&
           camera_catalog_set(1, 0, "Front camera", DEMO_CAMERA_FRONT_URL);
}
bool demo_camera_stream_start(const char *url)
{
    if (streaming || !sample_url(url)) return false;
    size_t size = DEMO_CAMERA_WIDTH * DEMO_CAMERA_HEIGHT * sizeof(uint16_t);
    sample = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!sample) return false;
    demo_camera_draw(sample, !strcmp(url, DEMO_CAMERA_SIDE_URL));
    last_frame = 0;
    streaming = true;
    return true;
}
bool demo_camera_stream_busy(void) { return streaming; }
void demo_camera_stream_stop(void)
{
    streaming = false;
    heap_caps_free(sample);
    sample = NULL;
    last_frame = 0;
}
bool demo_camera_stream_take(uint8_t **pixels, size_t *size, int *width, int *height, bool *ok)
{
    if (!streaming || !sample) return false;
    int64_t now = esp_timer_get_time();
    if (last_frame && now - last_frame < 1000000) return false;
    size_t length = DEMO_CAMERA_WIDTH * DEMO_CAMERA_HEIGHT * sizeof(uint16_t);
    uint8_t *copy = heap_caps_malloc(length, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!copy) return false;
    memcpy(copy, sample, length);
    last_frame = now ? now : 1;
    if (pixels) *pixels = copy; else heap_caps_free(copy);
    if (size) *size = length;
    if (width) *width = DEMO_CAMERA_WIDTH;
    if (height) *height = DEMO_CAMERA_HEIGHT;
    if (ok) *ok = true;
    return true;
}
bool demo_camera_test_start(const char *url)
{
    if (!url || !*url || test_pending) return false;
    test_pending = true;
    test_ok = sample_url(url);
    return true;
}
bool demo_camera_test_take(bool *ok, int *width, int *height, size_t *bytes)
{
    if (!test_pending) return false;
    test_pending = false;
    if (ok) *ok = test_ok;
    if (width) *width = test_ok ? DEMO_CAMERA_WIDTH : 0;
    if (height) *height = test_ok ? DEMO_CAMERA_HEIGHT : 0;
    if (bytes) *bytes = test_ok ? DEMO_CAMERA_WIDTH * DEMO_CAMERA_HEIGHT * sizeof(uint16_t) : 0;
    return true;
}

static bool discovery_pending;
static int discovery_profile;
static uint32_t discovery_generation;
bool demo_camera_discovery_start(int profile, const char *host, int port)
{
    if (discovery_pending || profile < 0 || profile >= 2 || !host || !*host || port <= 0 || port >= 65536) return false;
    discovery_profile = profile;
    discovery_generation = moonraker_config_generation();
    discovery_pending = true;
    return true;
}
bool demo_camera_discovery_take(moonraker_webcam_t *webcam, bool *found, size_t *count)
{
    if (!discovery_pending) return false;
    discovery_pending = false;
    if (discovery_generation != moonraker_config_generation()) return false;
    bool saved = camera_catalog_set(discovery_profile, 0, "Front camera", DEMO_CAMERA_FRONT_URL) &&
                 camera_catalog_set(discovery_profile, 1, "Side camera", DEMO_CAMERA_SIDE_URL);
    if (webcam) {
        memset(webcam, 0, sizeof(*webcam));
        if (saved) {
            strlcpy(webcam->name, "Front camera", sizeof(webcam->name));
            strlcpy(webcam->stream_url, DEMO_CAMERA_FRONT_URL, sizeof(webcam->stream_url));
        }
    }
    if (found) *found = saved;
    if (count) *count = saved ? 2 : 0;
    return true;
}
