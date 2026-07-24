#include "printer_files.h"
#include "moonraker.h"

#include <stdio.h>
#include <string.h>
#include "cJSON.h"

void printer_files_seconds_to_hhmm(double sec, char *out, size_t out_sz)
{
    if (!out || out_sz == 0) return;

    if (sec <= 0.0) {
        snprintf(out, out_sz, "--:--");
        return;
    }

    int total = (int)(sec + 0.5);
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

    double est = 0.0;
    double filament = 0.0;
    double layer_height = 0.0;
    double object_height = 0.0;
    double size = 0.0;

    json_find_number_after_key(metadata_json, "\"estimated_time\"", &est);
    json_find_number_after_key(metadata_json, "\"filament_total\"", &filament);
    json_find_number_after_key(metadata_json, "\"layer_height\"", &layer_height);
    json_find_number_after_key(metadata_json, "\"object_height\"", &object_height);
    json_find_number_after_key(metadata_json, "\"size\"", &size);

    char eta[32];
    printer_files_seconds_to_hhmm(est, eta, sizeof(eta));

    bool has_thumb = false;
    if (thumbnail_path && thumbnail_path_sz > 0) {
        thumbnail_path[0] = 0;
        has_thumb = json_find_best_thumbnail_path(metadata_json,
                                                  thumbnail_path,
                                                  thumbnail_path_sz);
    }

    int filament_m = (int)((filament / 1000.0) + 0.5);
    int layer_x100 = (int)((layer_height * 100.0) + 0.5);
    int height_x10 = (int)((object_height * 10.0) + 0.5);
    int size_mb_x100 = (int)(((size / (1024.0 * 1024.0)) * 100.0) + 0.5);

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


int printer_files_for_each_path(const char *body,
                                void (*cb)(const char *path, void *user),
                                void *user)
{
    if (!body || !cb) return 0;

    const char *pcur = body;
    int count = 0;

    while ((pcur = strstr(pcur, "\"path\""))) {
        const char *colon = strchr(pcur, ':');
        if (!colon) break;
        const char *q1 = strchr(colon, '"');
        if (!q1) break;
        const char *q2 = strchr(q1 + 1, '"');
        if (!q2) break;

        char path[160];
        size_t n = q2 - (q1 + 1);
        if (n >= sizeof(path)) n = sizeof(path) - 1;
        memcpy(path, q1 + 1, n);
        path[n] = 0;

        cb(path, user);
        count++;

        pcur = q2 + 1;
    }

    return count;
}

int printer_files_parse_entries(const char *body,
                                printer_file_entry_t *entries,
                                size_t capacity)
{
    if (!body || !entries || capacity == 0) return 0;

    cJSON *root = cJSON_Parse(body);
    if (!root) return 0;

    cJSON *array = cJSON_GetObjectItemCaseSensitive(root, "result");
    if (!cJSON_IsArray(array) && cJSON_IsArray(root)) array = root;
    if (!cJSON_IsArray(array)) {
        cJSON_Delete(root);
        return 0;
    }

    size_t count = 0;
    cJSON *item = NULL;
    cJSON_ArrayForEach(item, array) {
        if (count >= capacity || !cJSON_IsObject(item)) break;
        cJSON *path = cJSON_GetObjectItemCaseSensitive(item, "path");
        if (!cJSON_IsString(path) || !path->valuestring || !path->valuestring[0]) {
            continue;
        }

        printer_file_entry_t *entry = &entries[count++];
        memset(entry, 0, sizeof(*entry));
        snprintf(entry->path, sizeof(entry->path), "%s", path->valuestring);

        cJSON *size = cJSON_GetObjectItemCaseSensitive(item, "size");
        cJSON *modified = cJSON_GetObjectItemCaseSensitive(item, "modified");
        if (cJSON_IsNumber(size)) entry->size = size->valuedouble;
        if (cJSON_IsNumber(modified)) entry->modified = modified->valuedouble;
    }

    cJSON_Delete(root);
    return (int)count;
}
