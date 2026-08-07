#include "moonraker_endpoint_test.h"

#include "moonraker_config_controller.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>

static TaskHandle_t s_task = NULL;
static char s_host[MOONRAKER_CONFIG_HOST_LENGTH];
static int s_port = 0;
static moonraker_probe_result_t s_result;
static bool s_verified = false;
static volatile bool s_result_ready = false;


static void endpoint_test_task(void *arg)
{
    (void)arg;

    moonraker_probe_result_t result = {0};
    bool verified = moonraker_probe_endpoint(s_host, s_port, &result);

    s_result = result;
    s_verified = verified;
    __atomic_store_n(&s_result_ready, true, __ATOMIC_RELEASE);
    s_task = NULL;

    vTaskDelete(NULL);
}


bool moonraker_endpoint_test_start(const char *host, int port)
{
    if (!host || !host[0] || port <= 0 || port >= 65536 || s_task) {
        return false;
    }

    strlcpy(s_host, host, sizeof(s_host));
    s_port = port;
    memset(&s_result, 0, sizeof(s_result));
    s_verified = false;
    __atomic_store_n(&s_result_ready, false, __ATOMIC_RELEASE);

    if (xTaskCreate(
            endpoint_test_task,
            "moon_test",
            4096,
            NULL,
            4,
            &s_task) != pdPASS) {
        s_task = NULL;
        return false;
    }

    return true;
}


bool moonraker_endpoint_test_busy(void)
{
    return s_task != NULL;
}


bool moonraker_endpoint_test_take_result(
    moonraker_probe_result_t *result,
    bool *verified)
{
    if (!__atomic_exchange_n(
            &s_result_ready,
            false,
            __ATOMIC_ACQ_REL)) {
        return false;
    }

    if (result) {
        *result = s_result;
    }
    if (verified) {
        *verified = s_verified;
    }
    return true;
}
