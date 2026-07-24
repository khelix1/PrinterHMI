#include "printer_preview_cache_v32.h"

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

#define TAG "printer_preview_cache"

static preview_slot_t *s_slots = NULL;


static bool valid_index(int index)
{
    return index >= 0 && index < MOONRAKER_CONFIG_MAX_PROFILES;
}


static bool ensure_slots(void)
{
    if (s_slots) return true;

    s_slots = heap_caps_calloc(
        MOONRAKER_CONFIG_MAX_PROFILES,
        sizeof(preview_slot_t),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (!s_slots) {
        s_slots = heap_caps_calloc(
            MOONRAKER_CONFIG_MAX_PROFILES,
            sizeof(preview_slot_t),
            MALLOC_CAP_8BIT);
    }

    if (!s_slots) {
        ESP_LOGE(TAG, "Preview slot allocation failed");
        return false;
    }

    return true;
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

    if (!ensure_slots()) return false;

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
    if (!ensure_slots()) return false;

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
    if (!ensure_slots()) return NULL;

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
    if (!valid_index(profile_index) || !s_slots) return;

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
    if (!s_slots) return;

    for (int index = 0; index < MOONRAKER_CONFIG_MAX_PROFILES; ++index) {
        preview_slot_t *slot = &s_slots[index];

        if (slot->pixels) {
            heap_caps_free(slot->pixels);
        }

        memset(slot, 0, sizeof(*slot));
    }
}
