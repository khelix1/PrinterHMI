#include "telemetry_history.h"

#include <string.h>
#include <math.h>
#include <stdio.h>

#include "esp_heap_caps.h"
#include "esp_log.h"

static const char *TAG = "telemetry_history";

static telemetry_sample_t *s_samples = NULL;
static size_t s_head = 0;
static size_t s_count = 0;
static int64_t s_last_sample_us = 0;
static uint32_t s_generation;

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

    ++s_generation;
    s_head = 0;
    s_count = 0;
    s_last_sample_us = 0;

    ESP_LOGI(TAG, "History reset for active-printer change");
}


uint32_t telemetry_history_generation(void){return s_generation;}

static double reading(double v,double minimum,double maximum)
{
    return isfinite(v) && v>=minimum && v<=maximum?v:NAN;
}

void telemetry_history_from_state(const moonraker_state_t *state,int64_t now_us,telemetry_sample_t *out)
{
    if(!out)return;
    *out=(telemetry_sample_t){.time_us=now_us,.nozzle_temp=NAN,.nozzle_target=NAN,
        .bed_temp=NAN,.bed_target=NAN,.air_temp=NAN,.center_temp=NAN,.humidity=NAN,
        .live_velocity=NAN,.live_flow=NAN,.part_fan_speed=NAN,.drybox_fan_speed=NAN,
        .speed_factor=NAN,.flow_factor=NAN,.heater_target=NAN};
    if(!state)return;
    const moonraker_capabilities_t *caps=&state->capabilities;
    if(caps->discovered)out->unavailable=(!caps->has_heated_bed?TELEMETRY_NO_BED:0) |
        (!caps->has_drybox_center_sensor?TELEMETRY_NO_CENTER:0) |
        (!caps->has_drybox_environment_sensor?TELEMETRY_NO_ENV:0) |
        (!caps->has_part_fan?TELEMETRY_NO_PART_FAN:0) |
        (!caps->has_drybox_fan?TELEMETRY_NO_DRYBOX_FAN:0);
    if(!state->live_data_ok)return;
    out->nozzle_temp=reading(state->nozzle_temp,-100,1000);
    out->nozzle_target=reading(state->nozzle_target,0,1000);
    out->hotend_count=state->hotend_count>MOONRAKER_MAX_HOTENDS?MOONRAKER_MAX_HOTENDS:state->hotend_count;
    snprintf(out->active_hotend,sizeof(out->active_hotend),"%s",state->active_hotend);
    for(size_t i=0;i<out->hotend_count;i++) {
        out->hotends[i]=state->hotends[i];
        out->hotends[i].temperature=reading(state->hotends[i].temperature,-100,1000);
        out->hotends[i].target=reading(state->hotends[i].target,0,1000);
    }
    if(!caps->discovered || caps->has_heated_bed) {
        out->bed_temp=reading(state->bed_temp,-100,1000);
        out->bed_target=reading(state->bed_target,0,1000);
    }
    if(!caps->discovered || caps->has_drybox_environment_sensor) {
        out->air_temp=reading(state->air_temp,-100,1000);
        out->humidity=reading(state->humidity,0,100);
    }
    if(!caps->discovered || caps->has_drybox_center_sensor)out->center_temp=reading(state->chamber_temp,-100,1000);
    if(!caps->discovered || caps->has_drybox_heater)out->heater_target=reading(state->heater_target,0,1000);
    if(!caps->discovered || caps->has_part_fan)out->part_fan_speed=reading(state->part_fan_speed,0,100);
    if(!caps->discovered || caps->has_drybox_fan)out->drybox_fan_speed=reading(state->drybox_fan_speed,0,100);
    out->live_velocity=reading(state->live_velocity,0,100000);
    out->live_flow=reading(state->live_flow,0,100000);
    out->speed_factor=reading(state->speed_factor,0,10000);
    out->flow_factor=reading(state->flow_factor,0,10000);
}

bool telemetry_history_sample(
    const moonraker_state_t *state,
    int64_t now_us)
{
    if (!telemetry_history_init()) {
        return false;
    }

    if(!state || !state->live_data_ok || now_us<0)return false;
    /* Match the chart's wall-clock bins instead of restarting a two-second
     * delay after each late UI poll. Jitter must not accumulate into holes.
     * Keep real timestamps and never backfill missed/offline intervals. */
    if(s_count && now_us>=s_last_sample_us &&
       now_us/TELEMETRY_HISTORY_SAMPLE_INTERVAL_US ==
       s_last_sample_us/TELEMETRY_HISTORY_SAMPLE_INTERVAL_US)return false;
    if(s_count && now_us<s_last_sample_us)telemetry_history_reset();
    telemetry_sample_t sample;
    telemetry_history_from_state(state,now_us,&sample);

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
