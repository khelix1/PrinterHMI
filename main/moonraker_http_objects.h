#pragma once

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
