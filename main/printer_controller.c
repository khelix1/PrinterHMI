#include "printer_controller.h"

#include <stdbool.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
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

int printer_controller_progress_percent(double progress)
{
    if (!isfinite(progress) || progress < 0.0 || progress > 1.0) return -1;
    return (int)(progress * 100.0);
}

static bool duration_valid(double seconds)
{
    return isfinite(seconds) && seconds >= 0.0 && seconds <= INT_MAX;
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
    if (printer_controller_progress_percent(progress) < 0 ||
        !duration_valid(print_duration_seconds)) return false;

    if (progress >= 0.999) {
        return true;
    }

    if (progress <= 0.01 ||
        print_duration_seconds <= 1.0) {
        return false;
    }

    double estimate = print_duration_seconds * (1.0 - progress) / progress;
    if (!duration_valid(estimate)) return false;
    *remaining_seconds = estimate;
    return true;
}


void printer_controller_format_eta_seconds(char *out, size_t out_len, double remaining_seconds)
{
    if (!out || !out_len) return;
    snprintf(out, out_len, "--:--");
    if (!duration_valid(remaining_seconds)) return;

    time_t now = time(NULL);
    struct tm today, finish;
    if (now < 0 || !localtime_r(&now, &today) || today.tm_year < 2024 - 1900) return;

    int64_t rounded = (int64_t)(remaining_seconds + 0.5);
    if ((int64_t)now > INT64_MAX - rounded) return;
    int64_t future = (int64_t)now + rounded;
    time_t finish_time = (time_t)future;
    if ((int64_t)finish_time != future || !localtime_r(&finish_time, &finish)) return;

    /* Compare calendar dates, not 24-hour intervals: a local day can be
     * shorter or longer during a daylight-saving transition. */
    int days = finish.tm_yday - today.tm_yday;
    for (int year = today.tm_year; year < finish.tm_year; ++year) {
        int64_t calendar_year = (int64_t)year + 1900;
        bool leap = calendar_year % 4 == 0 &&
                    (calendar_year % 100 != 0 || calendar_year % 400 == 0);
        days += leap ? 366 : 365;
    }
    char clock[24];
    if (!strftime(clock, sizeof(clock), "%I:%M %p", &finish)) return;
    const char *display_clock = clock + (clock[0] == '0');
    int written = days > 0
        ? snprintf(out, out_len, "%s +%dd", display_clock, days)
        : snprintf(out, out_len, "%s", display_clock);
    if (written < 0 || (size_t)written >= out_len) snprintf(out, out_len, "--:--");
}

void printer_controller_format_eta_clock(char *out, size_t out_len,
                                         double progress, double print_duration_seconds)
{
    double remaining;
    if (!printer_controller_remaining_seconds(progress, print_duration_seconds, &remaining))
        remaining = -1;
    printer_controller_format_eta_seconds(out, out_len, remaining);
}

void printer_controller_format_hhmm(char *out,
                                    size_t out_len,
                                    double seconds)
{
    if (!out || out_len == 0) return;

    if (!duration_valid(seconds)) {
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
