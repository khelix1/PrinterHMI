#include "dashboard_runtime_controller.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"

#include "moonraker.h"
#include "moonraker_config_controller.h"
#include "printer_controller.h"
#include "printer_layer_resolver.h"
#include "thumbnail_preview_coordinator.h"
#include "thumbnail_session.h"
#include "ui_dashboard.h"
#include "ui_printer.h"

static const char *TAG = "dashboard_runtime";

static const char *dashboard_print_state_text(
    const moonraker_state_t *state)
{
    if (!state->moonraker_ok) return "OFFLINE";
    if (!state->live_data_ok) {
        if (!strcmp(state->printer_state, "shutdown") ||
            !strcmp(state->printer_state, "error")) return "SHUTDOWN";
        return "WAITING FOR KLIPPER";
    }

    const char *text =
        printer_controller_status_text(state->printer_state);

    if (text && strcmp(text, "--") != 0) {
        return text;
    }

    return state->moonraker_ok ? "CONNECTED" : "OFFLINE";
}

static void update_active_print(
    const moonraker_state_t *state,
    const dashboard_runtime_context_t *context)
{
    char layer[32];
    char elapsed[32];
    char remaining[32];

    printer_layer_result_t display_layers =
        printer_layer_resolver_resolve(
            state->current_layer,
            state->total_layer,
            state->current_gcode_z,
            context->meta_object_height,
            context->meta_layer_height,
            state->progress);

    if (display_layers.valid) {
        snprintf(
            layer,
            sizeof(layer),
            "%d/%d",
            display_layers.current,
            display_layers.total);
    } else {
        snprintf(layer, sizeof(layer), "--/--");
    }

    printer_controller_format_hhmm(
        elapsed,
        sizeof(elapsed),
        state->print_duration);

    printer_controller_format_remaining(
        remaining,
        sizeof(remaining),
        state->progress,
        state->print_duration);

    ui_dashboard_set_active_print(
        layer,
        elapsed,
        remaining);

}

static void update_live_preview(
    const moonraker_state_t *state,
    const dashboard_runtime_context_t *context)
{
    if (!context->preview_network_ready) {
        return;
    }

    thumbnail_preview_coordinator_context_t preview = {
        .printer_state = state->printer_state,
        .printer_file = state->printer_file,

        .moonraker_host = moonraker_config_host(),
        .moonraker_port = moonraker_config_port(),

        .selected_print_file =
            thumbnail_session_selected_file(),
        .selected_print_file_size =
            thumbnail_session_selected_file_size(),
        .selected_thumbnail_path =
            thumbnail_session_selected_thumbnail_path(),

        .dashboard_canvas_file =
            ui_dashboard_thumb_canvas_file(),
        .printer_canvas_file =
            ui_printer_preview_canvas_file(),

        .dashboard_canvas = context->dashboard_canvas,
        .dashboard_image = context->dashboard_image,
        .printer_canvas =
            ui_printer_preview_canvas_ref(),
        .printer_image =
            ui_printer_preview_image_ref(),

        .metadata_info =
            thumbnail_session_metadata_info(),
        .metadata_info_size =
            thumbnail_session_metadata_info_size(),

        .set_live_target = context->set_live_target,
        .free_thumbnail = context->free_thumbnail,
        .build_metadata = context->build_metadata,
        .start_delayed = context->start_delayed,
    };

    thumbnail_preview_coordinator_update(&preview);
}

static void update_print_complete_cleanup(
    const moonraker_state_t *state,
    const dashboard_runtime_context_t *context)
{
    const char *now_state =
        dashboard_print_state_text(state);

    if (context->last_print_state &&
        ((strcmp(context->last_print_state, "PRINTING") == 0 ||
          strcmp(context->last_print_state, "PAUSED") == 0) &&
         (strcmp(now_state, "READY") == 0 ||
          strcmp(now_state, "CONNECTED") == 0))) {
        if (context->dashboard_image &&
            *context->dashboard_image) {
            lv_obj_delete(*context->dashboard_image);
            *context->dashboard_image = NULL;
        }

        ui_dashboard_thumb_set_placeholder(
            "PRINT\nTHUMBNAIL");

        thumbnail_session_selected_file()[0] = 0;
        thumbnail_session_selected_thumbnail_path()[0] = 0;

        if (context->free_thumbnail) {
            context->free_thumbnail();
        }

        ESP_LOGI(TAG, "DASH_PRINT_COMPLETE cleanup");
    }

    if (context->last_print_state &&
        context->last_print_state_size > 0) {
        snprintf(
            context->last_print_state,
            context->last_print_state_size,
            "%s",
            now_state);
    }
}

void dashboard_runtime_controller_tick(
    const dashboard_runtime_context_t *context)
{
    if (!context) {
        return;
    }

    moonraker_state_t state;
    moonraker_state_snapshot(&state);
    if (!state.moonraker_ok || !state.live_data_ok) {
        state.progress = state.current_gcode_z = state.print_duration = -1.0;
        state.current_layer = state.total_layer = -1;
    }

    update_active_print(&state, context);
    update_live_preview(&state, context);
    update_print_complete_cleanup(&state, context);
}
