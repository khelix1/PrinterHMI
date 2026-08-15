#include "camera_stream_controller.h"
#include "camera_jpeg_decoder.h"

#include "network_activity_controller.h"

#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"

#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include <string.h>

#define CAMERA_STREAM_FRAME_CAPACITY (256 * 1024)

typedef struct {
    uint8_t *pixels;
    size_t pixel_size;
    int width;
    int height;
    bool ok;
} camera_frame_result_t;

static TaskHandle_t s_task = NULL;
static char s_url[192];
static const char *s_ca_pem = NULL;
static QueueHandle_t s_result_queue = NULL;
static volatile bool s_stop_requested = false;


static bool camera_stream_open(esp_http_client_handle_t *client_out)
{
    if (!client_out || !network_activity_controller_acquire_shared(3000)) {
        return false;
    }

    esp_http_client_config_t config = {
        .url = s_url,
        .cert_pem = s_ca_pem,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 2500,
        .buffer_size = 2048,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client || esp_http_client_open(client, 0) != ESP_OK) {
        if (client) esp_http_client_cleanup(client);
        network_activity_controller_end_shared();
        return false;
    }
    (void)esp_http_client_fetch_headers(client);
    *client_out = client;
    return true;
}



static void camera_stream_task(void *arg)
{
    (void)arg;
    uint8_t *jpeg = heap_caps_malloc(
        CAMERA_STREAM_FRAME_CAPACITY, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!jpeg) jpeg = heap_caps_malloc(CAMERA_STREAM_FRAME_CAPACITY, MALLOC_CAP_8BIT);
    if (!jpeg) {
        camera_frame_result_t failed = {.ok = false};
        (void)xQueueSend(s_result_queue, &failed, 0);
        s_task = NULL;
        vTaskDelete(NULL);
        return;
    }

    while (!s_stop_requested) {
        esp_http_client_handle_t client = NULL;
        if (!camera_stream_open(&client)) {
            camera_frame_result_t failed = {.ok = false};
            (void)xQueueSend(s_result_queue, &failed, 0);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        bool in_frame = false;
        uint8_t previous = 0;
        size_t used = 0;
        char chunk[2048];
        while (!s_stop_requested) {
            int read = esp_http_client_read(client, chunk, sizeof(chunk));
            if (read <= 0) break;
            for (int index = 0; index < read && !s_stop_requested; ++index) {
                uint8_t byte = (uint8_t)chunk[index];
                if (!in_frame) {
                    if (previous == 0xff && byte == 0xd8) {
                        jpeg[0] = 0xff;
                        jpeg[1] = 0xd8;
                        used = 2;
                        in_frame = true;
                    }
                } else if (used < CAMERA_STREAM_FRAME_CAPACITY) {
                    jpeg[used++] = byte;
                    if (previous == 0xff && byte == 0xd9) {
                        uint16_t *pixels = NULL;
                        int width = 0;
                        int height = 0;
                        if (camera_jpeg_decode_rgb565(
                                jpeg, used, &pixels, &width, &height)) {
                            camera_frame_result_t result = {
                                .pixels = (uint8_t *)pixels,
                                .pixel_size = (size_t)width * (size_t)height * sizeof(uint16_t),
                                .width = width,
                                .height = height,
                                .ok = true,
                            };
                            if (xQueueSend(s_result_queue, &result, 0) != pdPASS) {
                                heap_caps_free(pixels);
                            }
                        }
                        in_frame = false;
                        used = 0;
                    }
                } else {
                    in_frame = false;
                    used = 0;
                }
                previous = byte;
            }
        }

        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        network_activity_controller_end_shared();
        if (!s_stop_requested) vTaskDelay(pdMS_TO_TICKS(250));
    }

    heap_caps_free(jpeg);
    s_task = NULL;
    vTaskDelete(NULL);
}



bool camera_stream_start(const char *url)
{
    if (!url || !url[0] || s_task) return false;
    if (!s_result_queue) {
        s_result_queue = xQueueCreate(1, sizeof(camera_frame_result_t));
    }
    if (!s_result_queue) return false;

    camera_frame_result_t stale;
    while (xQueueReceive(s_result_queue, &stale, 0) == pdPASS) {
        if (stale.pixels) heap_caps_free(stale.pixels);
    }
    strlcpy(s_url, url, sizeof(s_url));
    s_stop_requested = false;
    if (xTaskCreatePinnedToCoreWithCaps(
            camera_stream_task, "camera_view", 12288, NULL, 4, &s_task,
            tskNO_AFFINITY, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) != pdPASS) {
        s_task = NULL;
        return false;
    }
    return true;
}



bool camera_stream_busy(void)
{
    return s_task != NULL;
}


bool camera_stream_take_result(
    uint8_t **pixels,
    size_t *pixel_size,
    int *width,
    int *height,
    bool *ok)
{
    camera_frame_result_t result = {0};
    if (!s_result_queue || xQueueReceive(s_result_queue, &result, 0) != pdPASS) {
        return false;
    }
    if (pixels) *pixels = result.pixels;
    else if (result.pixels) heap_caps_free(result.pixels);
    if (pixel_size) *pixel_size = result.pixel_size;
    if (width) *width = result.width;
    if (height) *height = result.height;
    if (ok) *ok = result.ok;
    return true;
}


void camera_stream_stop(void)
{
    s_stop_requested = true;
    if (!s_result_queue) return;
    camera_frame_result_t stale;
    while (xQueueReceive(s_result_queue, &stale, 0) == pdPASS) {
        if (stale.pixels) heap_caps_free(stale.pixels);
    }
}
