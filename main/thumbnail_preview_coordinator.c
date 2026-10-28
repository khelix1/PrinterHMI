#include "thumbnail_preview_coordinator.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"

#include "printer_controller.h"
#include "thumbnail_manager.h"

static const char *TAG = "dashboard_preview";

static char s_live_preview_key[240] = "";
static int64_t s_live_preview_retry_after_us;
static unsigned s_live_preview_attempts;

#define LIVE_PREVIEW_RETRY_DELAY_US (5LL * 1000000LL)
#define LIVE_PREVIEW_MAX_ATTEMPTS 4U

static void copy_text(char *destination,
                      size_t destination_size,
                      const char *source)
{
    if (!destination || destination_size == 0) {
        return;
    }

    if (!source) {
        destination[0] = '\0';
        return;
    }

    snprintf(destination, destination_size, "%s", source);
}

/* Capture all old targets before any DELETE callback can install replacements. */
static void retire_preview_objects(thumbnail_preview_coordinator_context_t *context,
                                   bool keep_dashboard, bool keep_printer)
{
    lv_obj_t **slots[]={keep_dashboard?NULL:context->dashboard_canvas,context->dashboard_image,keep_printer?NULL:context->printer_canvas,context->printer_image};
    lv_obj_t *old[4]={0};
    for(unsigned i=0;i<4;i++)if(slots[i]){old[i]=*slots[i];*slots[i]=NULL;}
    for(unsigned i=0;i<4;i++)if(old[i]){
        bool duplicate=false;for(unsigned j=0;j<i;j++)if(old[j]==old[i])duplicate=true;
        if(!duplicate)lv_obj_delete(old[i]);
    }
}

void thumbnail_preview_coordinator_reset(void)
{
    s_live_preview_key[0] = '\0';
    s_live_preview_retry_after_us = 0;
    s_live_preview_attempts = 0;
}

void thumbnail_preview_coordinator_update(
    thumbnail_preview_coordinator_context_t *context)
{
    if (!context ||
        !context->printer_state ||
        !context->printer_file) {
        return;
    }

    /*
     * Dashboard / printer preview policy:
     * - Printing/paused: show live print_stats.filename for current host:port.
     * - Switching host/port while printing forces fresh verification.
     * - File-confirm popup uses a separate temporary preview.
     */
    bool live_printing =
        printer_controller_is_live_state(
            context->printer_state) &&
        context->printer_file[0] &&
        strcmp(context->printer_file, "No file") != 0 &&
        strcmp(context->printer_file, "--") != 0;

    if (!live_printing) {
        thumbnail_preview_coordinator_reset();
        return;
    }

    if (thumbnail_manager_task_running()) {
        return;
    }

    char live_key[240];

    snprintf(live_key,
             sizeof(live_key),
             "%s:%d|%s",
             context->moonraker_host
                 ? context->moonraker_host
                 : "",
             context->moonraker_port,
             context->printer_file);

    int64_t now = esp_timer_get_time();
    if (strcmp(s_live_preview_key, live_key) == 0) {
        /* An attempted load is not a successful load. Browser-started jobs
         * can arrive before metadata is available, or a download can fail.
         * Retry without requiring Printer-page navigation, but do not keep
         * fetching metadata forever for files that have no thumbnail.
         */
        if (thumbnail_manager_result() == THUMBNAIL_MANAGER_RESULT_READY ||
            s_live_preview_attempts >= LIVE_PREVIEW_MAX_ATTEMPTS ||
            now < s_live_preview_retry_after_us) {
            return;
        }
    } else {
        s_live_preview_attempts = 0;
    }

    ++s_live_preview_attempts;
    s_live_preview_retry_after_us = now + LIVE_PREVIEW_RETRY_DELAY_US;

    copy_text(s_live_preview_key,
              sizeof(s_live_preview_key),
              live_key);

    copy_text(context->selected_print_file,
              context->selected_print_file_size,
              context->printer_file);

    if (context->set_live_target) {
        context->set_live_target();
    }

    if (context->selected_thumbnail_path) {
        context->selected_thumbnail_path[0] = '\0';
    }

    /* Profile switches invalidate these names before restoring an endpoint-
     * validated RGB canvas. Keep that canvas visible during fresh metadata/PNG
     * verification; RAW image objects still retire before PNG memory is freed. */
    bool keep_dashboard = context->dashboard_canvas && *context->dashboard_canvas &&
        context->dashboard_canvas_file &&
        strcmp(context->dashboard_canvas_file, context->printer_file) == 0;
    bool keep_printer = context->printer_canvas && *context->printer_canvas &&
        context->printer_canvas_file &&
        strcmp(context->printer_canvas_file, context->printer_file) == 0;

    if (context->dashboard_canvas_file && !keep_dashboard) {
        context->dashboard_canvas_file[0] = '\0';
    }

    if (context->printer_canvas_file && !keep_printer) {
        context->printer_canvas_file[0] = '\0';
    }

    thumbnail_manager_set_force_refresh(true);

    retire_preview_objects(context, keep_dashboard, keep_printer);

    if (context->free_thumbnail) {
        context->free_thumbnail();
    }

    if (context->build_metadata &&
        context->selected_print_file &&
        context->metadata_info &&
        context->metadata_info_size > 0) {
        context->build_metadata(
            context->selected_print_file,
            context->metadata_info,
            context->metadata_info_size);
    }

    ESP_LOGI(TAG,
             "DASH_LIVE_PREVIEW key=%s thumb=%s",
             s_live_preview_key,
             context->selected_thumbnail_path
                 ? context->selected_thumbnail_path
                 : "");

    if (context->start_delayed) {
        context->start_delayed();
    }
}
