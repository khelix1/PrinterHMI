#include "telemetry_history.h"

#include <string.h>

#include "esp_heap_caps.h"
#include "esp_log.h"

static const char *TAG = "telemetry_history";

static telemetry_sample_t *s_samples = NULL;
static size_t s_head = 0;
static size_t s_count = 0;
static int64_t s_last_sample_us = 0;

bool telemetry_history_init(void)
{
    if (s_samples) {
        return true;
    }

    s_samples = heap_caps_calloc(
        TELEMETRY_HISTORY_CAPACITY,
        sizeof(*s_samples),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (!s_samples) {
        ESP_LOGW(
            TAG,
            "PSRAM allocation failed; falling back to internal RAM");

        s_samples = heap_caps_calloc(
            TELEMETRY_HISTORY_CAPACITY,
            sizeof(*s_samples),
            MALLOC_CAP_8BIT);
    }

    if (!s_samples) {
        ESP_LOGE(TAG, "Unable to allocate telemetry history");
        return false;
    }

    ESP_LOGI(
        TAG,
        "History allocated: %u samples, %u bytes",
        (unsigned)TELEMETRY_HISTORY_CAPACITY,
        (unsigned)(
            TELEMETRY_HISTORY_CAPACITY *
            sizeof(*s_samples)));

    return true;
}

void telemetry_history_reset(void)
{
    if (s_samples) {
        memset(
            s_samples,
            0,
            TELEMETRY_HISTORY_CAPACITY *
                sizeof(*s_samples));
    }

    s_head = 0;
    s_count = 0;
    s_last_sample_us = 0;

    ESP_LOGI(TAG, "History reset for active-printer change");
}


static bool telemetry_state_is_valid(
    const moonraker_state_t *state)
{
    if (!state) {
        return false;
    }

    if (!state->live_data_ok) {
        return false;
    }

    /*
     * At least one meaningful temperature source must be present.
     */
    return state->nozzle_temp > -100.0 ||
           state->bed_temp > -100.0 ||
           state->air_temp > -100.0 ||
           state->chamber_temp > -100.0;
}

bool telemetry_history_sample(
    const moonraker_state_t *state,
    int64_t now_us)
{
    if (!telemetry_history_init()) {
        return false;
    }

    if (!telemetry_state_is_valid(state)) {
        return false;
    }

    if (s_last_sample_us != 0 &&
        now_us - s_last_sample_us <
            TELEMETRY_HISTORY_SAMPLE_INTERVAL_US) {
        return false;
    }

    telemetry_sample_t sample = {
        .nozzle_temp = state->nozzle_temp,
        .nozzle_target = state->nozzle_target,
        .bed_temp = state->bed_temp,
        .bed_target = state->bed_target,

        .air_temp = state->air_temp,
        .center_temp = state->chamber_temp,
        .humidity = state->humidity,

        .live_velocity = state->live_velocity,
        .live_flow = state->live_flow,

        .part_fan_speed = state->part_fan_speed,
        .drybox_fan_speed = state->drybox_fan_speed,
        .speed_factor = state->speed_factor,
        .flow_factor = state->flow_factor,
    };

    s_samples[s_head] = sample;
    s_head = (s_head + 1) % TELEMETRY_HISTORY_CAPACITY;

    if (s_count < TELEMETRY_HISTORY_CAPACITY) {
        s_count++;
    }

    s_last_sample_us = now_us;
    return true;
}

size_t telemetry_history_count(void)
{
    return s_count;
}

bool telemetry_history_get(
    size_t logical_index,
    telemetry_sample_t *out)
{
    if (!s_samples || !out || logical_index >= s_count) {
        return false;
    }

    size_t oldest =
        (s_head + TELEMETRY_HISTORY_CAPACITY - s_count) %
        TELEMETRY_HISTORY_CAPACITY;

    size_t physical =
        (oldest + logical_index) %
        TELEMETRY_HISTORY_CAPACITY;

    *out = s_samples[physical];
    return true;
}
