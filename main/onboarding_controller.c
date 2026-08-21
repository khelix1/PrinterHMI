#include "onboarding_controller.h"

#include "esp_err.h"
#include "nvs.h"

#define ONBOARDING_NVS_NAMESPACE "onboard"
#define ONBOARDING_NVS_KEY "complete"

static bool s_initialized = false;
static bool s_complete = false;

void onboarding_controller_init(void)
{
    if (s_initialized) {
        return;
    }

    s_initialized = true;
    nvs_handle_t handle;
    uint8_t complete = 0;
    if (nvs_open(ONBOARDING_NVS_NAMESPACE, NVS_READONLY, &handle) == ESP_OK) {
        (void)nvs_get_u8(handle, ONBOARDING_NVS_KEY, &complete);
        nvs_close(handle);
    }
    s_complete = complete == 1U;
}

bool onboarding_controller_should_show(void)
{
    onboarding_controller_init();
    return !s_complete;
}

bool onboarding_controller_mark_complete(void)
{
    onboarding_controller_init();
    nvs_handle_t handle;
    if (nvs_open(ONBOARDING_NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) {
        return false;
    }

    const esp_err_t error = nvs_set_u8(handle, ONBOARDING_NVS_KEY, 1U);
    const esp_err_t commit_error = error == ESP_OK ? nvs_commit(handle) : error;
    nvs_close(handle);
    if (commit_error != ESP_OK) {
        return false;
    }

    s_complete = true;
    return true;
}

void onboarding_controller_reset(void)
{
    onboarding_controller_init();
    nvs_handle_t handle;
    if (nvs_open(ONBOARDING_NVS_NAMESPACE, NVS_READWRITE, &handle) == ESP_OK) {
        (void)nvs_erase_key(handle, ONBOARDING_NVS_KEY);
        (void)nvs_commit(handle);
        nvs_close(handle);
    }
    s_complete = false;
}
