#include "bed_mesh_controller.h"
#include <float.h>
#include <stdio.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

typedef struct {
    bool valid, truncated;
    uint16_t rows, cols;
    double mesh_min_x, mesh_min_y, mesh_max_x, mesh_max_y;
    double minimum, maximum, average, range;
    char profile_name[BED_MESH_PROFILE_NAME_MAX];
    size_t profile_count;
    bed_mesh_profile_name_t *profile_names;
    size_t profile_capacity;
    float *values;
    size_t capacity;
} mesh_state_t;
static mesh_state_t s;
static SemaphoreHandle_t mutex;
static void lock(void){ if(mutex) xSemaphoreTake(mutex, portMAX_DELAY); }
static void unlock(void){ if(mutex) xSemaphoreGive(mutex); }
static bool ensure_capacity(size_t count){
    if(s.values && s.capacity >= count) return true;
    float *p = heap_caps_calloc(count,sizeof(float),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!p) p = heap_caps_calloc(count,sizeof(float),MALLOC_CAP_8BIT);
    if(!p) return false;
    if(s.values) heap_caps_free(s.values);
    s.values=p; s.capacity=count; return true;
}

static bool ensure_profile_capacity(void)
{
    if (s.profile_names &&
        s.profile_capacity >= BED_MESH_MAX_PROFILES) {
        return true;
    }

    bed_mesh_profile_name_t *profiles =
        heap_caps_calloc(
            BED_MESH_MAX_PROFILES,
            sizeof(*profiles),
            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (!profiles) {
        profiles =
            heap_caps_calloc(
                BED_MESH_MAX_PROFILES,
                sizeof(*profiles),
                MALLOC_CAP_8BIT);
    }

    if (!profiles) {
        return false;
    }

    if (s.profile_names) {
        heap_caps_free(s.profile_names);
    }

    s.profile_names = profiles;
    s.profile_capacity = BED_MESH_MAX_PROFILES;
    return true;
}

static bool read_xy(cJSON *a,double *x,double *y){
    if(!cJSON_IsArray(a)||cJSON_GetArraySize(a)<2) return false;
    cJSON *jx=cJSON_GetArrayItem(a,0), *jy=cJSON_GetArrayItem(a,1);
    if(!cJSON_IsNumber(jx)||!cJSON_IsNumber(jy)) return false;
    *x=jx->valuedouble; *y=jy->valuedouble; return true;
}

static bool valid_profile_character(char character)
{
    return
        (character >= 'a' && character <= 'z') ||
        (character >= 'A' && character <= 'Z') ||
        (character >= '0' && character <= '9') ||
        character == '_' ||
        character == '-' ||
        character == '.';
}

static bool valid_profile_name(const char *name)
{
    if (!name || !name[0]) {
        return false;
    }

    size_t length =
        strnlen(name, BED_MESH_PROFILE_NAME_MAX);

    if (length == 0 ||
        length >= BED_MESH_PROFILE_NAME_MAX) {
        return false;
    }

    for (size_t i = 0; i < length; ++i) {
        if (!valid_profile_character(name[i])) {
            return false;
        }
    }

    return true;
}

static void sort_profiles_locked(void)
{
    for (size_t i = 1; i < s.profile_count; ++i) {
        char current[BED_MESH_PROFILE_NAME_MAX];
        snprintf(current, sizeof(current), "%s", s.profile_names[i]);

        size_t position = i;

        while (position > 0 &&
               strcmp(
                   s.profile_names[position - 1],
                   current) > 0) {
            memmove(
                s.profile_names[position],
                s.profile_names[position - 1],
                sizeof(s.profile_names[position]));
            --position;
        }

        snprintf(
            s.profile_names[position],
            BED_MESH_PROFILE_NAME_MAX,
            "%s",
            current);
    }
}

static void read_profiles_locked(cJSON *profiles)
{
    s.profile_count = 0;

    if (!ensure_profile_capacity()) {
        return;
    }

    memset(
        s.profile_names,
        0,
        s.profile_capacity * sizeof(*s.profile_names));

    if (!cJSON_IsObject(profiles)) {
        return;
    }

    cJSON *entry = NULL;

    cJSON_ArrayForEach(entry, profiles) {
        const char *name = entry->string;

        if (s.profile_count >= BED_MESH_MAX_PROFILES ||
            !valid_profile_name(name)) {
            continue;
        }

        snprintf(
            s.profile_names[s.profile_count],
            BED_MESH_PROFILE_NAME_MAX,
            "%s",
            name);
        ++s.profile_count;
    }

    sort_profiles_locked();
}

void bed_mesh_controller_init(void)
{
    if (!mutex) {
        mutex = xSemaphoreCreateMutex();
    }

    if (!mutex) {
        return;
    }

    lock();
    memset(&s, 0, sizeof(s));
    unlock();
}
void bed_mesh_controller_reset(void){
    lock(); s.valid=false; s.rows=s.cols=0; s.profile_name[0]=0; unlock();
}
bool bed_mesh_controller_merge_status(cJSON *status)
{
    if (!cJSON_IsObject(status)) {
        return false;
    }

    cJSON *bed_mesh =
        cJSON_GetObjectItemCaseSensitive(status, "bed_mesh");

    if (!cJSON_IsObject(bed_mesh)) {
        return false;
    }

    cJSON *profiles =
        cJSON_GetObjectItemCaseSensitive(bed_mesh, "profiles");
    cJSON *profile =
        cJSON_GetObjectItemCaseSensitive(
            bed_mesh,
            "profile_name");
    cJSON *mesh_matrix =
        cJSON_GetObjectItemCaseSensitive(bed_mesh, "mesh_matrix");
    cJSON *probed_matrix =
        cJSON_GetObjectItemCaseSensitive(bed_mesh, "probed_matrix");
    bool matrix_present =
        mesh_matrix != NULL ||
        probed_matrix != NULL;
    cJSON *matrix = mesh_matrix;

    if (!cJSON_IsArray(matrix) ||
        cJSON_GetArraySize(matrix) <= 0) {
        matrix = probed_matrix;
    }

    /*
     * Moonraker status notifications are partial. Profile-name or saved-list
     * updates must not clear an unchanged active mesh.
     */
    if (!cJSON_IsArray(matrix) ||
        cJSON_GetArraySize(matrix) <= 0) {
        lock();

        if (cJSON_IsObject(profiles)) {
            read_profiles_locked(profiles);
        }

        if (cJSON_IsString(profile) &&
            profile->valuestring) {
            snprintf(
                s.profile_name,
                sizeof(s.profile_name),
                "%s",
                profile->valuestring);
        }

        /*
         * Only an explicitly reported empty matrix clears the mesh.
         * A missing matrix means this notification changed another field.
         */
        if (matrix_present) {
            s.valid = false;
            s.rows = 0;
            s.cols = 0;
        }

        unlock();
        return true;
    }

    int source_rows = cJSON_GetArraySize(matrix);
    cJSON *first_row = cJSON_GetArrayItem(matrix, 0);
    int source_cols =
        cJSON_IsArray(first_row)
            ? cJSON_GetArraySize(first_row)
            : 0;

    if (source_cols <= 0) {
        return false;
    }

    uint16_t rows =
        source_rows > BED_MESH_MAX_ROWS
            ? BED_MESH_MAX_ROWS
            : (uint16_t)source_rows;
    uint16_t cols =
        source_cols > BED_MESH_MAX_COLS
            ? BED_MESH_MAX_COLS
            : (uint16_t)source_cols;
    size_t count = (size_t)rows * cols;

    lock();

    if (!ensure_capacity(count)) {
        unlock();
        return false;
    }

    if (cJSON_IsObject(profiles)) {
        read_profiles_locked(profiles);
    }

    double low = DBL_MAX;
    double high = -DBL_MAX;
    double sum = 0.0;
    size_t valid_count = 0;

    for (uint16_t y = 0; y < rows; ++y) {
        cJSON *row = cJSON_GetArrayItem(matrix, y);

        for (uint16_t x = 0; x < cols; ++x) {
            cJSON *value =
                cJSON_IsArray(row)
                    ? cJSON_GetArrayItem(row, x)
                    : NULL;
            double z =
                cJSON_IsNumber(value)
                    ? value->valuedouble
                    : 0.0;

            s.values[(size_t)y * cols + x] = (float)z;

            if (cJSON_IsNumber(value)) {
                if (z < low) {
                    low = z;
                }
                if (z > high) {
                    high = z;
                }
                sum += z;
                ++valid_count;
            }
        }
    }

    if (cJSON_IsString(profile) &&
        profile->valuestring) {
        snprintf(
            s.profile_name,
            sizeof(s.profile_name),
            "%s",
            profile->valuestring);
    } else if (!s.profile_name[0]) {
        snprintf(
            s.profile_name,
            sizeof(s.profile_name),
            "%s",
            "active");
    }

    read_xy(
        cJSON_GetObjectItemCaseSensitive(
            bed_mesh,
            "mesh_min"),
        &s.mesh_min_x,
        &s.mesh_min_y);
    read_xy(
        cJSON_GetObjectItemCaseSensitive(
            bed_mesh,
            "mesh_max"),
        &s.mesh_max_x,
        &s.mesh_max_y);

    s.rows = rows;
    s.cols = cols;
    s.truncated =
        source_rows > BED_MESH_MAX_ROWS ||
        source_cols > BED_MESH_MAX_COLS;
    s.minimum = valid_count ? low : 0.0;
    s.maximum = valid_count ? high : 0.0;
    s.average =
        valid_count
            ? sum / valid_count
            : 0.0;
    s.range = s.maximum - s.minimum;
    s.valid = valid_count > 0;

    unlock();
    return true;
}

bool bed_mesh_controller_snapshot(
    bed_mesh_snapshot_t *out,
    float *values,
    size_t capacity,
    bed_mesh_profile_name_t *profile_names,
    size_t profile_capacity)
{
    if (!out) {
        return false;
    }

    lock();
    memset(out, 0, sizeof(*out));

    if (s.profile_names &&
        profile_names &&
        profile_capacity >= s.profile_count) {
        out->profile_count = s.profile_count;
        memcpy(
            profile_names,
            s.profile_names,
            s.profile_count * sizeof(*profile_names));
        out->profile_names = profile_names;
    }

    size_t count = (size_t)s.rows * s.cols;

    if (!s.valid ||
        !s.values ||
        !values ||
        capacity < count) {
        unlock();
        return out->profile_count > 0;
    }

    out->valid = s.valid;
    out->truncated = s.truncated;
    out->rows = s.rows;
    out->cols = s.cols;
    out->mesh_min_x = s.mesh_min_x;
    out->mesh_min_y = s.mesh_min_y;
    out->mesh_max_x = s.mesh_max_x;
    out->mesh_max_y = s.mesh_max_y;
    out->minimum = s.minimum;
    out->maximum = s.maximum;
    out->average = s.average;
    out->range = s.range;

    snprintf(
        out->profile_name,
        sizeof(out->profile_name),
        "%s",
        s.profile_name);

    memcpy(values, s.values, count * sizeof(float));
    out->values = values;

    unlock();
    return true;
}
