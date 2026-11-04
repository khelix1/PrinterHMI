#include "thumbnail_path.h"
#include "printer_files.h"
#include "moonraker_file_metadata.h"

#include <stdio.h>
#include <limits.h>
#include <math.h>
#include "json_document_parse.h"
#include <string.h>
#include "cJSON.h"

static int metadata_integer(double value, double scale)
{
    if (!isfinite(value) || value <= 0 || value > (INT_MAX - 0.5) / scale) return 0;
    return (int)(value * scale + 0.5);
}

void printer_files_seconds_to_hhmm(double sec, char *out, size_t out_sz)
{
    if (!out || out_sz == 0) return;

    if (!isfinite(sec) || sec <= 0.0 || sec > INT_MAX - 0.5) {
        snprintf(out, out_sz, "--:--");
        return;
    }

    int total = metadata_integer(sec, 1);
    int h = total / 3600;
    int m = (total % 3600) / 60;

    snprintf(out, out_sz, "%d:%02d", h, m);
}

bool printer_files_build_metadata_text(const char *file,
                                       const char *metadata_json,
                                       char *thumbnail_path,
                                       size_t thumbnail_path_sz,
                                       char *out,
                                       size_t out_sz)
{
    if (!file || !file[0] || !metadata_json || !out || out_sz == 0) {
        return false;
    }

    if (thumbnail_path && thumbnail_path_sz) thumbnail_path[0] = 0;
    const cJSON *result;
    cJSON *root = moonraker_metadata_parse(metadata_json, &result);
    if (!root) return false;

    double est = moonraker_metadata_number(result, "estimated_time");
    double filament = moonraker_metadata_number(result, "filament_total");
    double layer_height = moonraker_metadata_number(result, "layer_height");
    double object_height = moonraker_metadata_number(result, "object_height");
    double size = moonraker_metadata_number(result, "size");

    char eta[32];
    printer_files_seconds_to_hhmm(est, eta, sizeof(eta));

    bool has_thumb = false;
    if (thumbnail_path && thumbnail_path_sz > 0) {
        thumbnail_path[0] = 0;
        has_thumb = moonraker_metadata_best_thumbnail(result,
                                                        thumbnail_path, thumbnail_path_sz);
        if (has_thumb) has_thumb = thumbnail_path_resolve(
            file, thumbnail_path, thumbnail_path, thumbnail_path_sz);
    }

    cJSON_Delete(root);

    int filament_m = metadata_integer(filament, 1.0 / 1000.0);
    int layer_x100 = metadata_integer(layer_height, 100);
    int height_x10 = metadata_integer(object_height, 10);
    int size_mb_x100 = metadata_integer(size, 100.0 / (1024.0 * 1024.0));

    snprintf(out, out_sz,
             "File:\n%.120s\n\n"
             "ETA: %s\n"
             "Filament: %d m\n"
             "Layer Height: %d.%02d mm\n"
             "Object Height: %d.%d mm\n"
             "Size: %d.%02d MB\n"
             "Thumbnail: %s\n"
             "%.80s\n\n"
             "Confirm to start this print.",
             file,
             eta,
             filament_m,
             layer_x100 / 100, layer_x100 % 100,
             height_x10 / 10, height_x10 % 10,
             size_mb_x100 / 100, size_mb_x100 % 100,
             has_thumb ? "metadata found" : "none",
             has_thumb ? thumbnail_path : "");

    return true;
}


int printer_files_parse_entries(const char *body,
                                printer_file_entry_t *entries,
                                size_t capacity)
{
    if (!body || !entries || capacity == 0) return -1;
    size_t length = strnlen(body, PRINTER_FILES_LIST_CAPACITY);
    cJSON *root = json_document_parse_bounded(body, length,
        PRINTER_FILES_LIST_CAPACITY - 1, 16);
    if (!root) return -1;

    cJSON *array = cJSON_GetObjectItemCaseSensitive(root, "result");
    if (!cJSON_IsArray(array) && cJSON_IsArray(root)) array = root;
    if (!cJSON_IsArray(array)) {
        cJSON_Delete(root);
        return -1;
    }

    size_t count = 0;
    cJSON *item = NULL;
    cJSON_ArrayForEach(item, array) {
        if (count >= capacity) break;
        if (!cJSON_IsObject(item)) continue;
        cJSON *path = cJSON_GetObjectItemCaseSensitive(item, "path");
        if (!cJSON_IsString(path) || !path->valuestring || !path->valuestring[0] ||
            strnlen(path->valuestring, PRINTER_FILES_MAX_PATH) >= PRINTER_FILES_MAX_PATH) {
            continue;
        }

        printer_file_entry_t *entry = &entries[count++];
        memset(entry, 0, sizeof(*entry));
        snprintf(entry->path, sizeof(entry->path), "%s", path->valuestring);

        cJSON *size = cJSON_GetObjectItemCaseSensitive(item, "size");
        cJSON *modified = cJSON_GetObjectItemCaseSensitive(item, "modified");
        if (cJSON_IsNumber(size) && isfinite(size->valuedouble) && size->valuedouble >= 0) entry->size = size->valuedouble;
        if (cJSON_IsNumber(modified) && isfinite(modified->valuedouble) && modified->valuedouble >= 0) entry->modified = modified->valuedouble;
    }

    cJSON_Delete(root);
    return (int)count;
}
