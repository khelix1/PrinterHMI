#include "camera_discovery_controller.h"

#include "moonraker_config_controller.h"
#include "camera_catalog_controller.h"

#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/task.h"

#include "esp_heap_caps.h"

#include <string.h>

#define CAMERA_DISCOVERY_TASK_STACK_SIZE 12288

static TaskHandle_t s_task = NULL;
static char s_host[MOONRAKER_CONFIG_HOST_LENGTH];
static int s_port = 0;
static char s_api_key[MOONRAKER_CONFIG_API_KEY_LENGTH];
static moonraker_webcam_t s_webcam;
static bool s_found = false;
static size_t s_count = 0;
static volatile bool s_result_ready = false;


static void camera_discovery_task(void *arg)
{
    (void)arg;

    moonraker_webcam_t webcams[CAMERA_CATALOG_MAX_CAMERAS] = {0};
    size_t count = moonraker_probe_webcams_with_api_key(
        s_host, s_port, s_api_key, webcams, CAMERA_CATALOG_MAX_CAMERAS);
    int profile = moonraker_config_active_profile_index();
    for (size_t index = 0; index < count; ++index) {
        (void)camera_catalog_set(profile, index, webcams[index].name,
                                 webcams[index].stream_url);
    }

    s_webcam = count ? webcams[0] : (moonraker_webcam_t){0};
    s_count = count;
    s_found = count > 0;
    __atomic_store_n(&s_result_ready, true, __ATOMIC_RELEASE);
    s_task = NULL;
    vTaskDelete(NULL);
}


bool camera_discovery_start(const char *host, int port, const char *api_key)
{
    if (!host || !host[0] || port <= 0 || port >= 65536 || s_task) {
        return false;
    }

    strlcpy(s_host, host, sizeof(s_host));
    strlcpy(s_api_key, api_key ? api_key : "", sizeof(s_api_key));
    s_port = port;
    memset(&s_webcam, 0, sizeof(s_webcam));
    s_found = false;
    s_count = 0;
    __atomic_store_n(&s_result_ready, false, __ATOMIC_RELEASE);

    if (xTaskCreatePinnedToCoreWithCaps(
            camera_discovery_task,
            "camera_find",
            CAMERA_DISCOVERY_TASK_STACK_SIZE,
            NULL,
            4,
            &s_task,
            tskNO_AFFINITY,
            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) != pdPASS) {
        s_task = NULL;
        return false;
    }
    return true;
}


bool camera_discovery_busy(void)
{
    return s_task != NULL;
}


bool camera_discovery_take_result(moonraker_webcam_t *webcam, bool *found,
                                  size_t *count)
{
    if (!__atomic_exchange_n(
            &s_result_ready,
            false,
            __ATOMIC_ACQ_REL)) {
        return false;
    }
    if (webcam) {
        *webcam = s_webcam;
    }
    if (found) {
        *found = s_found;
    }
    if (count) {
        *count = s_count;
    }
    return true;
}
