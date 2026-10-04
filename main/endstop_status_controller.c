#include "endstop_status_controller.h"
#include <stdio.h>
#include <string.h>
#include "cJSON.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"

static endstop_status_snapshot_t s_status;
static uint32_t s_request = ENDSTOP_REQUEST_FIRST;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;

void endstop_status_controller_reset(void)
{
    portENTER_CRITICAL(&s_lock);
    memset(&s_status, 0, sizeof(s_status));
    portEXIT_CRITICAL(&s_lock);
}

uint32_t endstop_status_controller_begin(uint32_t generation)
{
    int64_t now = esp_timer_get_time();
    portENTER_CRITICAL(&s_lock);
    if (++s_request >= 0x80000000U) s_request = ENDSTOP_REQUEST_FIRST;
    memset(&s_status, 0, sizeof(s_status));
    s_status.waiting = true;
    s_status.owner_generation = generation;
    s_status.request_id = s_request;
    s_status.updated_us = now;
    uint32_t id = s_request;
    portEXIT_CRITICAL(&s_lock);
    return id;
}

void endstop_status_controller_failed(const char *message)
{
    int64_t now = esp_timer_get_time();
    portENTER_CRITICAL(&s_lock);
    s_status.updated_us = now;
    s_status.waiting = false;
    s_status.valid = false;
    snprintf(s_status.error, sizeof(s_status.error), "%s", message);
    portEXIT_CRITICAL(&s_lock);
}

bool endstop_status_controller_merge(const struct cJSON *root, uint32_t generation)
{
    const cJSON *id = cJSON_GetObjectItemCaseSensitive(root, "id");
    if (!cJSON_IsNumber(id) || id->valuedouble < ENDSTOP_REQUEST_FIRST ||
        id->valuedouble >= 0x80000000U) return false;
    endstop_status_snapshot_t next = {0};
    next.owner_generation = generation;
    next.request_id = (uint32_t)id->valuedouble;
    next.updated_us = esp_timer_get_time();
    const cJSON *result = cJSON_GetObjectItemCaseSensitive(root, "result");
    const cJSON *error = cJSON_GetObjectItemCaseSensitive(root, "error");
    if (!cJSON_IsObject(result) || error) {
        const cJSON *message = cJSON_GetObjectItemCaseSensitive(error, "message");
        snprintf(next.error, sizeof(next.error), "%s",
            cJSON_IsString(message) ? message->valuestring : "Endstop query failed.");
    } else {
        const cJSON *entry;
        cJSON_ArrayForEach(entry, result) {
            if (!entry->string || !cJSON_IsString(entry) ||
                (strcmp(entry->valuestring, "open") && strcmp(entry->valuestring, "TRIGGERED"))) continue;
            if (next.count == ENDSTOP_STATUS_MAX) { next.truncated = true; continue; }
            snprintf(next.items[next.count].name, ENDSTOP_STATUS_NAME_MAX, "%s", entry->string);
            next.items[next.count++].triggered = !strcmp(entry->valuestring, "TRIGGERED");
        }
        next.valid = true;
    }
    portENTER_CRITICAL(&s_lock);
    if (s_status.waiting && s_status.request_id == next.request_id &&
        s_status.owner_generation == generation) s_status = next;
    portEXIT_CRITICAL(&s_lock);
    return true; /* Consume even a late response from a closed viewer. */
}

void endstop_status_controller_snapshot(endstop_status_snapshot_t *out)
{
    if (!out) return;
    int64_t now = esp_timer_get_time();
    portENTER_CRITICAL(&s_lock);
    if (s_status.waiting && now - s_status.updated_us >= 5000000LL) {
        s_status.waiting = false;
        s_status.valid = false;
        s_status.updated_us = now;
        snprintf(s_status.error, sizeof(s_status.error), "Endstop query timed out. Retrying...");
    }
    *out = s_status;
    portEXIT_CRITICAL(&s_lock);
}
