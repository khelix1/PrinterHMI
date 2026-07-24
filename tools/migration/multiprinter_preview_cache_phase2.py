#!/usr/bin/env python3
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
MAIN = ROOT / "main"


def read(name: str) -> str:
    path = MAIN / name
    if not path.exists():
        raise RuntimeError(f"missing required file: {path}")
    return path.read_text()


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"expected one {label}, found {count}")
    return text.replace(old, new, 1)


cache_h = r'''#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lvgl.h"

/*
 * Profile-indexed rendered preview cache.
 *
 * The active pipeline publishes RGB565. The inactive-printer worker publishes
 * downloaded PNG data through the same renderer. UI consumers only see a
 * completed, profile-validated image descriptor.
 *
 * Publish functions must be called while the LVGL/display lock is held.
 */
bool printer_preview_cache_v32_publish_active(
    const char *file,
    const uint16_t *pixels,
    int width,
    int height);

bool printer_preview_cache_v32_publish_png(
    int profile_index,
    const char *expected_host,
    int expected_port,
    const char *file,
    const uint8_t *png,
    size_t png_size,
    int width,
    int height);

bool printer_preview_cache_v32_matches(
    int profile_index,
    const char *file);

const lv_image_dsc_t *printer_preview_cache_v32_image(
    int profile_index,
    const char **file_out,
    uint32_t *revision_out);

void printer_preview_cache_v32_invalidate(int profile_index);
void printer_preview_cache_v32_reset(void);
'''


cache_c = r'''#include "printer_preview_cache_v32.h"

#include <stdio.h>
#include <string.h>

#include "esp_heap_caps.h"
#include "esp_log.h"

#include "moonraker_config_controller.h"
#include "thumbnail_render_v32.h"

typedef struct {
    uint16_t *pixels;
    size_t pixel_capacity;
    int width;
    int height;
    char file[160];
    char host[MOONRAKER_CONFIG_HOST_LENGTH];
    int port;
    uint32_t revision;
    bool ready;
    lv_image_dsc_t image;
} preview_slot_t;

static const char *TAG = "printer_preview_cache";

static preview_slot_t
    s_slots[MOONRAKER_CONFIG_MAX_PROFILES];


static bool valid_index(int index)
{
    return index >= 0 && index < MOONRAKER_CONFIG_MAX_PROFILES;
}


static bool endpoint_matches(
    const moonraker_profile_t *profile,
    const char *host,
    int port)
{
    return profile &&
           profile->configured &&
           host &&
           strcmp(profile->host, host) == 0 &&
           profile->port == port;
}


static bool slot_matches_profile(
    const preview_slot_t *slot,
    const moonraker_profile_t *profile)
{
    return slot &&
           endpoint_matches(profile, slot->host, slot->port);
}


static uint16_t *allocate_pixels(size_t count)
{
    if (count == 0) return NULL;

    uint16_t *pixels = heap_caps_malloc(
        count * sizeof(uint16_t),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (!pixels) {
        pixels = heap_caps_malloc(
            count * sizeof(uint16_t),
            MALLOC_CAP_8BIT);
    }

    return pixels;
}


static bool install_pixels(
    int profile_index,
    const char *host,
    int port,
    const char *file,
    uint16_t *replacement,
    size_t count,
    int width,
    int height)
{
    if (!valid_index(profile_index) ||
        !host || !host[0] ||
        !file || !file[0] ||
        !replacement ||
        count == 0 ||
        width <= 0 ||
        height <= 0) {
        return false;
    }

    const moonraker_profile_t *profile =
        moonraker_config_profile(profile_index);

    if (!endpoint_matches(profile, host, port)) {
        ESP_LOGW(TAG, "Discarded stale preview for profile %d", profile_index + 1);
        return false;
    }

    preview_slot_t *slot = &s_slots[profile_index];

    if (slot->pixels) {
        heap_caps_free(slot->pixels);
    }

    slot->pixels = replacement;
    slot->pixel_capacity = count;
    slot->width = width;
    slot->height = height;
    snprintf(slot->file, sizeof(slot->file), "%s", file);
    snprintf(slot->host, sizeof(slot->host), "%s", host);
    slot->port = port;
    slot->ready = true;
    slot->revision++;

    if (slot->revision == 0) slot->revision = 1;

    memset(&slot->image, 0, sizeof(slot->image));
#if defined(LV_IMAGE_HEADER_MAGIC)
    slot->image.header.magic = LV_IMAGE_HEADER_MAGIC;
#endif
    slot->image.header.cf = LV_COLOR_FORMAT_RGB565;
    slot->image.header.w = width;
    slot->image.header.h = height;
    slot->image.header.stride = width * sizeof(uint16_t);
    slot->image.data_size = count * sizeof(uint16_t);
    slot->image.data = (const uint8_t *)slot->pixels;

    ESP_LOGI(
        TAG,
        "Profile %d preview cached: %s %dx%d rev=%u",
        profile_index + 1,
        slot->file,
        width,
        height,
        (unsigned)slot->revision);

    return true;
}


bool printer_preview_cache_v32_publish_active(
    const char *file,
    const uint16_t *pixels,
    int width,
    int height)
{
    if (!file || !file[0] || !pixels || width <= 0 || height <= 0) {
        return false;
    }

    int index = moonraker_config_active_profile_index();
    const moonraker_profile_t *profile = moonraker_config_profile(index);

    if (!valid_index(index) || !profile || !profile->configured) {
        return false;
    }

    size_t count = (size_t)width * (size_t)height;
    uint16_t *replacement = allocate_pixels(count);

    if (!replacement) {
        ESP_LOGW(TAG, "Profile %d RGB565 allocation failed", index + 1);
        return false;
    }

    memcpy(replacement, pixels, count * sizeof(uint16_t));

    if (!install_pixels(
            index,
            profile->host,
            profile->port,
            file,
            replacement,
            count,
            width,
            height)) {
        heap_caps_free(replacement);
        return false;
    }

    return true;
}


bool printer_preview_cache_v32_publish_png(
    int profile_index,
    const char *expected_host,
    int expected_port,
    const char *file,
    const uint8_t *png,
    size_t png_size,
    int width,
    int height)
{
    if (!valid_index(profile_index) ||
        !expected_host || !expected_host[0] ||
        !file || !file[0] ||
        !png || png_size == 0 ||
        width <= 0 || height <= 0) {
        return false;
    }

    const moonraker_profile_t *profile =
        moonraker_config_profile(profile_index);

    if (!endpoint_matches(profile, expected_host, expected_port)) {
        return false;
    }

    size_t count = (size_t)width * (size_t)height;
    uint16_t *replacement = allocate_pixels(count);

    if (!replacement) return false;

    lv_image_dsc_t raw_png;
    memset(&raw_png, 0, sizeof(raw_png));
#if defined(LV_IMAGE_HEADER_MAGIC)
    raw_png.header.magic = LV_IMAGE_HEADER_MAGIC;
#endif
    raw_png.header.cf = LV_COLOR_FORMAT_RAW;
    raw_png.data = png;
    raw_png.data_size = png_size;

    bool rendered = thumbnail_render_v32_to_rgb565(
        &raw_png,
        replacement,
        width,
        height);

    if (!rendered ||
        !install_pixels(
            profile_index,
            expected_host,
            expected_port,
            file,
            replacement,
            count,
            width,
            height)) {
        heap_caps_free(replacement);
        return false;
    }

    return true;
}


bool printer_preview_cache_v32_matches(
    int profile_index,
    const char *file)
{
    if (!valid_index(profile_index) || !file || !file[0]) return false;

    preview_slot_t *slot = &s_slots[profile_index];
    const moonraker_profile_t *profile =
        moonraker_config_profile(profile_index);

    return slot->ready &&
           slot->pixels &&
           slot_matches_profile(slot, profile) &&
           strcmp(slot->file, file) == 0;
}


const lv_image_dsc_t *printer_preview_cache_v32_image(
    int profile_index,
    const char **file_out,
    uint32_t *revision_out)
{
    if (file_out) *file_out = NULL;
    if (revision_out) *revision_out = 0;

    if (!valid_index(profile_index)) return NULL;

    preview_slot_t *slot = &s_slots[profile_index];
    const moonraker_profile_t *profile =
        moonraker_config_profile(profile_index);

    if (!slot->ready ||
        !slot->pixels ||
        !slot_matches_profile(slot, profile)) {
        return NULL;
    }

    if (file_out) *file_out = slot->file;
    if (revision_out) *revision_out = slot->revision;
    return &slot->image;
}


void printer_preview_cache_v32_invalidate(int profile_index)
{
    if (!valid_index(profile_index)) return;

    preview_slot_t *slot = &s_slots[profile_index];
    if (!slot->ready) return;

    slot->ready = false;
    slot->file[0] = '\0';
    slot->host[0] = '\0';
    slot->port = 0;
    slot->revision++;
}


void printer_preview_cache_v32_reset(void)
{
    for (int index = 0; index < MOONRAKER_CONFIG_MAX_PROFILES; ++index) {
        preview_slot_t *slot = &s_slots[index];

        if (slot->pixels) {
            heap_caps_free(slot->pixels);
        }

        memset(slot, 0, sizeof(*slot));
    }
}
'''


worker_h = r'''#pragma once

/*
 * Refresh one inactive printer's current-print preview.
 * Called by the existing application runtime worker; never creates a task.
 */
void printer_profile_preview_worker_v32_poll_one(const char *api_key);
void printer_profile_preview_worker_v32_reset(void);
'''


worker_c = r'''#include "printer_profile_preview_worker_v32.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bsp/display.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

#include "moonraker.h"
#include "moonraker_config_controller.h"
#include "printer_profile_health.h"
#include "printer_preview_cache_v32.h"

#define PROFILE_PREVIEW_WIDTH  286
#define PROFILE_PREVIEW_HEIGHT 215

static const char *TAG = "profile_preview_worker";

static int s_next_profile = 0;
static uint32_t s_generation = 0;


static void url_encode(
    const char *input,
    char *output,
    size_t output_size)
{
    static const char hex[] = "0123456789ABCDEF";

    if (!output || output_size == 0) return;

    size_t out = 0;

    for (size_t in = 0;
         input && input[in] && out + 4 < output_size;
         ++in) {
        unsigned char c = (unsigned char)input[in];

        if ((c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' ||
            c == '~' || c == '/') {
            output[out++] = (char)c;
        } else {
            output[out++] = '%';
            output[out++] = hex[(c >> 4) & 0x0f];
            output[out++] = hex[c & 0x0f];
        }
    }

    output[out] = '\0';
}


static bool state_has_current_print(const char *state)
{
    return state &&
           (strcmp(state, "printing") == 0 ||
            strcmp(state, "paused") == 0);
}


void printer_profile_preview_worker_v32_reset(void)
{
    s_next_profile = 0;
    s_generation = moonraker_config_generation();
}


void printer_profile_preview_worker_v32_poll_one(const char *api_key)
{
    uint32_t generation_before = moonraker_config_generation();

    if (generation_before != s_generation) {
        printer_profile_preview_worker_v32_reset();
        generation_before = s_generation;
    }

    int index = s_next_profile;
    s_next_profile =
        (s_next_profile + 1) % MOONRAKER_CONFIG_MAX_PROFILES;

    if (index == moonraker_config_active_profile_index()) {
        return;
    }

    const moonraker_profile_t *profile =
        moonraker_config_profile(index);

    if (!profile || !profile->configured) {
        printer_profile_health_set(index, true, false);
        printer_preview_cache_v32_invalidate(index);
        return;
    }

    char host[MOONRAKER_CONFIG_HOST_LENGTH];
    strlcpy(host, profile->host, sizeof(host));
    int port = profile->port;

    char stats[2048] = {0};
    int http_code = 0;
    esp_err_t error = ESP_FAIL;

    bool online = moonraker_fetch_print_stats(
        host,
        port,
        api_key,
        stats,
        sizeof(stats),
        &http_code,
        &error);

    if (generation_before != moonraker_config_generation()) return;

    printer_profile_health_set(index, true, online);

    if (!online) {
        return;
    }

    char state[32] = "";
    char file[160] = "";

    const char *print_stats = strstr(stats, "\"print_stats\"");
    if (!print_stats) print_stats = stats;

    json_find_string(print_stats, "state", state, sizeof(state));
    json_find_string(print_stats, "filename", file, sizeof(file));

    if (!state_has_current_print(state) || !file[0]) {
        printer_preview_cache_v32_invalidate(index);
        return;
    }

    if (printer_preview_cache_v32_matches(index, file)) {
        return;
    }

    char encoded_file[384];
    url_encode(file, encoded_file, sizeof(encoded_file));

    char *metadata = heap_caps_calloc(
        1,
        8192,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (!metadata) {
        metadata = heap_caps_calloc(1, 8192, MALLOC_CAP_8BIT);
    }

    if (!metadata) return;

    bool metadata_ok = moonraker_fetch_file_metadata(
        host,
        port,
        api_key,
        encoded_file,
        metadata,
        8192,
        &http_code,
        &error);

    if (generation_before != moonraker_config_generation()) {
        heap_caps_free(metadata);
        return;
    }

    char thumbnail_path[256] = "";

    bool path_ok = metadata_ok &&
        json_find_best_thumbnail_path(
            metadata,
            thumbnail_path,
            sizeof(thumbnail_path));

    heap_caps_free(metadata);

    if (!path_ok) {
        ESP_LOGW(TAG, "Profile %d has no thumbnail for %s", index + 1, file);
        return;
    }

    char encoded_thumbnail[512];
    url_encode(
        thumbnail_path,
        encoded_thumbnail,
        sizeof(encoded_thumbnail));

    uint8_t *png = NULL;
    size_t png_size = 0;

    if (!moonraker_fetch_thumbnail_encoded(
            host,
            port,
            encoded_thumbnail,
            &png,
            &png_size)) {
        ESP_LOGW(TAG, "Profile %d thumbnail download failed", index + 1);
        return;
    }

    if (generation_before != moonraker_config_generation()) {
        heap_caps_free(png);
        return;
    }

    bool installed = false;

    if (bsp_display_lock(1000)) {
        installed = printer_preview_cache_v32_publish_png(
            index,
            host,
            port,
            file,
            png,
            png_size,
            PROFILE_PREVIEW_WIDTH,
            PROFILE_PREVIEW_HEIGHT);

        bsp_display_unlock();
    }

    heap_caps_free(png);

    ESP_LOGI(
        TAG,
        "Profile %d preview refresh %s: %s",
        index + 1,
        installed ? "complete" : "failed",
        file);
}
'''


health_h = read("printer_profile_health.h")
if "printer_profile_health_set" not in health_h:
    health_h += r'''

/* Updated by the combined inactive-printer status/preview worker. */
void printer_profile_health_set(
    int profile_index,
    bool known,
    bool online);
'''


health_c = read("printer_profile_health.c")
if "void printer_profile_health_set(" not in health_c:
    health_c += r'''


void printer_profile_health_set(
    int profile_index,
    bool known,
    bool online)
{
    if (profile_index < 0 ||
        profile_index >= MOONRAKER_CONFIG_MAX_PROFILES) {
        return;
    }

    bool changed =
        s_known[profile_index] != known ||
        (known && s_online[profile_index] != online);

    s_known[profile_index] = known;
    s_online[profile_index] = online;

    if (changed && known) {
        ESP_LOGI(
            TAG,
            "Profile %d is %s",
            profile_index + 1,
            online ? "online" : "offline");
    }
}
'''


moon_h = read("moonraker.h")
if "moonraker_fetch_print_stats" not in moon_h:
    anchor = r'''bool moonraker_fetch_file_list(const char *host,
                               int port,
                               const char *api_key,
                               char *body,
                               size_t body_sz,
                               int *http_code,
                               esp_err_t *err_out);
'''

    addition = anchor + r'''

bool moonraker_fetch_print_stats(const char *host,
                                 int port,
                                 const char *api_key,
                                 char *body,
                                 size_t body_sz,
                                 int *http_code,
                                 esp_err_t *err_out);
'''

    moon_h = replace_once(
        moon_h,
        anchor,
        addition,
        "Moonraker file-list declaration")


moon_c = read("moonraker.c")
if "bool moonraker_fetch_print_stats(" not in moon_c:
    anchor = r'''bool moonraker_fetch_file_list(const char *host,
                               int port,
                               const char *api_key,
                               char *body,
                               size_t body_sz,
                               int *http_code,
                               esp_err_t *err_out)
{
    return moonraker_http_get_text(
        host,
        port,
        api_key,
        "/server/files/list?root=gcodes",
        2500,
        body,
        body_sz,
        http_code,
        err_out);
}
'''

    addition = anchor + r'''


bool moonraker_fetch_print_stats(const char *host,
                                 int port,
                                 const char *api_key,
                                 char *body,
                                 size_t body_sz,
                                 int *http_code,
                                 esp_err_t *err_out)
{
    return moonraker_http_get_text(
        host,
        port,
        api_key,
        "/printer/objects/query?print_stats",
        1200,
        body,
        body_sz,
        http_code,
        err_out);
}
'''

    moon_c = replace_once(
        moon_c,
        anchor,
        addition,
        "Moonraker file-list implementation")


cmake = read("CMakeLists.txt")
if '"printer_profile_preview_worker_v32.c"' not in cmake:
    cmake = replace_once(
        cmake,
        '        "printer_preview_cache_v32.c"\n',
        '        "printer_preview_cache_v32.c"\n'
        '        "printer_profile_preview_worker_v32.c"\n',
        "preview-cache CMake registration")


main = read("main.c")
if '#include "printer_profile_preview_worker_v32.h"' not in main:
    main = replace_once(
        main,
        '#include "printer_preview_cache_v32.h"\n',
        '#include "printer_preview_cache_v32.h"\n'
        '#include "printer_profile_preview_worker_v32.h"\n',
        "preview-cache include")

main = replace_once(
    main,
    r'''        moonraker_live_poll_tasklet();
        printer_profile_health_poll_one();
        ESP_LOGI(TAG, "RUNTIME_LOOP: after moonraker");
''',
    r'''        moonraker_live_poll_tasklet();
        printer_profile_preview_worker_v32_poll_one(
            MOONRAKER_API_KEY);
        ESP_LOGI(TAG, "RUNTIME_LOOP: after moonraker");
''',
    "runtime health poll")

bridge_old = r'''    printer_profile_health_reset();
    ui_printer_chooser_v32_refresh();
'''

bridge_new = r'''    printer_profile_health_reset();
    printer_profile_preview_worker_v32_reset();
    ui_printer_chooser_v32_refresh();
'''

if "printer_profile_preview_worker_v32_reset();" not in main:
    main = replace_once(
        main,
        bridge_old,
        bridge_new,
        "profile-switch preview reset")


(MAIN / "printer_preview_cache_v32.h").write_text(cache_h)
(MAIN / "printer_preview_cache_v32.c").write_text(cache_c)
(MAIN / "printer_profile_preview_worker_v32.h").write_text(worker_h)
(MAIN / "printer_profile_preview_worker_v32.c").write_text(worker_c)
(MAIN / "printer_profile_health.h").write_text(health_h)
(MAIN / "printer_profile_health.c").write_text(health_c)
(MAIN / "moonraker.h").write_text(moon_h)
(MAIN / "moonraker.c").write_text(moon_c)
(MAIN / "CMakeLists.txt").write_text(cmake)
(MAIN / "main.c").write_text(main)

print("PASS: automatic inactive-printer preview refresh installed")
print("  - one inactive profile checked per existing runtime loop")
print("  - unchanged filenames do not redownload or decode")
print("  - changed live prints refresh metadata, PNG, and RGB565 cache")
print("  - no new task, timer, or scheduler allocation")
print("Next: idf.py build")
