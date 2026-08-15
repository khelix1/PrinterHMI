#include "camera_discovery_controller.h"

#include "moonraker_config_controller.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>

static TaskHandle_t s_task = NULL;
static char s_host[MOONRAKER_CONFIG_HOST_LENGTH];
static int s_port = 0;
static char s_api_key[MOONRAKER_CONFIG_API_KEY_LENGTH];
static moonraker_webcam_t s_webcam;
static bool s_found = false;
static volatile bool s_result_ready = false;


static void camera_discovery_task(void *arg)
{
    (void)arg;

    moonraker_webcam_t webcam = {0};
    bool found = moonraker_probe_first_webcam_with_api_key(
        s_host, s_port, s_api_key, &webcam);

    s_webcam = webcam;
    s_found = found;
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
    __atomic_store_n(&s_result_ready, false, __ATOMIC_RELEASE);

    if (xTaskCreate(
            camera_discovery_task,
            "camera_find",
            4096,
            NULL,
            4,
            &s_task) != pdPASS) {
        s_task = NULL;
        return false;
    }
    return true;
}


bool camera_discovery_busy(void)
{
    return s_task != NULL;
}


bool camera_discovery_take_result(moonraker_webcam_t *webcam, bool *found)
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
    return true;
}
