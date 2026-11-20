#pragma once

#include <string.h>

#include "moonraker.h"
#include "moonraker_telemetry_number.h"
#include "json_document_parse.h"

static inline cJSON *moonraker_http_objects_parse(
    const char *json, size_t length, size_t max_length, const cJSON **status)
{
    *status = NULL;
    cJSON *root = json_document_parse_bounded(json, length, max_length, 16);
    const cJSON *result = cJSON_GetObjectItemCaseSensitive(root, "result");
    const cJSON *objects = cJSON_GetObjectItemCaseSensitive(result, "status");
    if (cJSON_IsObject(objects)) {
        *status = objects;
        return root;
    }
    cJSON_Delete(root);
    return NULL;
}

static inline bool moonraker_http_object_number(const cJSON *status,
    const char *object, const char *field, double scale, double *out)
{
    const cJSON *entry = cJSON_GetObjectItemCaseSensitive(status, object);
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(entry, field);
    return moonraker_telemetry_number(value, scale, out);
}

/* HTTP is a full snapshot: use only objects included in this response. */
static inline size_t moonraker_http_hotends(const cJSON *status,
    moonraker_hotend_t hotends[MOONRAKER_MAX_HOTENDS])
{
    static const char *const names[] = {"extruder", "extruder1", "extruder2", "extruder3"};
    const cJSON *toolhead = cJSON_GetObjectItemCaseSensitive(status, "toolhead");
    const char *active = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(toolhead, "extruder"));
    size_t count = 0;
    bool matched_active = false;
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        const cJSON *entry = cJSON_GetObjectItemCaseSensitive(status, names[i]);
        if (!cJSON_IsObject(entry)) continue;
        moonraker_hotend_t *hotend = &hotends[count++];
        *hotend = (moonraker_hotend_t){.temperature = -999.0, .target = -999.0};
        memcpy(hotend->object_name, names[i], strlen(names[i]) + 1);
        moonraker_telemetry_number(cJSON_GetObjectItemCaseSensitive(entry, "temperature"), 1, &hotend->temperature);
        moonraker_telemetry_number(cJSON_GetObjectItemCaseSensitive(entry, "target"), 1, &hotend->target);
        hotend->active = active && !strcmp(active, names[i]);
        matched_active |= hotend->active;
    }
    if (count && !matched_active) hotends[0].active = true;
    return count;
}
