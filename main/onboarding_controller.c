#include "onboarding_controller.h"

#include "esp_err.h"
#include "nvs.h"

#define ONBOARDING_NVS_NAMESPACE "onboard"
#define ONBOARDING_NVS_KEY "complete"
#define ONBOARDING_NVS_STEPS_KEY "steps"
#define ONBOARDING_STEP_MASK ((1U << ONBOARDING_STEP_COUNT) - 1U)

static bool s_initialized = false;
static bool s_complete = false;
static uint8_t s_steps;

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
        (void)nvs_get_u8(handle, ONBOARDING_NVS_STEPS_KEY, &s_steps);
        s_steps &= ONBOARDING_STEP_MASK;
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

bool onboarding_controller_step_complete(onboarding_step_t step)
{
    if ((unsigned)step >= ONBOARDING_STEP_COUNT) return false;
    onboarding_controller_init();
    return s_complete || (s_steps & (1U << step)) != 0;
}

bool onboarding_controller_mark_step_complete(onboarding_step_t step)
{
    if ((unsigned)step >= ONBOARDING_STEP_COUNT) return false;
    if (onboarding_controller_step_complete(step)) return true;
    const uint8_t updated = s_steps | (1U << step);
    nvs_handle_t handle;
    if (nvs_open(ONBOARDING_NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK)
        return false;
    esp_err_t error = nvs_set_u8(handle, ONBOARDING_NVS_STEPS_KEY, updated);
    if (error == ESP_OK) error = nvs_commit(handle);
    nvs_close(handle);
    if (error != ESP_OK) return false;
    s_steps = updated;
    return true;
}
