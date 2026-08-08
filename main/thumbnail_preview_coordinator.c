#include "thumbnail_preview_coordinator.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "esp_log.h"

#include "printer_controller.h"
#include "thumbnail_manager.h"

static const char *TAG = "dashboard_preview";

static char s_live_preview_key[240] = "";

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

static void delete_object(lv_obj_t **object)
{
    if (!object || !*object) {
        return;
    }

    lv_obj_delete(*object);
    *object = NULL;
}

void thumbnail_preview_coordinator_v32_reset(void)
{
    s_live_preview_key[0] = '\0';
}

void thumbnail_preview_coordinator_v32_update(
    thumbnail_preview_coordinator_v32_context_t *context)
{
    if (!context ||
        !context->printer_state ||
        !context->printer_file) {
        return;
    }

    /*
     * Dashboard / printer preview policy:
     * - Printing/paused: show live print_stats.filename for current host:port.
     * - Switching host/port while printing forces reload.
     * - File-confirm popup uses a separate temporary preview.
     */
    bool live_printing =
        printer_controller_is_live_state(
            context->printer_state) &&
        context->printer_file[0] &&
        strcmp(context->printer_file, "No file") != 0 &&
        strcmp(context->printer_file, "--") != 0;

    if (!live_printing) {
        thumbnail_preview_coordinator_v32_reset();
        return;
    }

    if (thumbnail_manager_v32_task_running()) {
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

    if (strcmp(s_live_preview_key, live_key) == 0) {
        return;
    }

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

    if (context->dashboard_canvas_file) {
        context->dashboard_canvas_file[0] = '\0';
    }

    if (context->printer_canvas_file) {
        context->printer_canvas_file[0] = '\0';
    }

    thumbnail_manager_v32_set_force_refresh(true);

    delete_object(context->dashboard_canvas);
    delete_object(context->dashboard_image);
    delete_object(context->printer_canvas);
    delete_object(context->printer_image);

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
