#include "printer_controller.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

bool printer_controller_state_is(const char *state, const char *want)
{
    return state && want && strcmp(state, want) == 0;
}

bool printer_controller_is_printing(const char *state)
{
    return printer_controller_state_is(state, "printing");
}

bool printer_controller_is_paused(const char *state)
{
    return printer_controller_state_is(state, "paused");
}

bool printer_controller_is_ready(const char *state)
{
    return printer_controller_state_is(state, "standby") ||
           printer_controller_state_is(state, "ready") ||
           printer_controller_state_is(state, "--");
}

bool printer_controller_is_error(const char *state)
{
    return printer_controller_state_is(state, "error") ||
           printer_controller_state_is(state, "shutdown");
}

bool printer_controller_is_live_state(const char *state)
{
    return printer_controller_is_printing(state) ||
           printer_controller_is_paused(state);
}

void printer_controller_format_status_symbol_text(char *out,
                                                  size_t out_len,
                                                  const char *state,
                                                  bool moonraker_ok,
                                                  bool live_data_ok)
{
    if (!out || out_len == 0) return;

    if (!moonraker_ok) {
        snprintf(out, out_len, LV_SYMBOL_WARNING " OFFLINE");
    } else if (printer_controller_state_is(state, "shutdown")) {
        snprintf(out, out_len, LV_SYMBOL_WARNING " SHUTDOWN");
    } else if (printer_controller_is_error(state)) {
        snprintf(out, out_len, LV_SYMBOL_WARNING " ERROR");
    } else if (!live_data_ok) {
        snprintf(out, out_len, LV_SYMBOL_WARNING " WAITING FOR KLIPPER");
    } else if (printer_controller_is_printing(state)) {
        snprintf(out, out_len, LV_SYMBOL_PLAY " PRINTING");
    } else if (printer_controller_is_paused(state)) {
        snprintf(out, out_len, LV_SYMBOL_PAUSE " PAUSED");
    } else {
        snprintf(out, out_len, LV_SYMBOL_OK " READY");
    }
}

const char *printer_controller_status_text(const char *state)
{
    if (printer_controller_is_printing(state)) return "PRINTING";
    if (printer_controller_is_paused(state)) return "PAUSED";
    if (printer_controller_is_ready(state)) return "READY";
    if (printer_controller_is_error(state)) return "ERROR";
    return state && state[0] ? state : "--";
}

const char *printer_controller_machine_banner_text(const char *state, bool moonraker_ok)
{
    if (printer_controller_is_printing(state)) return "MACHINE PRINTING";
    if (printer_controller_is_paused(state)) return "MACHINE PAUSED";
    if (printer_controller_is_ready(state)) return "MACHINE READY";
    if (printer_controller_is_error(state)) return "MACHINE ERROR";
    return moonraker_ok ? "MACHINE CONNECTED" : "MACHINE OFFLINE";
}


static bool state_is(const char *state, const char *want)
{
    return state && want && strcmp(state, want) == 0;
}

static void set_btn_enabled(lv_obj_t *btn, bool enabled)
{
    if (!btn) return;

    if (enabled) {
        lv_obj_remove_state(btn, LV_STATE_DISABLED);
        lv_obj_set_style_opa(btn, LV_OPA_COVER, 0);
    } else {
        lv_obj_add_state(btn, LV_STATE_DISABLED);
        lv_obj_set_style_opa(btn, LV_OPA_50, 0);
    }
}

static bool printer_controller_remaining_seconds(
    double progress,
    double print_duration_seconds,
    double *remaining_seconds)
{
    if (!remaining_seconds) {
        return false;
    }

    *remaining_seconds = 0.0;

    if (progress >= 0.999) {
        return true;
    }

    if (progress <= 0.01 ||
        print_duration_seconds <= 1.0) {
        return false;
    }

    *remaining_seconds =
        print_duration_seconds *
        (1.0 - progress) /
        progress;

    if (*remaining_seconds < 0.0) {
        *remaining_seconds = 0.0;
    }

    return true;
}


void printer_controller_format_eta_clock(char *out,
                                         size_t out_len,
                                         double progress,
                                         double print_duration_seconds)
{
    if (!out || out_len == 0) return;

    snprintf(out, out_len, "--:--");

    double remaining_seconds = 0.0;

    if (!printer_controller_remaining_seconds(
            progress,
            print_duration_seconds,
            &remaining_seconds)) {
        return;
    }

    time_t now = 0;
    struct tm ti = {0};

    time(&now);
    now += (time_t)(remaining_seconds + 0.5);
    localtime_r(&now, &ti);

    if (ti.tm_year >= (2024 - 1900)) {
        strftime(out, out_len, "%I:%M %p", &ti);

        if (out[0] == '0') {
            memmove(out, out + 1, strlen(out));
        }
    }
}

void printer_controller_format_hhmm(char *out,
                                    size_t out_len,
                                    double seconds)
{
    if (!out || out_len == 0) return;

    if (seconds < 0.0) {
        snprintf(out, out_len, "--:--");
        return;
    }

    int total_minutes = (int)((seconds / 60.0) + 0.5);
    int h = total_minutes / 60;
    int m = total_minutes % 60;
    snprintf(out, out_len, "%02d:%02d", h, m);
}

void printer_controller_format_remaining(char *out,
                                         size_t out_len,
                                         double progress,
                                         double print_duration_seconds)
{
    if (!out || out_len == 0) return;

    double remaining_seconds = 0.0;

    if (!printer_controller_remaining_seconds(
            progress,
            print_duration_seconds,
            &remaining_seconds)) {
        snprintf(out, out_len, "--:--");
        return;
    }

    int total_minutes =
        (int)((remaining_seconds / 60.0) + 0.5);

    int hours = total_minutes / 60;
    int minutes = total_minutes % 60;

    snprintf(
        out,
        out_len,
        "%d:%02d",
        hours,
        minutes);
}

void printer_controller_update_action_buttons(lv_obj_t *home_btn,
                                              lv_obj_t *pause_btn,
                                              lv_obj_t *resume_btn,
                                              lv_obj_t *cancel_btn,
                                              const char *printer_state)
{
    bool printing = state_is(printer_state, "printing");
    bool paused   = state_is(printer_state, "paused");
    bool standby  = state_is(printer_state, "standby") ||
                    state_is(printer_state, "ready") ||
                    state_is(printer_state, "--");

    set_btn_enabled(home_btn, standby);
    set_btn_enabled(pause_btn, printing);
    set_btn_enabled(resume_btn, paused);
    set_btn_enabled(cancel_btn, printing || paused);
}
