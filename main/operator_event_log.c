#include "operator_event_log.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"

static const char *TAG = "operator_events";

static operator_event_t *s_events = NULL;
static size_t s_head = 0;
static size_t s_count = 0;
static uint32_t s_next_sequence = 1;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;


bool operator_event_log_init(void)
{
    if (s_events) {
        return true;
    }

    s_events = heap_caps_calloc(
        OPERATOR_EVENT_LOG_CAPACITY,
        sizeof(*s_events),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (s_events) {
        ESP_LOGI(
            TAG,
            "Event history allocated in PSRAM: %u bytes",
            (unsigned)(
                OPERATOR_EVENT_LOG_CAPACITY *
                sizeof(*s_events)));
        return true;
    }

    s_events = heap_caps_calloc(
        OPERATOR_EVENT_LOG_CAPACITY,
        sizeof(*s_events),
        MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

    if (!s_events) {
        ESP_LOGE(TAG, "Unable to allocate event history");
        return false;
    }

    ESP_LOGW(TAG, "Event history using internal RAM fallback");
    return true;
}


void operator_event_log_add(
    operator_event_level_t level,
    const char *format,
    ...)
{
    if (!s_events || !format) {
        return;
    }

    operator_event_t event = {
        .timestamp = time(NULL),
        .uptime_seconds =
            (uint32_t)(esp_timer_get_time() / 1000000LL),
        .level = level,
    };

    va_list arguments;
    va_start(arguments, format);
    vsnprintf(
        event.message,
        sizeof(event.message),
        format,
        arguments);
    va_end(arguments);

    portENTER_CRITICAL(&s_lock);

    event.sequence = s_next_sequence++;

    if (s_next_sequence == 0) {
        s_next_sequence = 1;
    }

    size_t write_index =
        (s_head + s_count) %
        OPERATOR_EVENT_LOG_CAPACITY;

    if (s_count == OPERATOR_EVENT_LOG_CAPACITY) {
        write_index = s_head;
        s_head =
            (s_head + 1) %
            OPERATOR_EVENT_LOG_CAPACITY;
    } else {
        ++s_count;
    }

    s_events[write_index] = event;

    portEXIT_CRITICAL(&s_lock);
}


size_t operator_event_log_count(void)
{
    size_t count;

    portENTER_CRITICAL(&s_lock);
    count = s_count;
    portEXIT_CRITICAL(&s_lock);

    return count;
}


bool operator_event_log_get(
    size_t newest_index,
    operator_event_t *out)
{
    if (!s_events || !out) {
        return false;
    }

    bool found = false;

    portENTER_CRITICAL(&s_lock);

    if (newest_index < s_count) {
        size_t chronological_index =
            s_count - 1 - newest_index;

        size_t physical_index =
            (s_head + chronological_index) %
            OPERATOR_EVENT_LOG_CAPACITY;

        *out = s_events[physical_index];
        found = true;
    }

    portEXIT_CRITICAL(&s_lock);

    return found;
}


void operator_event_log_clear(void)
{
    if (!s_events) {
        return;
    }

    portENTER_CRITICAL(&s_lock);

    memset(
        s_events,
        0,
        OPERATOR_EVENT_LOG_CAPACITY *
            sizeof(*s_events));

    s_head = 0;
    s_count = 0;

    portEXIT_CRITICAL(&s_lock);
}
