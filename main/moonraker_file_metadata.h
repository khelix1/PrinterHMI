#pragma once

#include <math.h>
#include <stdio.h>
#include "json_document_parse.h"

#define MOONRAKER_METADATA_CAPACITY 8192U

static inline cJSON *moonraker_metadata_parse(const char *json, const cJSON **result)
{
    *result = NULL;
    if (!json) return NULL;
    size_t length = 0;
    while (length < MOONRAKER_METADATA_CAPACITY && json[length]) ++length;
    cJSON *root = json_document_parse_bounded(json, length, MOONRAKER_METADATA_CAPACITY - 1, 16);
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(root, "result");
    if (cJSON_IsObject(value)) {
        *result = value;
        return root;
    }
    cJSON_Delete(root);
    return NULL;
}

static inline double moonraker_metadata_number(const cJSON *result, const char *key)
{
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(result, key);
    return cJSON_IsNumber(value) && isfinite(value->valuedouble) &&
        value->valuedouble > 0 ? value->valuedouble : 0;
}

static inline bool moonraker_metadata_best_thumbnail(
    const cJSON *result, char *out, size_t out_sz)
{
    if (!out || !out_sz) return false;
    out[0] = 0;
    cJSON *thumbnails = cJSON_GetObjectItemCaseSensitive(result, "thumbnails");
    if (!cJSON_IsArray(thumbnails)) return false;
    const char *best_path = NULL;
    double best_area = -1.0;
    cJSON *thumbnail = NULL;
    cJSON_ArrayForEach(thumbnail, thumbnails) {
        cJSON *path = cJSON_GetObjectItemCaseSensitive(thumbnail, "relative_path");
        if (!cJSON_IsString(path) || !path->valuestring || !path->valuestring[0] ||
            strlen(path->valuestring) >= out_sz) continue;
        cJSON *width = cJSON_GetObjectItemCaseSensitive(thumbnail, "width");
        cJSON *height = cJSON_GetObjectItemCaseSensitive(thumbnail, "height");
        double area = 0.0;
        if (cJSON_IsNumber(width) && cJSON_IsNumber(height) &&
            isfinite(width->valuedouble) && isfinite(height->valuedouble) &&
            width->valuedouble > 0 && height->valuedouble > 0 &&
            width->valuedouble <= 32768 && height->valuedouble <= 32768) {
            area = width->valuedouble * height->valuedouble;
        }
        if (area > best_area) {
            best_area = area;
            best_path = path->valuestring;
        }
    }
    if (best_path) snprintf(out, out_sz, "%s", best_path);
    return out[0] != 0;
}
