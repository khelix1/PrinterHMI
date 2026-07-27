#include "dashboard_runtime_controller.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"

#include "moonraker.h"
#include "moonraker_config_controller.h"
#include "printer_controller.h"
#include "printer_layer_resolver.h"
#include "thumbnail_preview_coordinator_v32.h"
#include "thumbnail_session_v32.h"
#include "ui_dashboard_status_v32.h"
#include "ui_dashboard_v32.h"
#include "ui_printer_v32.h"
#include "ui_theme.h"

static const char *TAG = "dashboard_runtime";

static lv_color_t temp_value_color(double temp, double target)
{
    if (temp < -100.0) return UI_TEXT;
    if (target <= 0.0) return UI_TEXT;

    double diff = target - temp;

    if (diff > 10.0) return UI_DANGER_BRIGHT;
    if (diff > 2.0) return UI_WARN;
    if (diff < -5.0) return UI_DANGER_BRIGHT;

    return UI_OK_BRIGHT;
}

static const char *dashboard_print_state_text(
    const moonraker_state_t *state,
    bool moonraker_ok)
{
    const char *text =
        printer_controller_status_text(state->printer_state);

    if (text && strcmp(text, "--") != 0) {
        return text;
    }

    return moonraker_ok ? "CONNECTED" : "OFFLINE";
}

static void update_status_cards(
    const moonraker_state_t *state,
    const dashboard_runtime_context_t *context)
{
    char buffer[64];
    char layer[32];
    char elapsed[32];
    char remaining[32];

    if (context->nozzle_label) {
        if (state->nozzle_temp > -100.0) {
            snprintf(
                buffer,
                sizeof(buffer),
                "%.1f / %.1f C",
                state->nozzle_temp,
                state->nozzle_target);
        } else {
            snprintf(buffer, sizeof(buffer), "-- / -- C");
        }

        lv_label_set_text(context->nozzle_label, buffer);
        lv_obj_set_style_text_color(
            context->nozzle_label,
            temp_value_color(
                state->nozzle_temp,
                state->nozzle_target),
            0);
    }

    if (context->bed_label) {
        if (state->bed_temp > -100.0) {
            snprintf(
                buffer,
                sizeof(buffer),
                "%.1f / %.1f C",
                state->bed_temp,
                state->bed_target);
        } else {
            snprintf(buffer, sizeof(buffer), "-- / -- C");
        }

        lv_label_set_text(context->bed_label, buffer);
        lv_obj_set_style_text_color(
            context->bed_label,
            temp_value_color(
                state->bed_temp,
                state->bed_target),
            0);
    }

    ui_dashboard_status_v32_refresh(
        state->progress,
        state->print_duration);

    printer_layer_result_t display_layers =
        printer_layer_resolver_resolve(
            state->current_layer,
            state->total_layer,
            context->current_z,
            context->meta_object_height,
            context->meta_layer_height,
            state->progress);

    if (display_layers.current > 0 &&
        display_layers.total > 0) {
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

    ui_dashboard_v32_set_active_print(
        layer,
        elapsed,
        remaining);

    ui_dashboard_status_v32_set_print_state(
        dashboard_print_state_text(
            state,
            context->moonraker_ok));
}

static void update_live_preview(
    const moonraker_state_t *state,
    const dashboard_runtime_context_t *context)
{
    thumbnail_preview_coordinator_v32_context_t preview = {
        .printer_state = state->printer_state,
        .printer_file = state->printer_file,

        .moonraker_host = moonraker_config_host(),
        .moonraker_port = moonraker_config_port(),

        .selected_print_file =
            thumbnail_session_v32_selected_file(),
        .selected_print_file_size =
            thumbnail_session_v32_selected_file_size(),
        .selected_thumbnail_path =
            thumbnail_session_v32_selected_thumbnail_path(),

        .dashboard_canvas_file =
            ui_dashboard_v32_thumb_canvas_file(),
        .printer_canvas_file =
            ui_printer_v32_preview_canvas_file(),

        .dashboard_canvas = context->dashboard_canvas,
        .dashboard_image = context->dashboard_image,
        .printer_canvas =
            ui_printer_v32_preview_canvas_ref(),
        .printer_image =
            ui_printer_v32_preview_image_ref(),

        .metadata_info =
            thumbnail_session_v32_metadata_info(),
        .metadata_info_size =
            thumbnail_session_v32_metadata_info_size(),

        .set_live_target = context->set_live_target,
        .free_thumbnail = context->free_thumbnail,
        .build_metadata = context->build_metadata,
        .start_delayed = context->start_delayed,
    };

    thumbnail_preview_coordinator_v32_update(&preview);
}

static void update_print_complete_cleanup(
    const moonraker_state_t *state,
    const dashboard_runtime_context_t *context)
{
    const char *now_state =
        dashboard_print_state_text(
            state,
            context->moonraker_ok);

    if (context->last_print_state &&
        ((strcmp(context->last_print_state, "PRINTING") == 0 ||
          strcmp(context->last_print_state, "PAUSED") == 0) &&
         (strcmp(now_state, "READY") == 0 ||
          strcmp(now_state, "CONNECTED") == 0))) {
        ui_dashboard_v32_set_active_print_file(
            "No active file");

        ui_dashboard_status_v32_set_progress(
            "-- %",
            0,
            UI_TEXT);

        ui_dashboard_status_v32_set_times(
            "--:--",
            "--:--",
            "--:--");

        if (context->dashboard_image &&
            *context->dashboard_image) {
            lv_obj_delete(*context->dashboard_image);
            *context->dashboard_image = NULL;
        }

        ui_dashboard_v32_thumb_set_placeholder(
            "PRINT\nTHUMBNAIL");

        thumbnail_session_v32_selected_file()[0] = 0;
        thumbnail_session_v32_selected_thumbnail_path()[0] = 0;

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

static void update_environment_cards(
    const dashboard_runtime_context_t *context)
{
    char buffer[64];

    if (context->chamber_label) {
        snprintf(
            buffer,
            sizeof(buffer),
            "%.1f C",
            context->air_temp);
        lv_label_set_text(context->chamber_label, buffer);
    }

    if (context->humidity_label) {
        snprintf(
            buffer,
            sizeof(buffer),
            "%.1f %%RH",
            context->humidity);
        lv_label_set_text(context->humidity_label, buffer);
    }

    if (context->target_rh_label) {
        snprintf(
            buffer,
            sizeof(buffer),
            "Heat %.0f C",
            context->heater_target);
        lv_label_set_text(context->target_rh_label, buffer);
    }

    if (context->heater_label) {
        lv_label_set_text(
            context->heater_label,
            context->heater_on ? "ON" : "OFF");
    }

    if (context->fan_label) {
        snprintf(
            buffer,
            sizeof(buffer),
            "%.0f %%",
            context->fan_speed);
        lv_label_set_text(context->fan_label, buffer);
    }

    if (context->moonraker_label) {
        lv_label_set_text(
            context->moonraker_label,
            context->live_data_ok
                ? "linked"
                : "not linked");
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

    update_status_cards(&state, context);
    update_live_preview(&state, context);
    update_print_complete_cleanup(&state, context);
    update_environment_cards(context);
}
