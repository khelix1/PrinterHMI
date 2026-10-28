#include "printer_ui_controller.h"
#include "printer_connection_policy.h"
#include "ui_filament_recovery.h"
#include "ui_toast.h"
#include "moonraker_config_controller.h"

#include <string.h>
#include <stdio.h>

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

enum { ACTION_HOME, ACTION_PAUSE, ACTION_RESUME, ACTION_OBJECT, ACTION_CANCEL, ACTION_MOTION, ACTION_COUNT };
static lv_obj_t *s_action_buttons[ACTION_COUNT];
static uint32_t s_action_owner, s_action_epoch;
static bool s_action_dispatching;
static struct { bool pending; uint32_t owner, sent_at; } s_direct_requests[2];

static void action_deleted(lv_event_t *event)
{
    for (size_t i = 0; i < ACTION_COUNT; ++i)
        if (s_action_buttons[i] == lv_event_get_target(event)) {
            s_action_buttons[i] = NULL;
            ++s_action_epoch;
        }
}

void printer_ui_controller_bind_actions(const ui_printer_actions_t *actions)
{
    ++s_action_epoch;
    s_action_owner = moonraker_config_generation();
    memset(s_action_buttons, 0, sizeof(s_action_buttons));
    if (!actions) return;
    s_action_buttons[ACTION_HOME] = actions->home;
    s_action_buttons[ACTION_PAUSE] = actions->pause;
    s_action_buttons[ACTION_RESUME] = actions->resume;
    s_action_buttons[ACTION_OBJECT] = actions->object;
    s_action_buttons[ACTION_CANCEL] = actions->cancel;
    s_action_buttons[ACTION_MOTION] = actions->motion;
    for (size_t i = 0; i < ACTION_COUNT; ++i)
        if (s_action_buttons[i]) lv_obj_add_event_cb(s_action_buttons[i], action_deleted, LV_EVENT_DELETE, NULL);
}

static bool direct_pending(size_t index, const moonraker_state_t *live)
{
    if (!s_direct_requests[index].pending) return false;
    if (s_direct_requests[index].owner != moonraker_config_generation() ||
        lv_tick_elaps(s_direct_requests[index].sent_at) >= 5000 ||
        (index == ACTION_PAUSE && !strcmp(live->printer_state, "paused"))) {
        s_direct_requests[index].pending = false;
        return false;
    }
    return true;
}

static bool action_event_owned(lv_event_t *event, size_t index)
{
    return event && lv_event_get_code(event) == LV_EVENT_CLICKED && !s_action_dispatching &&
        s_action_owner == moonraker_config_generation() && s_action_buttons[index] &&
        lv_event_get_target(event) == s_action_buttons[index] &&
        !lv_obj_has_state(s_action_buttons[index], LV_STATE_DISABLED);
}

void printer_ui_controller_init(
    printer_ui_controller_send_gcode_cb_t send_gcode_cb,
    printer_ui_controller_action_cb_t show_cancel_cb,
    printer_ui_controller_action_cb_t show_object_cb,
    printer_ui_controller_action_cb_t show_motion_cb)
{
    ++s_action_epoch;
    memset(s_action_buttons, 0, sizeof(s_action_buttons));
    s_action_owner = moonraker_config_generation();
    s_send_gcode_cb = send_gcode_cb;
    s_show_cancel_cb = show_cancel_cb;
    s_show_object_cb = show_object_cb;
    s_show_motion_cb = show_motion_cb;
}

void printer_ui_controller_command_event_cb(lv_event_t *event)
{
    if (!event || lv_event_get_code(event) != LV_EVENT_CLICKED) return;
    const char *requested = (const char *)lv_event_get_user_data(event);
    static const char *const commands[] = {"HOME_ALL", "PAUSE", "RESUME", "CANCEL_OBJECT", "CANCEL_PRINT"};
    size_t index;
    if (!requested) return;
    for (index = 0; index < ACTION_MOTION; ++index)
        if (!strcmp(requested, commands[index])) break;
    if (index == ACTION_MOTION || !action_event_owned(event, index)) return;
    /* Use an owned literal; callbacks may destroy the event target and its data. */
    const char *command = commands[index];

    moonraker_state_t live;moonraker_state_snapshot(&live);
    if(printer_connection_reason(&live)[0])return;
    if(!strcmp(command,"HOME_ALL") && printer_idle_reason(&live)[0])return;
    if(!strcmp(command,"PAUSE") && strcmp(live.printer_state,"printing"))return;
    if(!strcmp(command,"RESUME") && strcmp(live.printer_state,"paused"))return;
    if((!strcmp(command,"CANCEL_PRINT") || !strcmp(command,"CANCEL_OBJECT")) &&
       strcmp(live.printer_state,"printing") && strcmp(live.printer_state,"paused"))return;

    if (strcmp(command, "RESUME") == 0) {
        ui_filament_recovery_show(s_send_gcode_cb);
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

    if (index > ACTION_PAUSE || !s_send_gcode_cb || direct_pending(index, &live)) return;
    uint32_t owner = s_action_owner, epoch = s_action_epoch;
    printer_ui_controller_send_gcode_cb_t send = s_send_gcode_cb;
    s_action_dispatching = true;
    bool accepted = send(command);
    s_action_dispatching = false;
    if (accepted) {
        s_direct_requests[index].pending = true;
        s_direct_requests[index].owner = owner;
        s_direct_requests[index].sent_at = lv_tick_get();
    }
    if (owner != moonraker_config_generation() || epoch != s_action_epoch) return;
    ui_toast_show(accepted ? UI_STATUS_INFO : UI_STATUS_DANGER,
        index == ACTION_HOME ? "HOME" : "PAUSE",
        accepted ? (index == ACTION_HOME ? "Home sent. Wait for motion to finish." : "Pause sent. Waiting for printer status.") :
            "Send failed. Check the connection and retry.");
}

void printer_ui_controller_motion_event_cb(lv_event_t *event)
{
    if (!action_event_owned(event, ACTION_MOTION)) return;

    moonraker_state_t live;moonraker_state_snapshot(&live);
    if(printer_connection_reason(&live)[0])return;
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
    moonraker_state_t live;moonraker_state_snapshot(&live);
    if(printer_connection_reason(&live)[0] || s_action_owner != moonraker_config_generation())printer_state="offline";
    else if(printer_state && (!strcmp(printer_state,"complete") || !strcmp(printer_state,"cancelled")))printer_state="ready";
    printer_controller_update_action_buttons(
        home_button,
        pause_button,
        resume_button,
        cancel_button,
        printer_state);

    if (home_button && (s_action_dispatching || direct_pending(ACTION_HOME, &live)))
        lv_obj_add_state(home_button, LV_STATE_DISABLED);
    if (pause_button && (s_action_dispatching || direct_pending(ACTION_PAUSE, &live)))
        lv_obj_add_state(pause_button, LV_STATE_DISABLED);

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

void printer_ui_controller_refresh(
    const printer_ui_controller_refresh_ctx_t *ctx)
{
    if (!ctx) {
        return;
    }

    moonraker_state_t readiness={.moonraker_ok=ctx->moonraker_ok,.live_data_ok=ctx->live_data_ok};
    snprintf(readiness.printer_state,sizeof(readiness.printer_state),"%s",ctx->printer_state?ctx->printer_state:"");
    const char *reason=s_action_owner != moonraker_config_generation() ?
        "Printer changed - reopen Printer controls." : printer_connection_reason(&readiness);
    if(ctx->printer_panel && ctx->motion_button) {
        if(reason[0])lv_obj_add_state(ctx->motion_button,LV_STATE_DISABLED);
        else lv_obj_remove_state(ctx->motion_button,LV_STATE_DISABLED);
    }
    ui_printer_banner_refresh(
        ctx->printer_panel,
        ctx->banner_label,
        reason[0]?reason:ctx->banner_text,
        !ctx->moonraker_ok?"offline":(!ctx->live_data_ok?"not ready":ctx->printer_state),
        ctx->printer_file);

    ui_printer_live_status_refresh(
        ctx->printer_panel,
        ctx->speed_label,
        ctx->flow_label,
        ctx->layer_label,
        ctx->filament_label,
        ctx->live_velocity,
        ctx->live_flow,
        ctx->speed_factor,
        ctx->flow_factor,
        ctx->current_layer,
        ctx->total_layer,
        ctx->moonraker_ok && ctx->live_data_ok,
        ctx->filament_state);

    if (ctx->printer_panel) {
        ui_printer_preview_show(
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
        ctx->moonraker_ok && ctx->live_data_ok,
        ctx->capabilities);

    printer_ui_controller_update_action_buttons(
        ctx->home_button,
        ctx->pause_button,
        ctx->resume_button,
        ctx->object_button,
        ctx->cancel_button,
        ctx->exclude_objects_available,
        ctx->printer_state);
}
