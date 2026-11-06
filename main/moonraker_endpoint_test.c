#include "moonraker_endpoint_test.h"

#include "moonraker_config_controller.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>
#include "async_result_slot.h"

static async_result_slot_t s_slot;
static char s_host[MOONRAKER_CONFIG_HOST_LENGTH];
static int s_port = 0;
static char s_api_key[MOONRAKER_CONFIG_API_KEY_LENGTH];
static moonraker_probe_result_t s_result;
static bool s_verified = false;


static void endpoint_test_task(void *arg)
{
    (void)arg;

    moonraker_probe_result_t result = {0};
    bool verified = moonraker_probe_endpoint_with_api_key(
        s_host, s_port, s_api_key, &result);

    for (size_t i = 0; i < sizeof(s_api_key); ++i)
        ((volatile char *)s_api_key)[i] = 0;
    s_result = result;
    s_verified = verified;
    async_result_publish(&s_slot);

    vTaskDelete(NULL);
}


bool moonraker_endpoint_test_start(
    const char *host, int port, const char *api_key)
{
    if (!host || !host[0] || strnlen(host, sizeof(s_host)) >= sizeof(s_host) ||
        (api_key && strnlen(api_key, sizeof(s_api_key)) >= sizeof(s_api_key)) ||
        port <= 0 || port >= 65536 || !async_result_start(&s_slot)) {
        return false;
    }

    strlcpy(s_host, host, sizeof(s_host));
    strlcpy(s_api_key, api_key ? api_key : "", sizeof(s_api_key));
    s_port = port;
    memset(&s_result, 0, sizeof(s_result));
    s_verified = false;

    if (xTaskCreate(
            endpoint_test_task,
            "moon_test",
            4096,
            NULL,
            4,
            NULL) != pdPASS) {
        for (size_t i = 0; i < sizeof(s_api_key); ++i)
            ((volatile char *)s_api_key)[i] = 0;
        async_result_release(&s_slot);
        return false;
    }

    return true;
}


bool moonraker_endpoint_test_busy(void)
{
    return async_result_busy(&s_slot);
}


bool moonraker_endpoint_test_take_result(
    moonraker_probe_result_t *result,
    bool *verified)
{
    if (!async_result_take(&s_slot)) {
        return false;
    }

    if (result) {
        *result = s_result;
    }
    if (verified) {
        *verified = s_verified;
    }
    async_result_release(&s_slot);
    return true;
}
