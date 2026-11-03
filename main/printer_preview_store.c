#include "printer_preview_store.h"

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

#include "moonraker_config_controller.h"
#include "printer_preview_cache.h"
#include "thumbnail_manager.h"

#define TAG "printer_preview_store"
#define STORE_DIR "/sdcard/hmi/profile_previews"
#define PREVIEW_WIDTH 286
#define PREVIEW_HEIGHT 215

static bool s_attempted[MOONRAKER_CONFIG_MAX_PROFILES];
static int s_next_profile = 0;
static uint32_t s_generation = 0;
static bool s_last_sd_available = false;


static bool valid_index(int index)
{
    return index >= 0 && index < MOONRAKER_CONFIG_MAX_PROFILES;
}


static void make_path(
    int profile_index,
    const char *suffix,
    char *out,
    size_t out_size)
{
    snprintf(
        out,
        out_size,
        STORE_DIR "/profile_%d.%s",
        profile_index,
        suffix);
}


/* Metadata is SD-card input. Reject incomplete/overlong lines before parsing. */
static bool read_metadata_line(FILE *input, char *text, size_t size)
{
    if (!fgets(text, size, input)) return false;
    size_t length = strlen(text);
    if (!length || text[length - 1] != '\n') return false;
    text[--length] = '\0';
    if (length && text[length - 1] == '\r') text[--length] = '\0';
    return true;
}

static bool read_preview_metadata(FILE *input,
    char *host, size_t host_size, int *port, char *file, size_t file_size)
{
    char magic[40], port_text[16];
    if (!read_metadata_line(input, magic, sizeof(magic)) ||
        strcmp(magic, "PRINTERHMI_PREVIEW_V1") ||
        !read_metadata_line(input, host, host_size) || !host[0] ||
        !read_metadata_line(input, port_text, sizeof(port_text)) ||
        !read_metadata_line(input, file, file_size) || !file[0] ||
        fgetc(input) != EOF || ferror(input)) return false;
    for (size_t i = 0; port_text[i]; ++i) {
        if (port_text[i] < '0' || port_text[i] > '9') return false;
    }
    errno = 0;
    char *end;
    long parsed = strtol(port_text, &end, 10);
    if (errno || end == port_text || *end || parsed < 1 || parsed > 65535) return false;
    *port = (int)parsed;
    return true;
}


static bool endpoint_matches(
    int profile_index,
    const char *host,
    int port)
{
    const moonraker_profile_t *profile =
        moonraker_config_profile(profile_index);

    return profile &&
           profile->configured &&
           host &&
           strcmp(profile->host, host) == 0 &&
           profile->port == port;
}


void printer_preview_store_reset_restore(void)
{
    memset(s_attempted, 0, sizeof(s_attempted));
    s_next_profile = 0;
    s_generation = moonraker_config_generation();
}


void printer_preview_store_invalidate(int profile_index)
{
    if (!valid_index(profile_index)) return;

    char path[96];
    char temporary[96];

    make_path(profile_index, "png", path, sizeof(path));
    remove(path);
    make_path(profile_index, "meta", path, sizeof(path));
    remove(path);
    make_path(profile_index, "png.tmp", temporary, sizeof(temporary));
    remove(temporary);
    make_path(profile_index, "meta.tmp", temporary, sizeof(temporary));
    remove(temporary);

    s_attempted[profile_index] = false;
}


bool printer_preview_store_store_png(
    int profile_index,
    const char *expected_host,
    int expected_port,
    const char *file,
    const uint8_t *png,
    size_t png_size)
{
    if (!valid_index(profile_index) ||
        !endpoint_matches(
            profile_index,
            expected_host,
            expected_port) ||
        !file || !file[0] || strnlen(file, 160) >= 160 ||
        strchr(file, '\n') || strchr(file, '\r') ||
        !png || png_size < 8 || png_size > 512 * 1024) {
        return false;
    }

    static const uint8_t png_signature[8] = {
        0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a
    };

    if (memcmp(png, png_signature, sizeof(png_signature)) != 0) {
        return false;
    }

    mkdir("/sdcard/hmi", 0775);
    mkdir(STORE_DIR, 0775);

    char png_path[96];
    char png_temporary[96];
    char meta_path[96];
    char meta_temporary[96];

    make_path(profile_index, "png", png_path, sizeof(png_path));
    make_path(
        profile_index,
        "png.tmp",
        png_temporary,
        sizeof(png_temporary));
    make_path(profile_index, "meta", meta_path, sizeof(meta_path));
    make_path(
        profile_index,
        "meta.tmp",
        meta_temporary,
        sizeof(meta_temporary));

    if (!thumbnail_manager_store_cache_file(
            png_temporary,
            png,
            png_size)) {
        return false;
    }

    FILE *metadata = fopen(meta_temporary, "wb");
    if (!metadata) {
        remove(png_temporary);
        return false;
    }

    int written = fprintf(
        metadata,
        "PRINTERHMI_PREVIEW_V1\n%s\n%d\n%s\n",
        expected_host,
        expected_port,
        file);

    int close_result = fclose(metadata);

    if (written <= 0 || close_result != 0) {
        remove(png_temporary);
        remove(meta_temporary);
        return false;
    }

    remove(png_path);
    if (rename(png_temporary, png_path) != 0) {
        remove(png_temporary);
        remove(meta_temporary);
        return false;
    }

    remove(meta_path);
    if (rename(meta_temporary, meta_path) != 0) {
        remove(meta_temporary);
        remove(png_path);
        return false;
    }

    s_attempted[profile_index] = true;

    ESP_LOGI(
        TAG,
        "Profile %d preview persisted: %s (%u bytes)",
        profile_index + 1,
        file,
        (unsigned)png_size);

    return true;
}


bool printer_preview_store_store_active(
    const char *file,
    const uint8_t *png,
    size_t png_size)
{
    int profile_index =
        moonraker_config_active_profile_index();

    const moonraker_profile_t *profile =
        moonraker_config_profile(profile_index);

    if (!profile || !profile->configured) return false;

    char host[MOONRAKER_CONFIG_HOST_LENGTH];
    strlcpy(host, profile->host, sizeof(host));
    int port = profile->port;

    return printer_preview_store_store_png(
        profile_index,
        host,
        port,
        file,
        png,
        png_size);
}


static bool restore_profile(int profile_index, bool *retry)
{
    *retry = false;
    /* Cache descriptors and profile fields belong to the display owner.
     * Keep SD I/O outside this lock, then recheck ownership before publishing. */
    if (!bsp_display_lock(50)) {
        *retry = true;
        return false;
    }
    const moonraker_profile_t *profile = moonraker_config_profile(profile_index);
    bool configured = profile && profile->configured;
    bool cached = configured && printer_preview_cache_image(profile_index, NULL, NULL);
    char expected_host[MOONRAKER_CONFIG_HOST_LENGTH] = "";
    int expected_port = 0;
    uint32_t generation = moonraker_config_generation();
    if (configured) {
        strlcpy(expected_host, profile->host, sizeof(expected_host));
        expected_port = profile->port;
    }
    bsp_display_unlock();
    if (!configured) return false;
    if (cached) return true;

    char meta_path[96];
    make_path(profile_index, "meta", meta_path, sizeof(meta_path));

    FILE *metadata = fopen(meta_path, "rb");
    if (!metadata) return false;

    char host[MOONRAKER_CONFIG_HOST_LENGTH + 1] = "";
    char file[161] = "";
    int port = 0;
    bool metadata_ok = read_preview_metadata(metadata,
        host, sizeof(host), &port, file, sizeof(file));
    fclose(metadata);

    if (!metadata_ok || strcmp(expected_host, host) || expected_port != port) {
        return false;
    }

    char png_path[96];
    make_path(profile_index, "png", png_path, sizeof(png_path));

    uint8_t *png = NULL;
    size_t png_size = 0;

    if (!thumbnail_manager_load_cache_file(
            png_path,
            &png,
            &png_size)) {
        return false;
    }

    bool installed = false;
    bool already_cached = false;
    if (bsp_display_lock(1000)) {
        if (generation == moonraker_config_generation()) {
            already_cached = printer_preview_cache_image(profile_index, NULL, NULL) != NULL;
            installed = already_cached || printer_preview_cache_publish_png(
                profile_index, host, port, file, png, png_size,
                PREVIEW_WIDTH, PREVIEW_HEIGHT);
        } else {
            *retry = true;
        }
        bsp_display_unlock();
    } else {
        *retry = true;
    }

    heap_caps_free(png);

    if (installed && !already_cached) {
        ESP_LOGI(
            TAG,
            "Profile %d preview restored: %s",
            profile_index + 1,
            file);
    }

    return installed;
}


void printer_preview_store_restore_one(bool sd_available)
{
    uint32_t generation = moonraker_config_generation();

    if (generation != s_generation ||
        (sd_available && !s_last_sd_available)) {
        printer_preview_store_reset_restore();
    }

    s_last_sd_available = sd_available;
    if (!sd_available) return;

    for (int count = 0;
         count < MOONRAKER_CONFIG_MAX_PROFILES;
         ++count) {
        int index = s_next_profile;
        s_next_profile =
            (s_next_profile + 1) % MOONRAKER_CONFIG_MAX_PROFILES;

        if (s_attempted[index]) continue;

        bool retry = false;
        restore_profile(index, &retry);
        s_attempted[index] = !retry;
        return;
    }
}
