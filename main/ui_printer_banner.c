#include "ui_printer_banner.h"

#include "ui_theme.h"
#include "ui_widgets.h"
#include "ui_status_banner_v32.h"

#include "esp_timer.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static char s_notice[128];
static int64_t s_notice_until_us;
static ui_status_kind_t s_notice_kind = UI_STATUS_INFO;

void ui_printer_banner_show_notice(const char *text,
                                   ui_status_kind_t kind,
                                   uint32_t duration_ms)
{
    snprintf(s_notice, sizeof(s_notice), "%s", text ? text : "");
    s_notice_kind = kind;
    s_notice_until_us = esp_timer_get_time() +
                        (int64_t)duration_ms * 1000;
}

static void printer_banner_normalize_state(
    const char *state,
    char *out,
    size_t out_len)
{
    if (!out || out_len == 0) {
        return;
    }

    if (!state) {
        state = "";
    }

    size_t index = 0;

    while (state[index] && index + 1 < out_len) {
        out[index] =
            (char)toupper(
                (unsigned char)state[index]);
        index++;
    }

    out[index] = '\0';
}


static ui_status_kind_t printer_banner_status_kind(
    const char *printer_state)
{
    const char *state =
        printer_state
            ? printer_state
            : "";

    /*
     * Operator state palette:
     *
     * READY       -> informational
     * PRINTING    -> active operation
     * PAUSED      -> operator attention
     * ERROR       -> danger
     * OFFLINE     -> danger
     */
    if (strcmp(state, "PRINTING") == 0 ||
        strcmp(state, "COMPLETE") == 0) {
        return UI_STATUS_OK;
    }

    if (strcmp(state, "PAUSED") == 0) {
        return UI_STATUS_WARNING;
    }

    if (strcmp(state, "ERROR") == 0 ||
        strcmp(state, "OFFLINE") == 0 ||
        strcmp(state, "DISCONNECTED") == 0) {
        return UI_STATUS_DANGER;
    }

    if (strcmp(state, "READY") == 0 ||
        strcmp(state, "STANDBY") == 0 ||
        strcmp(state, "IDLE") == 0 ||
        strcmp(state, "CONNECTED") == 0) {
        return UI_STATUS_INFO;
    }

    return UI_STATUS_NEUTRAL;
}


void ui_printer_banner_create(
    lv_obj_t *parent,
    lv_obj_t **banner_label,
    const char *initial_text)
{
    if (!parent || !banner_label) {
        return;
    }

    *banner_label = NULL;

    lv_obj_t *banner =
        ui_status_banner_v32_create(
            parent,
            UI_STATUS_BAR_X,
            UI_STATUS_BAR_Y,
            UI_STATUS_BAR_WIDTH,
            UI_STATUS_BAR_HEIGHT);

    if (!banner) {
        return;
    }

    ui_status_banner_v32_set(
        banner,
        "--",
        initial_text && initial_text[0]
            ? initial_text
            : "MACHINE STATUS",
        NULL,
        NULL);

    *banner_label =
        ui_status_banner_v32_state_label(banner);
}


void ui_printer_banner_refresh(
    lv_obj_t *parent,
    lv_obj_t *banner_label,
    lv_obj_t *state_label,
    const char *banner_text,
    const char *printer_state)
{
    (void)state_label;

    if (!parent || !banner_label) {
        return;
    }

    lv_obj_t *banner =
        lv_obj_get_parent(
            banner_label);

    if (!banner) {
        return;
    }

    if (s_notice[0] && esp_timer_get_time() < s_notice_until_us) {
        ui_status_banner_v32_set(banner,
                                 "UPDATED",
                                 s_notice,
                                 NULL,
                                 NULL);
        ui_operator_banner_set_status(banner, s_notice_kind);
        return;
    }
    s_notice[0] = '\0';

    char normalized_state[32];

    printer_banner_normalize_state(
        printer_state,
        normalized_state,
        sizeof(normalized_state));

    ui_status_banner_v32_set(
        banner,
        normalized_state[0]
            ? normalized_state
            : "--",
        banner_text && banner_text[0]
            ? banner_text
            : "MACHINE STATUS",
        NULL,
        NULL);

    /*
     * Moonraker reports lowercase lifecycle states. The shared status bar
     * and Printer's explicit palette both consume the normalized state.
     */
    ui_operator_banner_set_status(
        banner,
        printer_banner_status_kind(normalized_state));
}
