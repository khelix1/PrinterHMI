#include "printer_ui_controller.h"

#include <string.h>

#include "printer_controller.h"
#include "ui_printer_banner.h"
#include "ui_printer_live_status.h"
#include "ui_printer.h"
#include "ui_printer_info_cards.h"

static printer_ui_controller_send_gcode_cb_t
    s_send_gcode_cb = NULL;

static printer_ui_controller_action_cb_t
    s_show_cancel_cb = NULL;

static printer_ui_controller_action_cb_t
    s_show_object_cb = NULL;

static printer_ui_controller_action_cb_t
    s_show_motion_cb = NULL;

void printer_ui_controller_init(
    printer_ui_controller_send_gcode_cb_t send_gcode_cb,
    printer_ui_controller_action_cb_t show_cancel_cb,
    printer_ui_controller_action_cb_t show_object_cb,
    printer_ui_controller_action_cb_t show_motion_cb)
{
    s_send_gcode_cb = send_gcode_cb;
    s_show_cancel_cb = show_cancel_cb;
    s_show_object_cb = show_object_cb;
    s_show_motion_cb = show_motion_cb;
}

void printer_ui_controller_command_event_cb(lv_event_t *event)
{
    if (!event) {
        return;
    }

    const char *command =
        (const char *)lv_event_get_user_data(event);

    if (!command || !command[0]) {
        return;
    }

    if (strcmp(command, "CANCEL_PRINT") == 0) {
        if (s_show_cancel_cb) {
            s_show_cancel_cb();
        }

        return;
    }

    if (strcmp(command, "CANCEL_OBJECT") == 0) {
        if (s_show_object_cb) {
            s_show_object_cb();
        }

        return;
    }

    if (s_send_gcode_cb) {
        (void)s_send_gcode_cb(command);
    }
}

void printer_ui_controller_motion_event_cb(lv_event_t *event)
{
    if (!event ||
        lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }

    if (s_show_motion_cb) {
        s_show_motion_cb();
    }
}

void printer_ui_controller_update_action_buttons(
    lv_obj_t *home_button,
    lv_obj_t *pause_button,
    lv_obj_t *resume_button,
    lv_obj_t *object_button,
    lv_obj_t *cancel_button,
    bool object_available,
    const char *printer_state)
{
    printer_controller_update_action_buttons(
        home_button,
        pause_button,
        resume_button,
        cancel_button,
        printer_state);

    if (object_button) {
        bool enabled = object_available &&
            printer_controller_is_live_state(printer_state);

        if (enabled) {
            lv_obj_clear_state(object_button, LV_STATE_DISABLED);
            lv_obj_set_style_opa(object_button, LV_OPA_COVER, 0);
        } else {
            lv_obj_add_state(object_button, LV_STATE_DISABLED);
            lv_obj_set_style_opa(object_button, LV_OPA_50, 0);
        }
    }
}

void printer_ui_controller_update_topbar_eta(
    lv_obj_t *eta_label,
    double progress,
    double print_duration,
    bool moonraker_ok)
{
    if (!eta_label) {
        return;
    }

    char remaining[32];
    char text[40];

    printer_controller_format_remaining(
        remaining,
        sizeof(remaining),
        progress,
        print_duration);

    printer_controller_format_topbar_eta(
        text,
        sizeof(text),
        progress,
        print_duration,
        remaining,
        moonraker_ok);

    lv_label_set_text(eta_label, text);
}

void printer_ui_controller_refresh(
    const printer_ui_controller_refresh_ctx_t *ctx)
{
    if (!ctx) {
        return;
    }

    ui_printer_banner_refresh(
        ctx->printer_panel,
        ctx->banner_label,
        ctx->state_label,
        ctx->banner_text,
        ctx->printer_state,
        ctx->printer_file);

    ui_printer_live_status_refresh(
        ctx->printer_panel,
        ctx->active_file_box,
        ctx->active_file_label,
        ctx->speed_label,
        ctx->flow_label,
        ctx->layer_label,
        ctx->filament_label,
        ctx->printer_state,
        ctx->printer_file,
        ctx->live_velocity,
        ctx->live_flow,
        ctx->speed_factor,
        ctx->flow_factor,
        ctx->current_layer,
        ctx->total_layer,
        ctx->metadata_object_height,
        ctx->metadata_layer_height,
        ctx->progress,
        ctx->moonraker_ok && ctx->live_data_ok,
        ctx->filament_state);

    if (ctx->printer_panel) {
        ui_printer_v32_preview_show(
            ctx->printer_state,
            ctx->printer_file,
            ctx->selected_preview_file);
    }

    ui_printer_info_cards_refresh_live(
        ctx->printer_panel,
        ctx->info_cards,
        ctx->progress,
        ctx->nozzle_temp,
        ctx->nozzle_target,
        ctx->bed_temp,
        ctx->bed_target,
        ctx->part_fan_speed,
        ctx->print_duration,
        ctx->moonraker_ok,
        ctx->capabilities);

    if (ctx->printer_panel && ctx->file_label) {
        lv_label_set_text(ctx->file_label, ctx->printer_file);
    }

    printer_ui_controller_update_topbar_eta(
        ctx->topbar_eta_label,
        ctx->progress,
        ctx->print_duration,
        ctx->moonraker_ok);

    printer_ui_controller_update_action_buttons(
        ctx->home_button,
        ctx->pause_button,
        ctx->resume_button,
        ctx->object_button,
        ctx->cancel_button,
        ctx->exclude_objects_available,
        ctx->printer_state);
}
