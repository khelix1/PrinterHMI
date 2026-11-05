#pragma once

#include <math.h>
#include <stdio.h>
#include "json_document_parse.h"
#include "thumbnail_path.h"

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

/* For row previews, prefer the smallest image that covers the requested long
 * edge. Other consumers pass zero and use the largest usable original. */
static inline bool moonraker_metadata_select_thumbnail(
    const cJSON *result, const char *file, unsigned minimum_edge, char *out, size_t out_sz)
{
    if (!out || !out_sz) return false;
    out[0] = 0;
    const cJSON *thumbnails = cJSON_GetObjectItemCaseSensitive(result, "thumbnails");
    if (!cJSON_IsArray(thumbnails)) return false;
    const char *best = NULL, *adequate = NULL;
    double best_area = -1, adequate_area = INFINITY;
    cJSON *thumbnail = NULL;
    cJSON_ArrayForEach(thumbnail, thumbnails) {
        const cJSON *path = cJSON_GetObjectItemCaseSensitive(thumbnail, "relative_path");
        if (!cJSON_IsString(path) || !thumbnail_path_fits(file, path->valuestring, out_sz))
            continue;
        double width = moonraker_metadata_number(thumbnail, "width");
        double height = moonraker_metadata_number(thumbnail, "height");
        bool valid = width > 0 && height > 0 && width <= 32768 && height <= 32768;
        double area = valid ? width * height : 0;
        if (area > best_area) {
            best = path->valuestring;
            best_area = area;
        }
        if (minimum_edge && valid && fmax(width, height) >= minimum_edge && area < adequate_area) {
            adequate = path->valuestring;
            adequate_area = area;
        }
    }
    return thumbnail_path_resolve(file, adequate ? adequate : best, out, out_sz);
}

static inline bool moonraker_metadata_thumbnail_path(
    const char *json, const char *file, unsigned minimum_edge, char *out, size_t out_sz)
{
    const cJSON *result;
    cJSON *root = moonraker_metadata_parse(json, &result);
    bool found = moonraker_metadata_select_thumbnail(result, file, minimum_edge, out, out_sz);
    cJSON_Delete(root);
    return found;
}
