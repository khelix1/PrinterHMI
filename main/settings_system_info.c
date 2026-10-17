#include "settings_system_info.h"
#include "ui_value_update.h"

#include "esp_heap_caps.h"
#include "esp_idf_version.h"
#include "esp_timer.h"

#include <stdio.h>

static lv_obj_t *s_idf_label = NULL;
static lv_obj_t *s_heap_label = NULL;
static lv_obj_t *s_psram_label = NULL;
static lv_obj_t *s_uptime_label = NULL;
static lv_obj_t *s_heap_metrics[3], *s_psram_metrics[2];
static uint32_t s_info_epoch;
static bool s_info_refreshing;
static void info_field_deleted(lv_event_t *event)
{
    lv_obj_t **slot=lv_event_get_user_data(event);
    if(*slot==lv_event_get_target_obj(event)){*slot=NULL;++s_info_epoch;}
}
static void info_bind(lv_obj_t **slot,lv_obj_t *label)
{
    if(*slot==label)return;
    ++s_info_epoch;*slot=label;
    if(label)lv_obj_add_event_cb(label,info_field_deleted,LV_EVENT_DELETE,slot);
}

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
            "%.1f MiB",
            (double)bytes / (1024.0 * 1024.0));
    } else if (bytes >= 1024U) {
        snprintf(
            buf,
            buf_size,
            "%.1f KiB",
            (double)bytes / 1024.0);
    } else {
        snprintf(
            buf,
            buf_size,
            "%u B",
            (unsigned)bytes);
    }
}

static void format_memory_health(
    char *buf,
    size_t buf_size,
    size_t current,
    size_t minimum,
    size_t largest)
{
    char current_buf[16];
    char minimum_buf[16];
    char largest_buf[16];

    format_bytes(current_buf, sizeof(current_buf), current);
    format_bytes(minimum_buf, sizeof(minimum_buf), minimum);
    if (largest > 0) {
        format_bytes(largest_buf, sizeof(largest_buf), largest);
        snprintf(buf, buf_size, "%s / %s / %s", current_buf, minimum_buf, largest_buf);
    } else {
        snprintf(buf, buf_size, "%s / %s", current_buf, minimum_buf);
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
    info_bind(&s_idf_label,label);
}

void settings_system_info_bind_heap_label(lv_obj_t *label)
{
    info_bind(&s_heap_label,label);
}

void settings_system_info_bind_psram_label(lv_obj_t *label)
{
    info_bind(&s_psram_label,label);
}

void settings_system_info_bind_uptime_label(lv_obj_t *label)
{
    info_bind(&s_uptime_label,label);
}

void settings_system_info_bind_heap_metrics(lv_obj_t *current,lv_obj_t *minimum,lv_obj_t *largest)
{
    info_bind(&s_heap_metrics[0],current);info_bind(&s_heap_metrics[1],minimum);
    info_bind(&s_heap_metrics[2],largest);
}
void settings_system_info_bind_psram_metrics(lv_obj_t *current,lv_obj_t *minimum)
{
    info_bind(&s_psram_metrics[0],current);info_bind(&s_psram_metrics[1],minimum);
}

void settings_system_info_refresh(void)
{
    bool heap_bound=s_heap_label || s_heap_metrics[0] || s_heap_metrics[1] || s_heap_metrics[2];
    bool psram_bound=s_psram_label || s_psram_metrics[0] || s_psram_metrics[1];
    if(s_info_refreshing || (!s_idf_label && !heap_bound && !psram_bound && !s_uptime_label))return;
    s_info_refreshing=true;
    uint32_t epoch=s_info_epoch;
    char heap_buf[64],psram_buf[64],uptime_buf[48];
    char heap_values[3][16],psram_values[2][16];
    const char *idf_version=NULL;
    if(heap_bound){
        size_t current=heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        if(epoch!=s_info_epoch)goto done;
        size_t minimum=heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
        if(epoch!=s_info_epoch)goto done;
        size_t largest=heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
        if(epoch!=s_info_epoch)goto done;
        format_memory_health(heap_buf,sizeof(heap_buf),current,minimum,largest);
        format_bytes(heap_values[0],sizeof(heap_values[0]),current);
        format_bytes(heap_values[1],sizeof(heap_values[1]),minimum);
        format_bytes(heap_values[2],sizeof(heap_values[2]),largest);
    }
    if(psram_bound){
        size_t current=heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        if(epoch!=s_info_epoch)goto done;
        size_t minimum=heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM);
        if(epoch!=s_info_epoch)goto done;
        format_memory_health(psram_buf,sizeof(psram_buf),current,minimum,0);
        format_bytes(psram_values[0],sizeof(psram_values[0]),current);
        format_bytes(psram_values[1],sizeof(psram_values[1]),minimum);
    }
    if(s_uptime_label){
        int64_t uptime=esp_timer_get_time()/1000000LL;
        if(epoch!=s_info_epoch)goto done;
        format_uptime(uptime_buf,sizeof(uptime_buf),uptime);
    }
    if(s_idf_label){
        idf_version=settings_system_info_idf_version();
        if(epoch!=s_info_epoch)goto done;

    }
    if(s_idf_label)ui_value_set_text(s_idf_label,idf_version?idf_version:"--");
    if(epoch!=s_info_epoch)goto done;
    if(s_heap_label)ui_value_set_text(s_heap_label,heap_buf);
    if(epoch!=s_info_epoch)goto done;
    if(s_psram_label)ui_value_set_text(s_psram_label,psram_buf);
    if(epoch!=s_info_epoch)goto done;
    if(s_uptime_label)ui_value_set_text(s_uptime_label,uptime_buf);
    if(epoch!=s_info_epoch)goto done;
    for(unsigned i=0;i<3;i++) {
        if(s_heap_metrics[i])ui_value_set_text(s_heap_metrics[i],heap_values[i]);
        if(epoch!=s_info_epoch)goto done;
    }
    for(unsigned i=0;i<2;i++) {
        if(s_psram_metrics[i])ui_value_set_text(s_psram_metrics[i],psram_values[i]);
        if(epoch!=s_info_epoch)goto done;
    }
 done:
    s_info_refreshing=false;
}

void settings_system_info_unbind(void)
{
    ++s_info_epoch;
    s_idf_label = NULL;
    s_heap_label = NULL;
    s_psram_label = NULL;
    s_uptime_label = NULL;
    for(unsigned i=0;i<3;i++)s_heap_metrics[i]=NULL;
    for(unsigned i=0;i<2;i++)s_psram_metrics[i]=NULL;
}
