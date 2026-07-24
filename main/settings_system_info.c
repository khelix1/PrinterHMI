#include "settings_system_info.h"

#include "esp_heap_caps.h"
#include "esp_idf_version.h"
#include "esp_timer.h"

#include <stdio.h>

static lv_obj_t *s_idf_label = NULL;
static lv_obj_t *s_heap_label = NULL;
static lv_obj_t *s_psram_label = NULL;
static lv_obj_t *s_uptime_label = NULL;

static void format_bytes(
    char *buf,
    size_t buf_size,
    size_t bytes)
{
    if (!buf || buf_size == 0) {
        return;
    }

    if (bytes >= (1024U * 1024U)) {
        snprintf(
            buf,
            buf_size,
            "%.1f MB",
            (double)bytes / (1024.0 * 1024.0));
    } else if (bytes >= 1024U) {
        snprintf(
            buf,
            buf_size,
            "%.1f KB",
            (double)bytes / 1024.0);
    } else {
        snprintf(
            buf,
            buf_size,
            "%u B",
            (unsigned)bytes);
    }
}

static void format_uptime(
    char *buf,
    size_t buf_size,
    int64_t uptime_seconds)
{
    if (!buf || buf_size == 0) {
        return;
    }

    int64_t days = uptime_seconds / 86400;
    int64_t hours = (uptime_seconds % 86400) / 3600;
    int64_t minutes = (uptime_seconds % 3600) / 60;
    int64_t seconds = uptime_seconds % 60;

    if (days > 0) {
        snprintf(
            buf,
            buf_size,
            "%lldd %02lldh %02lldm",
            (long long)days,
            (long long)hours,
            (long long)minutes);
    } else if (hours > 0) {
        snprintf(
            buf,
            buf_size,
            "%02lldh %02lldm %02llds",
            (long long)hours,
            (long long)minutes,
            (long long)seconds);
    } else {
        snprintf(
            buf,
            buf_size,
            "%02lldm %02llds",
            (long long)minutes,
            (long long)seconds);
    }
}

const char *settings_system_info_idf_version(void)
{
    return esp_get_idf_version();
}

void settings_system_info_bind_idf_label(lv_obj_t *label)
{
    s_idf_label = label;
}

void settings_system_info_bind_heap_label(lv_obj_t *label)
{
    s_heap_label = label;
}

void settings_system_info_bind_psram_label(lv_obj_t *label)
{
    s_psram_label = label;
}

void settings_system_info_bind_uptime_label(lv_obj_t *label)
{
    s_uptime_label = label;
}

void settings_system_info_refresh(void)
{
    char heap_buf[32];
    char psram_buf[32];
    char uptime_buf[48];

    format_bytes(
        heap_buf,
        sizeof(heap_buf),
        heap_caps_get_free_size(MALLOC_CAP_INTERNAL));

    format_bytes(
        psram_buf,
        sizeof(psram_buf),
        heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    format_uptime(
        uptime_buf,
        sizeof(uptime_buf),
        esp_timer_get_time() / 1000000LL);

    if (s_idf_label) {
        lv_label_set_text(
            s_idf_label,
            settings_system_info_idf_version());
    }

    if (s_heap_label) {
        lv_label_set_text(
            s_heap_label,
            heap_buf);
    }

    if (s_psram_label) {
        lv_label_set_text(
            s_psram_label,
            psram_buf);
    }

    if (s_uptime_label) {
        lv_label_set_text(
            s_uptime_label,
            uptime_buf);
    }
}

void settings_system_info_unbind(void)
{
    s_idf_label = NULL;
    s_heap_label = NULL;
    s_psram_label = NULL;
    s_uptime_label = NULL;
}
