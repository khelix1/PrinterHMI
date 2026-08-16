#include "camera_test_controller.h"

#include "network_activity_controller.h"

#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"

#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/task.h"

#include <string.h>

#define CAMERA_TEST_FRAME_CAPACITY (256 * 1024)

static TaskHandle_t s_task = NULL;
static char s_url[192];
static bool s_ok = false;
static int s_width = 0;
static int s_height = 0;
static size_t s_bytes = 0;
static volatile bool s_ready = false;

static bool jpeg_dimensions(const uint8_t *data, size_t length,
                            int *width, int *height)
{
    if (!data || length < 10 || data[0] != 0xff || data[1] != 0xd8) return false;
    for (size_t pos = 2; pos + 8 < length; ++pos) {
        if (data[pos] != 0xff) continue;
        uint8_t marker = data[pos + 1];
        if (marker == 0xff || marker == 0x00 || marker == 0xd8 || marker == 0xd9) continue;
        size_t segment = ((size_t)data[pos + 2] << 8) | data[pos + 3];
        if (segment < 2 || pos + 2 + segment > length) return false;
        bool sof = (marker >= 0xc0 && marker <= 0xc3) ||
                   (marker >= 0xc5 && marker <= 0xc7) ||
                   (marker >= 0xc9 && marker <= 0xcb) ||
                   (marker >= 0xcd && marker <= 0xcf);
        if (sof) {
            *height = ((int)data[pos + 5] << 8) | data[pos + 6];
            *width = ((int)data[pos + 7] << 8) | data[pos + 8];
            return *width > 0 && *height > 0;
        }
        pos += 1 + segment;
    }
    return false;
}

static bool fetch_frame(int *width, int *height, size_t *bytes)
{
    uint8_t *buffer = heap_caps_malloc(CAMERA_TEST_FRAME_CAPACITY,
                                       MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buffer) buffer = heap_caps_malloc(CAMERA_TEST_FRAME_CAPACITY, MALLOC_CAP_8BIT);
    if (!buffer) return false;

    esp_http_client_config_t config = {
        .url = s_url, .method = HTTP_METHOD_GET, .timeout_ms = 3500, .buffer_size = 2048,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client || !network_activity_controller_acquire_shared(4000)) {
        if (client) esp_http_client_cleanup(client);
        heap_caps_free(buffer);
        return false;
    }

    bool started = false, complete = false;
    uint8_t previous = 0;
    size_t used = 0;
    char chunk[1024];
    if (esp_http_client_open(client, 0) == ESP_OK) {
        (void)esp_http_client_fetch_headers(client);
        while (used < CAMERA_TEST_FRAME_CAPACITY && !complete) {
            int read = esp_http_client_read(client, chunk, sizeof(chunk));
            if (read <= 0) break;
            for (int index = 0; index < read; ++index) {
                uint8_t value = (uint8_t)chunk[index];
                if (!started) {
                    if (previous == 0xff && value == 0xd8) {
                        buffer[0] = 0xff; buffer[1] = 0xd8; used = 2; started = true;
                    }
                } else {
                    buffer[used++] = value;
                    if (previous == 0xff && value == 0xd9) { complete = true; break; }
                }
                previous = value;
            }
        }
    }
    esp_http_client_close(client);
    network_activity_controller_end_shared();
    esp_http_client_cleanup(client);

    bool ok = complete && jpeg_dimensions(buffer, used, width, height);
    if (ok && bytes) *bytes = used;
    heap_caps_free(buffer);
    return ok;
}

static void camera_test_task(void *arg)
{
    (void)arg;
    s_width = s_height = 0;
    s_bytes = 0;
    s_ok = fetch_frame(&s_width, &s_height, &s_bytes);
    __atomic_store_n(&s_ready, true, __ATOMIC_RELEASE);
    s_task = NULL;
    vTaskDelete(NULL);
}

bool camera_test_start(const char *url)
{
    if (!url || !url[0] || s_task) return false;
    strlcpy(s_url, url, sizeof(s_url));
    s_ok = false; s_width = s_height = 0; s_bytes = 0;
    __atomic_store_n(&s_ready, false, __ATOMIC_RELEASE);
    if (xTaskCreatePinnedToCoreWithCaps(camera_test_task, "camera_test", 8192,
                                        NULL, 4, &s_task, tskNO_AFFINITY,
                                        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) != pdPASS) {
        s_task = NULL;
        return false;
    }
    return true;
}

bool camera_test_busy(void) { return s_task != NULL; }

bool camera_test_take_result(bool *ok, int *width, int *height, size_t *bytes)
{
    if (!__atomic_exchange_n(&s_ready, false, __ATOMIC_ACQ_REL)) return false;
    if (ok) *ok = s_ok;
    if (width) *width = s_width;
    if (height) *height = s_height;
    if (bytes) *bytes = s_bytes;
    return true;
}
