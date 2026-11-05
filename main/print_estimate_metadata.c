#include "print_estimate_metadata.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "moonraker.h"
#include "moonraker_config_controller.h"
#include "moonraker_file_metadata.h"
#include "thumbnail_manager.h"

#define ESTIMATE_RETRY_US INT64_C(10000000)

typedef struct {
    uint32_t generation, revision;
    char file[sizeof(((moonraker_state_t *)0)->printer_file)];
} estimate_key_t;

typedef struct {
    estimate_key_t key;
    char host[MOONRAKER_CONFIG_HOST_LENGTH];
    char api_key[MOONRAKER_CONFIG_API_KEY_LENGTH];
    int port;
} estimate_request_t;

static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static estimate_key_t s_key;
static bool s_busy, s_valid;
static int64_t s_retry_at;
static print_estimate_metadata_t s_metadata;

static bool same_key(const estimate_key_t *key)
{
    return key->generation == s_key.generation && key->revision == s_key.revision &&
           !strcmp(key->file, s_key.file);
}

static bool parse_metadata(const char *body, print_estimate_metadata_t *out)
{
    const cJSON *result;
    cJSON *root = moonraker_metadata_parse(body, &result);
    if (!root) return false;
    *out = (print_estimate_metadata_t){
        .start_byte = -1, .end_byte = -1,
        .filament_total = moonraker_metadata_number(result, "filament_total"),
        .estimated_time = moonraker_metadata_number(result, "estimated_time"),
    };
    const cJSON *start = cJSON_GetObjectItemCaseSensitive(result, "gcode_start_byte");
    const cJSON *end = cJSON_GetObjectItemCaseSensitive(result, "gcode_end_byte");
    if (cJSON_IsNumber(start) && cJSON_IsNumber(end) && isfinite(start->valuedouble) &&
        isfinite(end->valuedouble) && start->valuedouble >= 0 &&
        end->valuedouble > start->valuedouble) {
        out->start_byte = start->valuedouble;
        out->end_byte = end->valuedouble;
    }
    cJSON_Delete(root);
    return out->end_byte >= 0 || out->filament_total > 0 || out->estimated_time > 0;
}

static void finish_request(estimate_request_t *request, const print_estimate_metadata_t *metadata)
{
    uint32_t generation = moonraker_config_generation();
    taskENTER_CRITICAL(&s_lock);
    if (metadata && request->key.generation == generation && same_key(&request->key)) {
        s_metadata = *metadata;
        s_valid = true;
    }
    s_busy = false;
    taskEXIT_CRITICAL(&s_lock);
    /* Don't leave the endpoint's key in a freed request buffer. */
    for (size_t i = 0; i < sizeof(request->api_key); ++i)
        ((volatile char *)request->api_key)[i] = 0;
    free(request);
}

static void fetch_metadata(void *argument)
{
    estimate_request_t *request = argument;
    print_estimate_metadata_t metadata;
    bool ok = false;
    if (request->key.generation == moonraker_config_generation()) {
        char *body = heap_caps_calloc(1, MOONRAKER_METADATA_CAPACITY,
            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!body) body = calloc(1, MOONRAKER_METADATA_CAPACITY);
        if (body) {
            char encoded[sizeof(request->key.file) * 3];
            thumbnail_manager_url_encode(request->key.file, encoded, sizeof(encoded));
            ok = moonraker_fetch_file_metadata(request->host, request->port, request->api_key,
                encoded, body, MOONRAKER_METADATA_CAPACITY, NULL, NULL) &&
                parse_metadata(body, &metadata);
            free(body);
        }
    }
    finish_request(request, ok ? &metadata : NULL);
    vTaskDelete(NULL);
}

void print_estimate_metadata_poll(const char *file, uint32_t revision, uint32_t generation, int64_t now_us)
{
    if (generation != moonraker_config_generation()) return;
    estimate_key_t key = {.generation = generation, .revision = revision};
    if (!file || !*file || !strcmp(file, "No file") || !strcmp(file, "--") ||
        strlen(file) >= sizeof(key.file)) return;
    memcpy(key.file, file, strlen(file) + 1);
    taskENTER_CRITICAL(&s_lock);
    if (!same_key(&key)) {
        s_key = key;
        s_valid = false;
        s_retry_at = 0;
    }
    if (s_busy || s_valid || now_us < s_retry_at) {
        taskEXIT_CRITICAL(&s_lock);
        return;
    }
    s_busy = true;
    s_retry_at = now_us + ESTIMATE_RETRY_US;
    taskEXIT_CRITICAL(&s_lock);

    estimate_request_t *request = calloc(1, sizeof(*request));
    if (!request) {
        taskENTER_CRITICAL(&s_lock);
        s_busy = false;
        taskEXIT_CRITICAL(&s_lock);
        return;
    }
    request->key = key;
    snprintf(request->host, sizeof(request->host), "%s", moonraker_config_host());
    snprintf(request->api_key, sizeof(request->api_key), "%s", moonraker_config_api_key());
    request->port = moonraker_config_port();
    if (!request->host[0] || request->port <= 0 || key.generation != moonraker_config_generation() ||
        xTaskCreatePinnedToCore(fetch_metadata, "estimate_meta", 8192, request, 4, NULL, 0) != pdPASS)
        finish_request(request, NULL);
}

bool print_estimate_metadata_get(const char *file, uint32_t revision, print_estimate_metadata_t *out)
{
    if (!file || !out) return false;
    uint32_t generation = moonraker_config_generation();
    taskENTER_CRITICAL(&s_lock);
    bool valid = s_valid && generation == s_key.generation && revision == s_key.revision &&
                 !strcmp(file, s_key.file);
    if (valid) *out = s_metadata;
    taskEXIT_CRITICAL(&s_lock);
    return valid;
}
