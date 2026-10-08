#include "ui_endstop_status.h"
#include <stdio.h>
#include <string.h>
#include "endstop_status_controller.h"
#include "esp_timer.h"
#include "moonraker.h"
#include "moonraker_config_controller.h"
#include "moonraker_live_websocket.h"
#include "ui_popup.h"
#include "ui_value_update.h"

static lv_obj_t *s_popup, *s_body;
static lv_timer_t *s_timer;
static uint32_t s_owner;
static int64_t s_next_query;

void ui_endstop_status_close(void)
{
    if (s_timer) lv_timer_delete(s_timer);
    s_timer = NULL;
    if (s_popup) lv_obj_delete(s_popup);
    s_popup = s_body = NULL;
    endstop_status_controller_reset();
}
static void close_cb(lv_event_t *event) { (void)event; ui_endstop_status_close(); }

static void refresh(lv_timer_t *timer)
{
    (void)timer;
    if (!s_body) return;
    if (s_owner != moonraker_config_generation()) {
        endstop_status_controller_reset();
        ui_value_set_text(s_body, "Printer changed. Close and reopen Endstops.");
        return;
    }
    moonraker_state_t state;
    moonraker_state_snapshot(&state);
    if (!state.moonraker_ok || !state.live_data_ok ||
        !moonraker_live_websocket_connected() ||
        !strcmp(state.printer_state, "error") || !strcmp(state.printer_state, "shutdown")) {
        endstop_status_controller_reset();
        ui_value_set_text(s_body, "Live readings unavailable. Waiting for the printer...");
        return;
    }
    if (!strcmp(state.printer_state, "printing") || !strcmp(state.printer_state, "paused")) {
        endstop_status_controller_reset();
        ui_value_set_text(s_body, "Readings paused during a print.\nResume checks when the printer is idle.");
        return;
    }
    endstop_status_snapshot_t status;
    endstop_status_controller_snapshot(&status);
    int64_t now = esp_timer_get_time();
    if (!status.waiting && now >= s_next_query &&
        (!status.error[0] || now - status.updated_us >= 2000000LL)) {
        uint32_t id = endstop_status_controller_begin(s_owner);
        if (!moonraker_live_websocket_request_endstops(id))
            endstop_status_controller_failed("Could not request endstops. Retrying...");
        s_next_query = now + 2000000LL;
        endstop_status_controller_snapshot(&status);
    }
    if (status.waiting) {
        /* Keep the last rendered sample briefly; never manufacture a new state. */
        if (!status.count && now - status.updated_us < 500000LL) return;
        ui_value_set_text(s_body, "Waiting for live endstop readings...");
    } else if (!status.valid) {
        ui_value_set_text(s_body, status.error[0] ? status.error : "Waiting for the first sample...");
    } else {
        char text[1024];
        size_t used = 0;
        text[0] = 0;
        for (size_t i = 0; i < status.count; ++i) {
            int length = snprintf(text + used, sizeof(text) - used, "%s:  %s\n",
                status.items[i].name, status.items[i].triggered ? "TRIGGERED" : "OPEN");
            if (length < 0 || (size_t)length >= sizeof(text) - used) break;
            used += (size_t)length;
        }
        snprintf(text + used, sizeof(text) - used, "%s\nLive sample; refreshed every 2 seconds.\n"
            "Sensorless endstops may only trigger during homing.",
            status.truncated ? "Additional endstops omitted." : status.count ? "" : "No endstops reported.");
        ui_value_set_text(s_body, text);
    }
}

void ui_endstop_status_show(void)
{
    if (s_popup) { lv_obj_move_foreground(s_popup); return; }
    s_owner = moonraker_config_generation();
    s_next_query = 0;
    endstop_status_controller_reset();
    s_popup = ui_popup_create(lv_layer_top(), 660, 460, UI_POPUP_STANDARD);
    if (!s_popup) return;
    ui_popup_add_title(s_popup, "LIVE ENDSTOPS", false, 4);
    ui_popup_add_header_divider(s_popup, 48);
    lv_obj_t *list = ui_popup_add_list(s_popup, 24, 68, 612, 300);
    s_body = ui_popup_add_body(list, "Waiting for the first live sample...", 12, 12, 580);
    ui_popup_add_standard_footer_divider(s_popup);
    ui_popup_add_footer_action(s_popup, UI_POPUP_ACTION_CLOSE, "CLOSE", 170,
        UI_POPUP_FOOTER_RIGHT, close_cb, NULL, NULL);
    s_timer = lv_timer_create(refresh, 250, NULL);
    refresh(NULL);
}
