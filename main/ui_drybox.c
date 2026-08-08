#include "ui_drybox.h"

#include <string.h>

#include "esp_log.h"

#include "moonraker.h"
#include "ui_drybox_page.h"
#include "ui_shell.h"

static const char TAG[] = "ui_drybox";

static ui_drybox_page_v32_t s_page;
static ui_drybox_v32_command_cb_t s_command_cb;
static ui_drybox_v32_status_cb_t s_status_cb;
static ui_drybox_program_v32_t s_selected_program =
    UI_DRYBOX_PROGRAM_NONE;
static ui_drybox_program_v32_t s_active_program =
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

    if (!state->live_data_ok) {
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

    if (synchronize_programs) {
        s_selected_program =
            (ui_drybox_program_v32_t)state.drybox_selected_program;
        s_active_program =
            (ui_drybox_program_v32_t)state.drybox_active_program;
    }

    ui_drybox_page_v32_state_t page_state = {
        .banner_text = drybox_banner_text_from_state(&state),
        .air_temp = state.air_temp,
        .center_temp = state.chamber_temp,
        .humidity = state.humidity,
        .heater_target = state.heater_target,
        .heater_on = state.heater_on,
        .fan_speed = state.drybox_fan_speed,
        .active_program = s_active_program,
    };

    ui_drybox_page_v32_refresh(&s_page, &page_state);
}


static void drybox_page_action(
    const char *command,
    lv_event_t *event)
{
    if (!command) {
        return;
    }

    if (strcmp(command, "DRY_STATUS") == 0) {
        if (s_status_cb) {
            s_status_cb(event);
        }
        return;
    }

    if (strcmp(command, "DRY_PLA") == 0) {
        s_selected_program = UI_DRYBOX_PROGRAM_PLA;
        s_active_program = UI_DRYBOX_PROGRAM_PLA;
    } else if (strcmp(command, "DRY_PETG") == 0) {
        s_selected_program = UI_DRYBOX_PROGRAM_PETG;
        s_active_program = UI_DRYBOX_PROGRAM_PETG;
    } else if (strcmp(command, "DRY_HOLD") == 0) {
        s_active_program = UI_DRYBOX_PROGRAM_HOLD;
    } else if (strcmp(command, "DRY_RESUME") == 0) {
        s_active_program = s_selected_program;
    } else if (strcmp(command, "DRY_STOP") == 0) {
        s_active_program = UI_DRYBOX_PROGRAM_NONE;
    }

    if (s_command_cb) {
        (void)s_command_cb(command);
    }

    /* Show the operator's selection without waiting for live telemetry. */
    refresh_from_state(false);
}


void ui_drybox_v32_set_callbacks(
    ui_drybox_v32_command_cb_t command_cb,
    ui_drybox_v32_status_cb_t status_cb)
{
    s_command_cb = command_cb;
    s_status_cb = status_cb;
}


void ui_drybox_v32_show(void)
{
    if (!s_command_cb || !s_status_cb) {
        ESP_LOGE(TAG, "Drybox callbacks are not configured");
        return;
    }

    if (!ui_drybox_page_v32_create(
            &s_page,
            drybox_page_action,
            drybox_banner_text)) {
        ESP_LOGE(TAG, "Drybox page creation failed");
        return;
    }

    refresh_from_state(true);
}


void ui_drybox_v32_hide(void)
{
    ui_drybox_page_v32_cleanup(&s_page);
    ui_shell_raise_topbar();
    ui_shell_raise_nav();
}


void ui_drybox_v32_refresh(void)
{
    refresh_from_state(true);
}
