#include "ui_filament_recovery.h"
#include "ui_popup.h"
#include "ui_button.h"
#include "ui_theme.h"
#include "ui_value_update.h"
#include "moonraker.h"
#include "moonraker_config_controller.h"
#include "macro_controller.h"
#include <stdio.h>
#include <string.h>

static lv_obj_t *popup, *status_label, *sensor_label, *message_label;
static lv_obj_t *buttons[5], *temperature_label;
static int selected_temperature;
static lv_timer_t *refresh_timer;
static bool (*send_command)(const char *);
static uint32_t endpoint_generation, last_action_tick, action_wait_ms;
static bool action_pending;
static const char *commands[] = {
    "HMI_FILAMENT_HEAT", "HMI_FILAMENT_UNLOAD", "HMI_FILAMENT_LOAD",
    "HMI_FILAMENT_PURGE", "RESUME"
};

static bool macro_available(const char *wanted)
{
    macro_controller_status_t catalog;
    macro_controller_status(&catalog);
    if (!catalog.discovered) return false;
    for (size_t i = 0; i < catalog.count; ++i) {
        char name[MACRO_CONTROLLER_NAME_MAX];
        if (macro_controller_get(i, name, sizeof(name)) &&
            strcmp(name, wanted) == 0) return true;
    }
    return false;
}

static bool paused_here(moonraker_state_t *state)
{
    moonraker_state_snapshot(state);
    return endpoint_generation == moonraker_config_generation() &&
        state->moonraker_ok && state->live_data_ok &&
        strcmp(state->printer_state, "paused") == 0;
}

static bool sensors_allow_resume(const moonraker_filament_state_t *sensors)
{
    if (sensors->truncated) return false;
    for (size_t i = 0; i < sensors->sensor_count; ++i) {
        const moonraker_filament_sensor_t *s = &sensors->sensors[i];
        if (s->enabled && (!s->status_known || !s->filament_detected)) return false;
    }
    return true;
}

static void refresh(lv_timer_t *timer)
{
    (void)timer;
    if (!popup) return;
    moonraker_state_t state;
    bool paused = paused_here(&state);
    moonraker_filament_state_t sensors;
    moonraker_filament_state_snapshot(&sensors);
    char text[192];
    if (endpoint_generation != moonraker_config_generation()) {
        snprintf(text, sizeof(text), "Printer changed. Close and reopen recovery.");
    } else if (!state.moonraker_ok || !state.live_data_ok) {
        snprintf(text, sizeof(text), "Printer offline. Recovery actions unavailable.");
    } else {
        snprintf(text, sizeof(text), "%s | Nozzle %.0f / %.0f C",
            paused ? "Print paused" : "Print is no longer paused",
            state.nozzle_temp, state.nozzle_target);
    }
    ui_value_set_text(status_label, text);
    bool sensor_ready = sensors_allow_resume(&sensors);
    if (!sensors.discovered) snprintf(text, sizeof(text), "Filament sensor: checking");
    else if (!sensors.sensor_count) snprintf(text, sizeof(text), "No filament sensor reported. Check filament manually.");
    else snprintf(text, sizeof(text), "Filament sensor: %s", sensor_ready ? "ready (or disabled)" : "empty or checking");
    ui_value_set_text(sensor_label, text);
    bool hot = state.nozzle_target >= 170 && state.nozzle_temp >= 170 &&
        state.nozzle_temp >= state.nozzle_target - 5;
    bool homed = strchr(state.homed_axes, 'x') && strchr(state.homed_axes, 'y') &&
        strchr(state.homed_axes, 'z');
    bool cooldown = action_pending && lv_tick_elaps(last_action_tick) < action_wait_ms;
    for (size_t i = 0; i < 5; ++i) {
        bool enabled = paused && !cooldown && send_command;
        if (i < 4) enabled = enabled && macro_available(commands[i]);
        if (i > 0) enabled = enabled && homed;
        if (i > 0 && i < 4) enabled = enabled && hot;
        if (i == 4) enabled = enabled && sensors.discovered && sensor_ready;
        if (enabled) lv_obj_remove_state(buttons[i], LV_STATE_DISABLED);
        else lv_obj_add_state(buttons[i], LV_STATE_DISABLED);
    }
}

static void action(lv_event_t *event)
{
    size_t index = (size_t)(uintptr_t)lv_event_get_user_data(event);
    if (index >= 5) return;
    /* Recheck telemetry and endpoint on the click, not just on the timer. */
    refresh(NULL);
    if (lv_obj_has_state(buttons[index], LV_STATE_DISABLED)) return;
    char command[64];
    if (index == 0) snprintf(command, sizeof(command), "HMI_FILAMENT_HEAT TEMP=%d", selected_temperature);
    else snprintf(command, sizeof(command), "%s", commands[index]);
    bool sent = send_command(command);
    ui_value_set_text(message_label, sent
        ? "Command sent. Check nozzle and Console before continuing."
        : "Command could not be sent. Check connection and Console.");
    if (sent) {
        action_pending = true;
        last_action_tick = lv_tick_get();
        action_wait_ms = index == 1 || index == 2 ? 8000 : index == 3 ? 5000 : 2500;
        refresh(NULL);
    }
}

static void deleted(lv_event_t *event)
{
    (void)event;
    popup = NULL;
    if (refresh_timer) lv_timer_delete(refresh_timer);
    refresh_timer = NULL;
    send_command = NULL;
    memset(buttons, 0, sizeof(buttons));
}

void ui_filament_recovery_close(void)
{
    if (popup) lv_obj_delete(popup);
}

static void close_event(lv_event_t *event)
{
    (void)event;
    ui_filament_recovery_close();
}

static void temperature_change(lv_event_t *event)
{
    int delta = (int)(intptr_t)lv_event_get_user_data(event);
    selected_temperature += delta;
    if (selected_temperature < 170) selected_temperature = 170;
    if (selected_temperature > 255) selected_temperature = 255;
    char text[64];
    snprintf(text, sizeof(text), "Set nozzle: %d C", selected_temperature);
    ui_value_set_text(temperature_label, text);
}

void ui_filament_recovery_show(bool (*send)(const char *))
{
    if (popup) { lv_obj_move_foreground(popup); return; }
    endpoint_generation = moonraker_config_generation();
    send_command = send;
    action_pending = false;
    moonraker_state_t state;
    moonraker_state_snapshot(&state);
    selected_temperature = state.nozzle_target >= 170 && state.nozzle_target <= 255
        ? (int)state.nozzle_target : 205;
    int32_t width = lv_display_get_horizontal_resolution(NULL) - 32;
    if (width > 720) width = 720;
    int32_t height = lv_display_get_vertical_resolution(NULL) - 32;
    if (height > 500) height = 500;
    popup = ui_popup_create(lv_screen_active(), width, height, UI_POPUP_STANDARD);
    if (!popup) return;
    lv_obj_add_event_cb(popup, deleted, LV_EVENT_DELETE, NULL);
    lv_obj_set_flex_flow(popup, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(popup, UI_PAD_POPUP, 0);
    lv_obj_set_style_pad_row(popup, 10, 0);
    lv_obj_add_flag(popup, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(popup, LV_DIR_VER);
    lv_obj_t *title = lv_label_create(popup);
    lv_label_set_text(title, "FILAMENT RECOVERY / RESUME");
    lv_obj_set_width(title, LV_PCT(100));
    lv_obj_set_style_text_font(title, UI_FONT_TITLE, 0);
    lv_obj_set_style_text_color(title, UI_TEXT_BRIGHT, 0);
    status_label = lv_label_create(popup);
    sensor_label = lv_label_create(popup);
    message_label = lv_label_create(popup);
    lv_obj_t *labels[] = {status_label, sensor_label, message_label};
    for (size_t i = 0; i < 3; ++i) {
        lv_obj_set_width(labels[i], LV_PCT(100));
        lv_obj_set_style_text_font(labels[i], UI_FONT_BODY, 0);
        lv_obj_set_style_text_color(labels[i], UI_TEXT, 0);
    }
    lv_label_set_text(message_label,
        "Set temperature and tap Heat. Replace filament, load and purge.\n"
        "Remove the purge strand before Resume. Missing controls need the HMI macros.");
    lv_obj_t *temperature_row = lv_obj_create(popup);
    lv_obj_remove_style_all(temperature_row);
    lv_obj_set_size(temperature_row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(temperature_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(temperature_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *minus = ui_button_create(temperature_row, UI_BUTTON_OUTLINED, "-5 C");
    lv_obj_set_size(minus, 90, 48);
    lv_obj_add_event_cb(minus, temperature_change, LV_EVENT_CLICKED, (void *)(intptr_t)-5);
    temperature_label = lv_label_create(temperature_row);
    lv_obj_set_style_text_font(temperature_label, UI_FONT_BODY, 0);
    lv_obj_set_style_text_color(temperature_label, UI_TEXT_BRIGHT, 0);
    lv_obj_t *plus = ui_button_create(temperature_row, UI_BUTTON_OUTLINED, "+5 C");
    lv_obj_set_size(plus, 90, 48);
    lv_obj_add_event_cb(plus, temperature_change, LV_EVENT_CLICKED, (void *)(intptr_t)5);
    char temperature_text[64];
    snprintf(temperature_text, sizeof(temperature_text), "Set nozzle: %d C", selected_temperature);
    lv_label_set_text(temperature_label, temperature_text);
    lv_obj_t *row = lv_obj_create(popup);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_column(row, 10, 0);
    lv_obj_set_style_pad_row(row, 10, 0);
    static const char *names[] = {"HEAT", "UNLOAD", "LOAD 30 mm", "PURGE 10 mm", "RESUME"};
    for (size_t i = 0; i < 5; ++i) {
        buttons[i] = ui_button_create(row, i == 4 ? UI_BUTTON_SUCCESS : UI_BUTTON_OUTLINED, names[i]);
        lv_obj_set_size(buttons[i], LV_PCT(47), 54);
        lv_obj_add_event_cb(buttons[i], action, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
    }
    lv_obj_t *close = ui_button_create(row, UI_BUTTON_CLOSE, "CLOSE");
    lv_obj_set_size(close, LV_PCT(47), 54);
    lv_obj_add_event_cb(close, close_event, LV_EVENT_CLICKED, NULL);
    refresh(NULL);
    refresh_timer = lv_timer_create(refresh, 500, NULL);
}
