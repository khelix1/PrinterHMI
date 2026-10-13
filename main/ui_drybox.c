#include "ui_drybox.h"

#include <string.h>
#include <stdio.h>

#include "esp_log.h"

#include "moonraker.h"
#include "ui_drybox_page.h"
#include "ui_shell.h"
#include "moonraker_config_controller.h"
#include "printer_connection_policy.h"
#include "ui_toast.h"

static const char TAG[] = "ui_drybox";

static ui_drybox_page_t s_page;
static ui_drybox_command_cb_t s_command_cb;
static ui_drybox_status_cb_t s_status_cb;
static uint32_t s_drybox_owner, s_drybox_epoch, s_sent_owner, s_sent_at;
static bool s_drybox_dispatching, s_sent_pending;
static char s_sent_command[16];
static ui_drybox_program_t s_selected_program =
    UI_DRYBOX_PROGRAM_NONE;
static ui_drybox_program_t s_active_program =
    UI_DRYBOX_PROGRAM_NONE;


static const char *drybox_banner_text_from_state(
    const moonraker_state_t *state)
{
    if (!state) {
        return "DRYBOX OFFLINE";
    }

    const moonraker_capabilities_t *capabilities =
        &state->capabilities;

    bool available =
        capabilities->has_drybox_center_sensor ||
        capabilities->has_drybox_environment_sensor ||
        capabilities->has_drybox_heater ||
        capabilities->has_drybox_fan ||
        capabilities->has_drybox_macros;

    if (capabilities->discovered && !available) {
        return "DRYBOX UNAVAILABLE";
    }

    if (printer_connection_reason(state)[0]) {
        return "DRYBOX OFFLINE";
    }

    if (state->heater_on) {
        return "DRYBOX HEATING";
    }

    if (state->humidity > 0.0 && state->humidity < 20.0) {
        return "DRYBOX READY";
    }

    return "DRYBOX MONITORING";
}


static const char *drybox_banner_text(void)
{
    moonraker_state_t state;
    moonraker_state_snapshot(&state);
    return drybox_banner_text_from_state(&state);
}


static void refresh_from_state(bool synchronize_programs)
{
    if (!s_page.panel) {
        return;
    }

    moonraker_state_t state;
    moonraker_state_snapshot(&state);

    bool same = s_drybox_owner == moonraker_config_generation();
    if (!same) { s_selected_program = s_active_program = UI_DRYBOX_PROGRAM_NONE; }
    else if (synchronize_programs) {
        s_selected_program =
            (ui_drybox_program_t)state.drybox_selected_program;
        s_active_program =
            (ui_drybox_program_t)state.drybox_active_program;
    }

    ui_drybox_page_state_t page_state = {
        .banner_text = same ? drybox_banner_text_from_state(&state) : "DRYBOX OFFLINE / PRINTER CHANGED",
        .air_temp = state.air_temp,
        .center_temp = state.chamber_temp,
        .humidity = state.humidity,
        .heater_target = state.heater_target,
        .heater_on = state.heater_on,
        .fan_speed = state.drybox_fan_speed,
        .active_program = s_active_program,
    };

    ui_drybox_page_refresh(&s_page, &page_state);
}


static void drybox_page_action(
    const char *command,
    lv_event_t *event)
{
    if (!command || !s_page.panel || s_drybox_dispatching ||
        s_drybox_owner != moonraker_config_generation()) return;
    if (event && lv_event_get_code(event) != LV_EVENT_CLICKED) return;
    if (!strcmp(command,"DRY_STATUS")) { if(s_status_cb)s_status_cb(event);return; }
    const char *owned_command = !strcmp(command,"DRY_PLA") ? "DRY_PLA" :
        !strcmp(command,"DRY_PETG") ? "DRY_PETG" : !strcmp(command,"DRY_HOLD") ? "DRY_HOLD" :
        !strcmp(command,"DRY_RESUME") ? "DRY_RESUME" : !strcmp(command,"DRY_STOP") ? "DRY_STOP" : NULL;
    if (!owned_command || !s_command_cb) return;
    moonraker_state_t live;moonraker_state_snapshot(&live);
    const char *reason=printer_connection_reason(&live);
    if(reason[0]){ui_toast_show(UI_STATUS_DANGER,"DRYBOX",reason);return;}
    if(s_sent_pending && s_sent_owner==s_drybox_owner && !strcmp(s_sent_command,owned_command) &&
        lv_tick_elaps(s_sent_at)<5000){ui_toast_show(UI_STATUS_INFO,"DRYBOX","Command already sent. Check printer before repeating.");return;}
    uint32_t owner=s_drybox_owner,epoch=s_drybox_epoch;
    lv_obj_t *panel=s_page.panel;
    ui_drybox_command_cb_t send=s_command_cb;
    s_drybox_dispatching=true;
    bool accepted=send(owned_command);
    s_drybox_dispatching=false;
    if(accepted){s_sent_pending=true;s_sent_owner=owner;s_sent_at=lv_tick_get();snprintf(s_sent_command,sizeof(s_sent_command),"%s",owned_command);}
    if(!s_page.panel || s_page.panel!=panel || epoch!=s_drybox_epoch || owner!=moonraker_config_generation())return;
    if(accepted){
        if(!strcmp(owned_command,"DRY_PLA"))s_selected_program=UI_DRYBOX_PROGRAM_PLA;
        else if(!strcmp(owned_command,"DRY_PETG"))s_selected_program=UI_DRYBOX_PROGRAM_PETG;
        /* Active program and heater state are confirmed only by telemetry. */
    }
    refresh_from_state(false);
    ui_toast_show(accepted?UI_STATUS_INFO:UI_STATUS_DANGER,"DRYBOX",
        accepted?"Command sent. Waiting for printer status.":"Send failed. Previous program kept; check connection and retry.");
}


void ui_drybox_set_callbacks(
    ui_drybox_command_cb_t command_cb,
    ui_drybox_status_cb_t status_cb)
{
    s_command_cb = command_cb;
    s_status_cb = status_cb;
}


void ui_drybox_show(void)
{
    if (!s_command_cb || !s_status_cb) {
        ESP_LOGE(TAG, "Drybox callbacks are not configured");
        return;
    }

    if(s_page.panel && s_drybox_owner!=moonraker_config_generation())ui_drybox_hide();
    if(!s_page.panel){++s_drybox_epoch;s_drybox_owner=moonraker_config_generation();}
    if (!ui_drybox_page_create(
            &s_page,
            drybox_page_action,
            drybox_banner_text)) {
        ESP_LOGE(TAG, "Drybox page creation failed");
        return;
    }

    refresh_from_state(true);
}


void ui_drybox_hide(void)
{
    ++s_drybox_epoch;
    ui_drybox_page_cleanup(&s_page);
    ui_shell_raise_topbar();
    ui_shell_raise_nav();
}


void ui_drybox_refresh(void)
{
    refresh_from_state(true);
}
