#include "printer_controller.h"
#include "ui_theme.h"

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
    return printer_controller_state_is(state, "error");
}

bool printer_controller_is_live_state(const char *state)
{
    return printer_controller_is_printing(state) ||
           printer_controller_is_paused(state);
}

bool printer_controller_has_active_job(const char *state, const char *file)
{
    bool live = printer_controller_is_printing(state) ||
                printer_controller_is_paused(state);

    bool has_file = file && file[0] && strcmp(file, "No file") != 0;

    return live && has_file;
}

void printer_controller_format_status_symbol_text(char *out,
                                                  size_t out_len,
                                                  const char *state,
                                                  bool moonraker_ok,
                                                  bool live_data_ok)
{
    if (!out || out_len == 0) return;

    if (printer_controller_is_printing(state)) {
        snprintf(out, out_len, LV_SYMBOL_PLAY " PRINTING");
    } else if (printer_controller_is_paused(state)) {
        snprintf(out, out_len, LV_SYMBOL_PAUSE " PAUSED");
    } else if (printer_controller_is_error(state)) {
        snprintf(out, out_len, LV_SYMBOL_WARNING " ERROR");
    } else if (moonraker_ok || live_data_ok) {
        snprintf(out, out_len, LV_SYMBOL_OK " READY");
    } else {
        snprintf(out, out_len, LV_SYMBOL_WARNING " OFFLINE");
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


lv_color_t printer_controller_state_text_color(const char *state)
{
    if (printer_controller_is_printing(state)) return UI_OK_BRIGHT;
    if (printer_controller_is_paused(state)) return UI_WARN;
    if (printer_controller_is_error(state)) return UI_DANGER_BRIGHT;
    return UI_TEXT;
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

void printer_controller_format_eta_clock(char *out,
                                         size_t out_len,
                                         double progress,
                                         double print_duration_seconds)
{
    if (!out || out_len == 0) return;

    snprintf(out, out_len, "--:--");

    if (progress <= 0.01 || progress >= 0.999 || print_duration_seconds <= 1.0) {
        return;
    }

    double remain = print_duration_seconds * (1.0 - progress) / progress;

    time_t now = 0;
    struct tm ti = {0};

    time(&now);
    now += (time_t)remain;
    localtime_r(&now, &ti);

    if (ti.tm_year >= (2024 - 1900)) {
        strftime(out, out_len, "%I:%M %p", &ti);
        if (out[0] == '0') {
            memmove(out, out + 1, strlen(out));
        }
    }
}

void printer_controller_format_topbar_eta(char *out,
                                           size_t out_len,
                                           double progress,
                                           double print_duration_seconds,
                                           const char *remaining_text,
                                           bool moonraker_ok)
{
    if (!out || out_len == 0) return;

    if (progress > 0.01 && progress < 0.999 && print_duration_seconds > 1.0) {
        snprintf(out, out_len, "ETA %s", (remaining_text && remaining_text[0]) ? remaining_text : "--:--");
    } else if (moonraker_ok) {
        snprintf(out, out_len, "IDLE");
    } else {
        snprintf(out, out_len, "OFFLINE");
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

    if (progress > 0.01 && progress < 0.999 && print_duration_seconds > 1.0) {
        double remain = print_duration_seconds * (1.0 - progress) / progress;
        int mins = (int)(remain / 60.0);
        int hrs = mins / 60;
        mins = mins % 60;
        snprintf(out, out_len, "%d:%02d", hrs, mins);
    } else {
        snprintf(out, out_len, "--:--");
    }
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
