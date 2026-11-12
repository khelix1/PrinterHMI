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
static moonraker_profile_t s_profile_owner;
static camera_discovery_status_t s_status;

static bool profile_owner_current(void)
{
    const moonraker_profile_t *p = moonraker_config_profile(s_profile);
    return p && p->configured == s_profile_owner.configured &&
        p->port == s_profile_owner.port && p->secure_transport == s_profile_owner.secure_transport &&
        !strcmp(p->name, s_profile_owner.name) && !strcmp(p->host, s_profile_owner.host) &&
        !strcmp(p->api_key, s_profile_owner.api_key) &&
        !strcmp(p->camera_stream_url, s_profile_owner.camera_stream_url);
}

static void release_result(void)
{
    for (size_t i = 0; i < sizeof(s_profile_owner); ++i)
        ((volatile unsigned char *)&s_profile_owner)[i] = 0;
    for (size_t i = 0; i < sizeof(s_api_key); ++i)
        ((volatile char *)s_api_key)[i] = 0;
    async_result_release(&s_result);
}

camera_discovery_status_t camera_discovery_status(void)
{
    return s_status;
}


static void camera_discovery_task(void *arg)
{
    (void)arg;

    moonraker_webcam_t webcams[CAMERA_CATALOG_MAX_CAMERAS] = {0};
    size_t count = 0;
    if (s_generation == moonraker_config_generation())
        count = moonraker_probe_webcams_with_api_key(
            s_host, s_port, s_api_key, webcams, CAMERA_CATALOG_MAX_CAMERAS);
    if (count > CAMERA_CATALOG_MAX_CAMERAS) count = 0;
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
    if (demo_mode_enabled()) {
        bool started = demo_camera_discovery_start(profile, host, port);
        if (started) s_status = CAMERA_DISCOVERY_WAITING;
        return started;
    }
#endif
    if (profile < 0 || profile >= MOONRAKER_CONFIG_MAX_PROFILES ||
        !host || !host[0] || strnlen(host, sizeof(s_host)) >= sizeof(s_host) ||
        (api_key && strnlen(api_key, sizeof(s_api_key)) >= sizeof(s_api_key)) ||
        port <= 0 || port >= 65536 || !async_result_start(&s_result)) {
        return false;
    }

    strlcpy(s_host, host, sizeof(s_host));
    strlcpy(s_api_key, api_key ? api_key : "", sizeof(s_api_key));
    s_port = port;
    s_profile = profile;
    const moonraker_profile_t *owner = moonraker_config_profile(profile);
    if (!owner) { release_result();return false; }
    s_profile_owner = *owner;
    s_status = CAMERA_DISCOVERY_WAITING;
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
        release_result();
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
    if (demo_mode_enabled()) {
        moonraker_webcam_t value = {0};bool detected = false;size_t n = 0;
        bool received = demo_camera_discovery_take(&value, &detected, &n);
        if (!received) return false;
        s_status = detected ? CAMERA_DISCOVERY_READY : CAMERA_DISCOVERY_SAVE_FAILED;
        if (webcam) *webcam = value;
        if (found) *found = detected;
        if (count) *count = n;
        return true;
    }
#endif
    if (!async_result_take(&s_result)) {
        return false;
    }
    if (s_generation != moonraker_config_generation() || !profile_owner_current()) {
        s_status = CAMERA_DISCOVERY_STALE;
        release_result();
        return false;
    }
    s_status = CAMERA_DISCOVERY_READY;
    for (size_t index = 0; index < s_count; ++index) {
        if (!camera_catalog_set(s_profile, index, s_webcams[index].name,
                                 s_webcams[index].stream_url)) s_status = CAMERA_DISCOVERY_SAVE_FAILED;
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
    release_result();
    return true;
}
