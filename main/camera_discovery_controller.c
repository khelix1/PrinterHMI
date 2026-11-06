#include "camera_discovery_controller.h"
#ifdef PRINTERHMI_DEMO_RUNTIME
#include "demo_mode.h"
#include "demo_camera.h"
#endif

#include "moonraker_config_controller.h"
#include "camera_catalog_controller.h"

#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/task.h"

#include "esp_heap_caps.h"

#include <string.h>
#include "async_result_slot.h"

#define CAMERA_DISCOVERY_TASK_STACK_SIZE 12288

static async_result_slot_t s_result;
static char s_host[MOONRAKER_CONFIG_HOST_LENGTH];
static int s_port = 0;
static char s_api_key[MOONRAKER_CONFIG_API_KEY_LENGTH];
static moonraker_webcam_t s_webcams[CAMERA_CATALOG_MAX_CAMERAS];
static int s_profile;
static uint32_t s_generation;
static bool s_found = false;
static size_t s_count = 0;


static void camera_discovery_task(void *arg)
{
    (void)arg;

    moonraker_webcam_t webcams[CAMERA_CATALOG_MAX_CAMERAS] = {0};
    size_t count = moonraker_probe_webcams_with_api_key(
        s_host, s_port, s_api_key, webcams, CAMERA_CATALOG_MAX_CAMERAS);
    for (size_t i = 0; i < sizeof(s_api_key); ++i)
        ((volatile char *)s_api_key)[i] = 0;
    memcpy(s_webcams, webcams, sizeof(s_webcams));
    s_count = count;
    s_found = count > 0;
    async_result_publish(&s_result);
    vTaskDelete(NULL);
}


bool camera_discovery_start(const char *host, int port, const char *api_key)
{
    return camera_discovery_start_for_profile(moonraker_config_active_profile_index(),
                                               host, port, api_key);
}

bool camera_discovery_start_for_profile(int profile, const char *host, int port,
                                        const char *api_key)
{
#ifdef PRINTERHMI_DEMO_RUNTIME
    if (demo_mode_enabled()) return demo_camera_discovery_start(profile, host, port);
#endif
    if (profile < 0 || profile >= MOONRAKER_CONFIG_MAX_PROFILES ||
        !host || !host[0] || strlen(host) >= sizeof(s_host) ||
        (api_key && strlen(api_key) >= sizeof(s_api_key)) ||
        port <= 0 || port >= 65536 || !async_result_start(&s_result)) {
        return false;
    }

    strlcpy(s_host, host, sizeof(s_host));
    strlcpy(s_api_key, api_key ? api_key : "", sizeof(s_api_key));
    s_port = port;
    s_profile = profile;
    s_generation = moonraker_config_generation();
    memset(s_webcams, 0, sizeof(s_webcams));
    s_found = false;
    s_count = 0;


    if (xTaskCreatePinnedToCoreWithCaps(
            camera_discovery_task,
            "camera_find",
            CAMERA_DISCOVERY_TASK_STACK_SIZE,
            NULL,
            4,
            NULL,
            tskNO_AFFINITY,
            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) != pdPASS) {
        for (size_t i = 0; i < sizeof(s_api_key); ++i)
            ((volatile char *)s_api_key)[i] = 0;
        async_result_release(&s_result);
        return false;
    }
    return true;
}


bool camera_discovery_busy(void)
{
#ifdef PRINTERHMI_DEMO_RUNTIME
    if (demo_mode_enabled()) return false;
#endif
    return async_result_busy(&s_result);
}


bool camera_discovery_take_result(moonraker_webcam_t *webcam, bool *found,
                                  size_t *count)
{
#ifdef PRINTERHMI_DEMO_RUNTIME
    if (demo_mode_enabled()) return demo_camera_discovery_take(webcam, found, count);
#endif
    if (!async_result_take(&s_result)) {
        return false;
    }
    if (s_generation != moonraker_config_generation()) {
        async_result_release(&s_result);
        return false;
    }
    for (size_t index = 0; index < s_count; ++index) {
        (void)camera_catalog_set(s_profile, index, s_webcams[index].name,
                                 s_webcams[index].stream_url);
    }
    if (webcam) {
        *webcam = s_count ? s_webcams[0] : (moonraker_webcam_t){0};
    }
    if (found) {
        *found = s_found;
    }
    if (count) {
        *count = s_count;
    }
    async_result_release(&s_result);
    return true;
}
