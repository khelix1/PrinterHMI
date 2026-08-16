#include "macro_controller.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"

static const char TAG[] = "macro_controller";
static const char MACRO_PREFIX[] = "gcode_macro ";
#define MACRO_FAVORITES_MAX 8
#define MACRO_NVS_NAMESPACE "netcfg"
#define MACRO_FAVORITES_KEY "mac_favs"

typedef struct {
    char names[MACRO_FAVORITES_MAX][MACRO_CONTROLLER_NAME_MAX];
} macro_favorites_t;
static macro_favorites_t s_favorites;
static bool s_favorites_loaded = false;

typedef struct {
    macro_controller_status_t status;
    char names[MACRO_CONTROLLER_MAX_MACROS]
              [MACRO_CONTROLLER_NAME_MAX];
} macro_store_t;

static macro_store_t *s_store = NULL;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;


static int compare_macro_names(
    const void *left,
    const void *right)
{
    return strcmp(
        (const char *)left,
        (const char *)right);
}


static void load_favorites(void)
{
    if (s_favorites_loaded) return;
    s_favorites_loaded = true;
    nvs_handle_t handle;
    if (nvs_open(MACRO_NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) return;
    size_t size = sizeof(s_favorites);
    (void)nvs_get_blob(handle, MACRO_FAVORITES_KEY, &s_favorites, &size);
    nvs_close(handle);
}

static void save_favorites(void)
{
    nvs_handle_t handle;
    if (nvs_open(MACRO_NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) return;
    (void)nvs_set_blob(handle, MACRO_FAVORITES_KEY, &s_favorites, sizeof(s_favorites));
    (void)nvs_commit(handle);
    nvs_close(handle);
}

bool macro_controller_is_favorite(const char *name)
{
    if (!name || !name[0]) return false;
    load_favorites();
    for (size_t i = 0; i < MACRO_FAVORITES_MAX; ++i) {
        if (strcmp(s_favorites.names[i], name) == 0) return true;
    }
    return false;
}

bool macro_controller_toggle_favorite(const char *name)
{
    if (!name || !name[0]) return false;
    load_favorites();
    for (size_t i = 0; i < MACRO_FAVORITES_MAX; ++i) {
        if (strcmp(s_favorites.names[i], name) == 0) {
            s_favorites.names[i][0] = '\0';
            save_favorites();
            return false;
        }
    }
    for (size_t i = 0; i < MACRO_FAVORITES_MAX; ++i) {
        if (!s_favorites.names[i][0]) {
            snprintf(s_favorites.names[i], sizeof(s_favorites.names[i]), "%s", name);
            save_favorites();
            return true;
        }
    }
    return false;
}

bool macro_controller_init(void)
{
    if (s_store) {
        return true;
    }

    s_store = heap_caps_calloc(
        1,
        sizeof(*s_store),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (s_store) {
        ESP_LOGI(
            TAG,
            "Macro catalog allocated permanently in PSRAM: %u bytes",
            (unsigned)sizeof(*s_store));
        return true;
    }

    s_store = heap_caps_calloc(
        1,
        sizeof(*s_store),
        MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

    if (!s_store) {
        ESP_LOGE(TAG, "Unable to allocate macro catalog");
        return false;
    }

    ESP_LOGW(TAG, "Macro catalog using internal RAM fallback");
    return true;
}


void macro_controller_update_from_objects(
    const struct cJSON *objects)
{
    if (!s_store) {
        return;
    }

    portENTER_CRITICAL(&s_lock);

    memset(
        s_store->names,
        0,
        sizeof(s_store->names));

    s_store->status.discovered =
        cJSON_IsArray(objects);
    s_store->status.count = 0;
    s_store->status.total_count = 0;
    s_store->status.truncated = false;

    if (cJSON_IsArray(objects)) {
        size_t prefix_length =
            strlen(MACRO_PREFIX);
        const cJSON *entry = NULL;

        cJSON_ArrayForEach(entry, objects) {
            if (!cJSON_IsString(entry) ||
                !entry->valuestring ||
                strncmp(
                    entry->valuestring,
                    MACRO_PREFIX,
                    prefix_length) != 0) {
                continue;
            }

            const char *name =
                entry->valuestring + prefix_length;

            /*
             * Klipper convention uses a leading underscore for internal
             * helper macros. The operator page shows only public actions.
             */
            if (!name[0] || name[0] == '_') {
                continue;
            }

            ++s_store->status.total_count;

            if (s_store->status.count >=
                MACRO_CONTROLLER_MAX_MACROS) {
                s_store->status.truncated = true;
                continue;
            }

            snprintf(
                s_store->names[s_store->status.count],
                MACRO_CONTROLLER_NAME_MAX,
                "%.*s",
                MACRO_CONTROLLER_NAME_MAX - 1,
                name);
            ++s_store->status.count;
        }

        qsort(
            s_store->names,
            s_store->status.count,
            sizeof(s_store->names[0]),
            compare_macro_names);
    }

    ++s_store->status.generation;
    if (s_store->status.generation == 0) {
        s_store->status.generation = 1;
    }

    portEXIT_CRITICAL(&s_lock);
}


void macro_controller_status(
    macro_controller_status_t *out)
{
    if (!out) {
        return;
    }

    memset(out, 0, sizeof(*out));
    if (!s_store) {
        return;
    }

    portENTER_CRITICAL(&s_lock);
    *out = s_store->status;
    portEXIT_CRITICAL(&s_lock);
}


bool macro_controller_get(
    size_t index,
    char *output,
    size_t output_size)
{
    if (!s_store || !output || output_size == 0) {
        return false;
    }

    bool found = false;
    portENTER_CRITICAL(&s_lock);

    if (index < s_store->status.count) {
        snprintf(
            output,
            output_size,
            "%s",
            s_store->names[index]);
        found = true;
    }

    portEXIT_CRITICAL(&s_lock);
    return found;
}
