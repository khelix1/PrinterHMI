#include "ota_release_catalog.h"

#include "cJSON.h"
#include "esp_app_desc.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RELEASE_API_URL \
    "https://api.github.com/repos/khelix1/PrinterHMI_v3_2/releases?per_page=30"
#define RELEASE_RESPONSE_MAX (128U * 1024U)

typedef struct {
    ota_release_entry_t entries[OTA_RELEASE_CATALOG_MAX];
    size_t count;
} release_catalog_t;

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
    bool overflow;
} release_capture_t;

static const char *TAG = "ota_release_catalog";
static SemaphoreHandle_t s_lock = NULL;
static release_catalog_t *s_catalog = NULL;
static ota_release_catalog_state_t s_state =
    OTA_RELEASE_CATALOG_IDLE;
static bool s_task_running = false;
static char *s_error = NULL;


static void ensure_lock(void)
{
    if (!s_lock) {
        /*
         * Create this only when Remote Builds is first opened. Keeping the
         * Heap-backed synchronization avoids consuming permanent .bss storage.
         */
        s_lock = xSemaphoreCreateMutexWithCaps(
            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
}


static void lock_catalog(void)
{
    ensure_lock();
    xSemaphoreTake(s_lock, portMAX_DELAY);
}


static void unlock_catalog(void)
{
    xSemaphoreGive(s_lock);
}


static void *psram_calloc(size_t count, size_t size)
{
    size_t bytes = count * size;
    void *memory = heap_caps_malloc(
        bytes,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (!memory) {
        memory = malloc(bytes);
    }

    if (memory) {
        memset(memory, 0, bytes);
    }

    return memory;
}


static void catalog_fail(const char *message)
{
    lock_catalog();

    if (!s_error) {
        s_error = psram_calloc(1, 128);
    }

    if (s_error) {
        snprintf(
            s_error,
            128,
            "%s",
            message ? message : "Release catalog failed");
    }

    s_state = OTA_RELEASE_CATALOG_ERROR;
    s_task_running = false;
    unlock_catalog();
}


static bool parse_semver(
    const char *text,
    int *major,
    int *minor,
    int *patch)
{
    if (!text || !major || !minor || !patch) {
        return false;
    }

    const char *version = text[0] == 'v' ? text + 1 : text;
    int consumed = 0;

    if (sscanf(
            version,
            "%d.%d.%d%n",
            major,
            minor,
            patch,
            &consumed) != 3) {
        return false;
    }

    return version[consumed] == '\0' &&
           *major >= 0 &&
           *minor >= 0 &&
           *patch >= 0;
}


static ota_release_relation_t compare_version(
    const char *candidate,
    const char *running)
{
    int ca = 0;
    int cb = 0;
    int cc = 0;
    int ra = 0;
    int rb = 0;
    int rc = 0;

    if (!parse_semver(candidate, &ca, &cb, &cc) ||
        !parse_semver(running, &ra, &rb, &rc)) {
        return OTA_RELEASE_STABLE;
    }

    if (ca != ra) {
        return ca > ra ? OTA_RELEASE_NEWER : OTA_RELEASE_OLDER;
    }
    if (cb != rb) {
        return cb > rb ? OTA_RELEASE_NEWER : OTA_RELEASE_OLDER;
    }
    if (cc != rc) {
        return cc > rc ? OTA_RELEASE_NEWER : OTA_RELEASE_OLDER;
    }

    return OTA_RELEASE_INSTALLED;
}


static void copy_json_string(
    char *destination,
    size_t capacity,
    const cJSON *value)
{
    if (!destination || capacity == 0) {
        return;
    }

    snprintf(
        destination,
        capacity,
        "%s",
        cJSON_IsString(value) && value->valuestring
            ? value->valuestring
            : "");
}


static void normalize_notes(char *notes)
{
    if (!notes) {
        return;
    }

    for (char *cursor = notes; *cursor; ++cursor) {
        if (*cursor == '\r' ||
            *cursor == '\n' ||
            *cursor == '\t') {
            *cursor = ' ';
        }
    }
}


static esp_err_t release_http_event(
    esp_http_client_event_t *event)
{
    if (!event ||
        event->event_id != HTTP_EVENT_ON_DATA ||
        !event->data ||
        event->data_len <= 0) {
        return ESP_OK;
    }

    release_capture_t *capture =
        (release_capture_t *)event->user_data;

    if (!capture || !capture->data || capture->overflow) {
        return ESP_OK;
    }

    size_t incoming = (size_t)event->data_len;
    if (capture->length + incoming + 1 > capture->capacity) {
        capture->overflow = true;
        return ESP_OK;
    }

    memcpy(
        capture->data + capture->length,
        event->data,
        incoming);
    capture->length += incoming;
    capture->data[capture->length] = '\0';
    return ESP_OK;
}


static bool valid_nightly_tag(const char *tag)
{
    if (!tag || strlen(tag) != 31 ||
        strncmp(tag, "nightly-", 8) != 0) {
        return false;
    }

    for (size_t index = 8; index < 31; ++index) {
        if (index == 12 || index == 15 || index == 18) {
            if (tag[index] != '-') {
                return false;
            }
            continue;
        }

        if (index < 19) {
            if (!isdigit((unsigned char)tag[index])) {
                return false;
            }
        } else if (!isxdigit((unsigned char)tag[index]) ||
                   isupper((unsigned char)tag[index])) {
            return false;
        }
    }

    return true;
}


static bool parse_releases(
    const char *json,
    release_catalog_t *catalog)
{
    if (!json || !catalog) {
        return false;
    }

    cJSON *root = cJSON_Parse(json);
    if (!cJSON_IsArray(root)) {
        cJSON_Delete(root);
        return false;
    }

    const esp_app_desc_t *app = esp_app_get_description();
    const char *running =
        app && app->version[0] ? app->version : "unknown";
    size_t stable_count = 0;
    size_t nightly_count = 0;

    int release_count = cJSON_GetArraySize(root);
    for (int index = 0;
         index < release_count &&
         catalog->count < OTA_RELEASE_CATALOG_MAX;
         ++index) {
        cJSON *release = cJSON_GetArrayItem(root, index);
        if (!cJSON_IsObject(release) ||
            cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(
                release, "draft"))) {
            continue;
        }

        cJSON *tag = cJSON_GetObjectItemCaseSensitive(
            release, "tag_name");
        if (!cJSON_IsString(tag) || !tag->valuestring) {
            continue;
        }

        bool prerelease = cJSON_IsTrue(
            cJSON_GetObjectItemCaseSensitive(
                release, "prerelease"));
        cJSON *release_body =
            cJSON_GetObjectItemCaseSensitive(release, "body");
        char nightly_identity[80];
        snprintf(
            nightly_identity,
            sizeof(nightly_identity),
            "Firmware identity: %s",
            tag->valuestring);
        bool identity_aware_nightly =
            cJSON_IsString(release_body) &&
            release_body->valuestring &&
            strstr(
                release_body->valuestring,
                nightly_identity) != NULL;
        int major = 0;
        int minor = 0;
        int patch = 0;
        bool stable =
            !prerelease &&
            tag->valuestring[0] == 'v' &&
            parse_semver(
                tag->valuestring,
                &major,
                &minor,
                &patch);
        bool nightly =
            prerelease &&
            valid_nightly_tag(tag->valuestring) &&
            identity_aware_nightly;

        if ((!stable && !nightly) ||
            (stable &&
             stable_count >=
                 OTA_RELEASE_CATALOG_MAX_PER_CHANNEL) ||
            (nightly &&
             nightly_count >=
                 OTA_RELEASE_CATALOG_MAX_PER_CHANNEL)) {
            continue;
        }

        char expected_asset[112];
        snprintf(
            expected_asset,
            sizeof(expected_asset),
            stable
                ? "PrinterHMI-%s-ota.bin"
                : "PrinterHMI-%s.bin",
            tag->valuestring);

        cJSON *assets = cJSON_GetObjectItemCaseSensitive(
            release, "assets");
        cJSON *matched_asset = NULL;
        int asset_count = cJSON_IsArray(assets)
            ? cJSON_GetArraySize(assets)
            : 0;

        for (int asset_index = 0;
             asset_index < asset_count;
             ++asset_index) {
            cJSON *asset = cJSON_GetArrayItem(
                assets, asset_index);
            cJSON *name = cJSON_GetObjectItemCaseSensitive(
                asset, "name");

            if (cJSON_IsString(name) &&
                name->valuestring &&
                strcmp(
                    name->valuestring,
                    expected_asset) == 0) {
                matched_asset = asset;
                break;
            }
        }

        if (!matched_asset) {
            continue;
        }

        cJSON *url = cJSON_GetObjectItemCaseSensitive(
            matched_asset, "browser_download_url");
        if (!cJSON_IsString(url) ||
            !url->valuestring ||
            !url->valuestring[0]) {
            continue;
        }

        ota_release_entry_t *entry =
            &catalog->entries[catalog->count];

        copy_json_string(
            entry->version,
            sizeof(entry->version),
            tag);
        copy_json_string(
            entry->name,
            sizeof(entry->name),
            cJSON_GetObjectItemCaseSensitive(
                release, "name"));
        copy_json_string(
            entry->asset_url,
            sizeof(entry->asset_url),
            url);
        copy_json_string(
            entry->notes,
            sizeof(entry->notes),
            cJSON_GetObjectItemCaseSensitive(
                release, "body"));
        normalize_notes(entry->notes);

        cJSON *published = cJSON_GetObjectItemCaseSensitive(
            release, "published_at");
        if (cJSON_IsString(published) &&
            published->valuestring &&
            strlen(published->valuestring) >= 16) {
            snprintf(
                entry->published,
                sizeof(entry->published),
                "%.10s %.5s UTC",
                published->valuestring,
                published->valuestring + 11);
        }

        cJSON *size = cJSON_GetObjectItemCaseSensitive(
            matched_asset, "size");
        if (cJSON_IsNumber(size)) {
            entry->asset_size = (int64_t)size->valuedouble;
        }

        if (stable) {
            entry->channel = OTA_RELEASE_CHANNEL_STABLE;
            entry->relation = compare_version(
                entry->version,
                running);
            ++stable_count;
        } else {
            entry->channel = OTA_RELEASE_CHANNEL_NIGHTLY;
            entry->relation =
                strcmp(entry->version, running) == 0
                    ? OTA_RELEASE_INSTALLED
                    : OTA_RELEASE_DEVELOPMENT;
            ++nightly_count;
        }

        ++catalog->count;
    }

    cJSON_Delete(root);
    return true;
}


static void release_catalog_task(void *argument)
{
    (void)argument;

    release_catalog_t *catalog =
        psram_calloc(1, sizeof(*catalog));
    char *response =
        psram_calloc(1, RELEASE_RESPONSE_MAX);

    if (!catalog || !response) {
        free(catalog);
        free(response);
        catalog_fail("Not enough memory for release catalog");
        vTaskDeleteWithCaps(NULL);
        return;
    }

    release_capture_t capture = {
        .data = response,
        .length = 0,
        .capacity = RELEASE_RESPONSE_MAX,
        .overflow = false,
    };

    esp_http_client_config_t config = {
        .url = RELEASE_API_URL,
        .timeout_ms = 15000,
        .keep_alive_enable = true,
        .event_handler = release_http_event,
        .user_data = &capture,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client =
        esp_http_client_init(&config);
    if (!client) {
        free(catalog);
        free(response);
        catalog_fail("Could not create GitHub request");
        vTaskDeleteWithCaps(NULL);
        return;
    }

    esp_http_client_set_header(
        client,
        "Accept",
        "application/vnd.github+json");
    esp_http_client_set_header(
        client,
        "X-GitHub-Api-Version",
        "2022-11-28");
    esp_http_client_set_header(
        client,
        "User-Agent",
        "PrinterHMI");

    esp_err_t result = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (result != ESP_OK || status != 200) {
        char message[128];
        snprintf(
            message,
            sizeof(message),
            "GitHub request failed (%d: %s)",
            status,
            esp_err_to_name(result));
        free(catalog);
        free(response);
        catalog_fail(message);
        vTaskDeleteWithCaps(NULL);
        return;
    }

    if (capture.overflow) {
        free(catalog);
        free(response);
        catalog_fail("GitHub response exceeded 128 KiB");
        vTaskDeleteWithCaps(NULL);
        return;
    }

    if (!parse_releases(response, catalog)) {
        free(catalog);
        free(response);
        catalog_fail("GitHub returned an invalid release catalog");
        vTaskDeleteWithCaps(NULL);
        return;
    }

    free(response);

    release_catalog_t *old_catalog = NULL;
    lock_catalog();
    old_catalog = s_catalog;
    s_catalog = catalog;
    if (s_error) {
        s_error[0] = '\0';
    }
    s_state = OTA_RELEASE_CATALOG_READY;
    s_task_running = false;
    size_t ready_count = catalog->count;
    unlock_catalog();
    free(old_catalog);

    ESP_LOGI(
        TAG,
        "Stable releases ready: %u",
        (unsigned)ready_count);
    vTaskDeleteWithCaps(NULL);
}


bool ota_release_catalog_start(void)
{
    lock_catalog();
    if (s_task_running) {
        unlock_catalog();
        return true;
    }

    s_task_running = true;
    s_state = OTA_RELEASE_CATALOG_LOADING;
    if (s_error) {
        s_error[0] = '\0';
    }
    unlock_catalog();

    /*
     * The catalog parser needs a useful stack, but not scarce internal RAM.
     * ESP-IDF keeps the small TCB internal and places this stack in PSRAM.
     */
    BaseType_t created = xTaskCreateWithCaps(
        release_catalog_task,
        "ota_releases",
        8192,
        NULL,
        5,
        NULL,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (created != pdPASS) {
        catalog_fail("Could not start release catalog task");
        return false;
    }

    return true;
}


void ota_release_catalog_snapshot(
    ota_release_catalog_snapshot_t *snapshot)
{
    if (!snapshot) {
        return;
    }

    memset(snapshot, 0, sizeof(*snapshot));

    lock_catalog();
    snapshot->state = s_state;
    snapshot->count = s_catalog ? s_catalog->count : 0;
    snprintf(
        snapshot->error,
        sizeof(snapshot->error),
        "%s",
        s_error ? s_error : "");
    unlock_catalog();
}


bool ota_release_catalog_entry(
    size_t index,
    ota_release_entry_t *entry)
{
    if (!entry) {
        return false;
    }

    bool found = false;
    lock_catalog();

    if (s_catalog && index < s_catalog->count) {
        *entry = s_catalog->entries[index];
        found = true;
    }

    unlock_catalog();
    return found;
}
