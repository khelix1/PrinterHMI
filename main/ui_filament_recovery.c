#include "ui_filament_recovery.h"
#include "ui_popup.h"
#include "ui_button.h"
#include "ui_widgets.h"
#include "ui_theme.h"
#include "ui_value_update.h"
#include "ui_form_dialog.h"
#include "moonraker.h"
#include "moonraker_config_controller.h"
#include "macro_controller.h"
#include <stdio.h>
#include <string.h>

static lv_obj_t *popup, *status_label, *sensor_label, *message_label;
static lv_obj_t *buttons[5], *temperature_buttons[2], *temperature_label;
static int selected_temperature;
static char recovery_tool[MOONRAKER_HOTEND_NAME_MAX];
static bool tool_changed;
static bool resume_pending;
static uint32_t resume_owner, resume_job, resume_tick;
static const uint32_t resume_wait_ms = 30000;
static lv_timer_t *refresh_timer;
static bool (*send_command)(const char *);
static uint32_t endpoint_generation, last_action_tick, action_wait_ms;
static bool action_pending, dispatching;
static uint32_t popup_epoch, pending_generation, recovery_job, pending_job;
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

static bool paused_here(const moonraker_state_t *state)
{
    return endpoint_generation == moonraker_config_generation() &&
        state->print_job_revision == recovery_job &&
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

static bool action_allowed(size_t index, const moonraker_state_t *state,
                           const moonraker_filament_state_t *sensors)
{
    if (!refresh_timer || !send_command || !paused_here(state) || tool_changed ||
        !recovery_tool[0] || strcmp(recovery_tool, state->active_hotend)) return false;
    if (resume_pending && resume_owner == endpoint_generation && resume_job == recovery_job)
        return false;
    if (action_pending && pending_generation == endpoint_generation &&
        pending_job == recovery_job && lv_tick_elaps(last_action_tick) < action_wait_ms)
        return false;
    if (index < 4 && !macro_available(commands[index])) return false;
    if (index == 0) return true;
    if (!strchr(state->homed_axes, 'x') || !strchr(state->homed_axes, 'y') ||
        !strchr(state->homed_axes, 'z')) return false;
    if (index == 4) return sensors->discovered && sensors_allow_resume(sensors);
    return state->nozzle_target >= 170 && state->nozzle_temp >= 170 &&
           state->nozzle_temp >= state->nozzle_target - 5;
}

static void follow_resume(const moonraker_state_t *state)
{
    if (!resume_pending) return;
    if (resume_owner != moonraker_config_generation() ||
        (state->moonraker_ok && state->live_data_ok &&
         state->print_job_revision != resume_job)) {
        resume_pending = false;
        return;
    }
    if (!state->moonraker_ok || !state->live_data_ok) return;
    if (strcmp(state->printer_state, "paused")) {
        resume_pending = false;
        if (!strcmp(state->printer_state, "printing"))
            ui_value_set_text(message_label, "Print resumed.");
        else
            ui_value_set_text(message_label, "Print is no longer paused. Check Printer for its current state.");
    } else if (lv_tick_elaps(resume_tick) >= resume_wait_ms) {
        resume_pending = false;
        ui_value_set_text(message_label, "Still paused. Check Console and nozzle before retrying Resume.");
        ui_value_set_color(message_label, UI_WARN, 0);
    } else {
        ui_value_set_text(message_label, "Resume requested. Waiting for the printer to leave Paused...");
        ui_value_set_color(message_label, UI_TEXT_DIM, 0);
    }
}

static void refresh(lv_timer_t *timer)
{
    if (!popup || (timer && timer != refresh_timer)) return;
    moonraker_state_t state;
    moonraker_state_snapshot(&state);
    bool paused = paused_here(&state);
    if (paused && strcmp(recovery_tool, state.active_hotend)) tool_changed = true;
    follow_resume(&state);
    moonraker_filament_state_t sensors;
    moonraker_filament_state_snapshot(&sensors);
    char text[192];
    if (!refresh_timer) {
        snprintf(text, sizeof(text), "Recovery refresh unavailable. Close and reopen recovery.");
    } else if (endpoint_generation != moonraker_config_generation()) {
        snprintf(text, sizeof(text), "Printer changed. Close and reopen recovery.");
    } else if (state.print_job_revision != recovery_job) {
        snprintf(text, sizeof(text), "Print changed. Close and reopen recovery.");
    } else if (!state.moonraker_ok || !state.live_data_ok) {
        snprintf(text, sizeof(text), "Printer offline. Recovery actions unavailable.");
    } else if (tool_changed || !recovery_tool[0]) {
        snprintf(text, sizeof(text), "%s. Close and reopen recovery.",
            tool_changed ? "Active extruder changed" : "Active extruder unavailable");
    } else {
        snprintf(text, sizeof(text), "%s | %s: %.0f / %.0f C",
            paused ? "Print paused" : "Print is no longer paused", recovery_tool,
            state.nozzle_temp, state.nozzle_target);
    }
    ui_value_set_text(status_label, text);
    ui_value_set_color(status_label,!refresh_timer || endpoint_generation!=moonraker_config_generation() || !state.moonraker_ok || !state.live_data_ok ? UI_WARN : UI_TEXT_BRIGHT,0);
    bool sensor_ready = sensors_allow_resume(&sensors);
    if (endpoint_generation != moonraker_config_generation())
        snprintf(text, sizeof(text), "Filament sensor unavailable for this printer.");
    else if (!refresh_timer || state.print_job_revision != recovery_job)
        snprintf(text, sizeof(text), "Filament sensor unavailable for this recovery.");
    else if (!state.moonraker_ok || !state.live_data_ok)
        snprintf(text, sizeof(text), "Filament sensor unavailable while offline.");
    else if (!sensors.discovered) snprintf(text, sizeof(text), "Filament sensor: checking");
    else if (!sensors.sensor_count) snprintf(text, sizeof(text), "No filament sensor reported. Check filament manually.");
    else snprintf(text, sizeof(text), "Filament sensor: %s", sensor_ready ? "ready (or disabled)" : "empty or checking");
    ui_value_set_text(sensor_label, text);
    ui_value_set_color(sensor_label,sensors.discovered && !sensor_ready ? UI_WARN : UI_TEXT_DIM,0);
    for (size_t i = 0; i < 5; ++i) {
        if (!buttons[i]) continue;
        bool enabled = !dispatching && action_allowed(i, &state, &sensors);
        if (enabled) lv_obj_remove_state(buttons[i], LV_STATE_DISABLED);
        else lv_obj_add_state(buttons[i], LV_STATE_DISABLED);
    }
    bool target_enabled = buttons[0] && !lv_obj_has_state(buttons[0], LV_STATE_DISABLED);
    for (size_t i = 0; i < 2; ++i) {
        if (!temperature_buttons[i]) continue;
        if (target_enabled) lv_obj_remove_state(temperature_buttons[i], LV_STATE_DISABLED);
        else lv_obj_add_state(temperature_buttons[i], LV_STATE_DISABLED);
    }
}

static void action(lv_event_t *event)
{
    if (!event || lv_event_get_code(event) != LV_EVENT_CLICKED || !popup || !refresh_timer || dispatching) return;
    size_t index = (size_t)(uintptr_t)lv_event_get_user_data(event);
    if (index >= 5 || !buttons[index] || lv_event_get_target_obj(event) != buttons[index] ||
        lv_event_get_current_target_obj(event) != buttons[index]) return;
    lv_obj_t *owner_popup = popup;
    uint32_t epoch = popup_epoch, owner = endpoint_generation, job = recovery_job;
    /* Recheck telemetry and endpoint on the click, not just on the timer. */
    refresh(NULL);
    if (epoch != popup_epoch || owner_popup != popup || !buttons[index] ||
        lv_obj_has_state(buttons[index], LV_STATE_DISABLED) || !send_command) return;
    char command[64];
    if (index == 0) snprintf(command, sizeof(command), "HMI_FILAMENT_HEAT TEMP=%d", selected_temperature);
    else snprintf(command, sizeof(command), "%s", commands[index]);
    dispatching = true;
    refresh(NULL);
    if (epoch != popup_epoch || owner_popup != popup || !buttons[index] ||
        owner != moonraker_config_generation() || !send_command) {
        dispatching = false;
        refresh(NULL);
        return;
    }
    moonraker_filament_state_t latest_sensors;
    moonraker_state_t latest;
    moonraker_filament_state_snapshot(&latest_sensors);
    moonraker_state_snapshot(&latest);
    if (!action_allowed(index, &latest, &latest_sensors) ||
        epoch != popup_epoch || owner_popup != popup || !buttons[index] ||
        owner != moonraker_config_generation() || !send_command) {
        dispatching = false;
        refresh(NULL);
        return;
    }
    bool sent = send_command(command);
    dispatching = false;
    /* Accepted commands keep their cooldown even across close/reopen. */
    if (sent && index == 4) {
        resume_pending = true;
        resume_owner = owner;
        resume_job = job;
        resume_tick = lv_tick_get();
    } else if (sent) {
        action_pending = true;
        pending_generation = owner;
        pending_job = job;
        last_action_tick = lv_tick_get();
        action_wait_ms = index == 1 || index == 2 ? 8000 : index == 3 ? 5000 : 2500;
    }
    if (epoch != popup_epoch || owner_popup != popup || owner != moonraker_config_generation()) return;
    ui_value_set_text(message_label, sent
        ? "Command sent. Check nozzle and Console before continuing."
        : "Command could not be sent. Check connection and retry.");
    ui_value_set_color(message_label,sent?UI_TEXT_DIM:UI_DANGER_BRIGHT,0);
    refresh(NULL);
}

static void field_deleted(lv_event_t *event)
{
    lv_obj_t *field = lv_event_get_target_obj(event);
    if (field == status_label) status_label = NULL;
    if (field == sensor_label) sensor_label = NULL;
    if (field == message_label) message_label = NULL;
    for (unsigned i = 0; i < 2; ++i) if (field == temperature_buttons[i]) temperature_buttons[i] = NULL;
    if (field == temperature_label) temperature_label = NULL;
    for (unsigned i = 0; i < 5; ++i) if (field == buttons[i]) buttons[i] = NULL;
}

static void deleted(lv_event_t *event)
{
    if (lv_event_get_target_obj(event) != popup) return;
    popup = NULL;
    ++popup_epoch;
    status_label = sensor_label = message_label = temperature_label = NULL;
    if (refresh_timer) lv_timer_delete(refresh_timer);
    refresh_timer = NULL;
    send_command = NULL;
    memset(buttons, 0, sizeof(buttons));
    memset(temperature_buttons, 0, sizeof(temperature_buttons));
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
    if (!event || lv_event_get_code(event) != LV_EVENT_CLICKED || !popup || dispatching) return;
    size_t index = (size_t)(uintptr_t)lv_event_get_user_data(event);
    if (index >= 2 || !temperature_buttons[index] ||
        lv_event_get_target_obj(event) != temperature_buttons[index] ||
        lv_event_get_current_target_obj(event) != temperature_buttons[index]) return;
    uint32_t epoch = popup_epoch;
    refresh(NULL);
    if (epoch != popup_epoch || !temperature_buttons[index] ||
        lv_obj_has_state(temperature_buttons[index], LV_STATE_DISABLED)) return;
    int delta = index ? 5 : -5;
    selected_temperature += delta;
    if (selected_temperature < 170) selected_temperature = 170;
    if (selected_temperature > 255) selected_temperature = 255;
    char text[64];
    snprintf(text, sizeof(text), "%d C", selected_temperature);
    ui_value_set_text(temperature_label, text);
}

void ui_filament_recovery_show(bool (*send)(const char *))
{
    moonraker_state_t state;
    moonraker_state_snapshot(&state);
    if (popup && (endpoint_generation != moonraker_config_generation() ||
        recovery_job != state.print_job_revision)) ui_filament_recovery_close();
    if (popup) { lv_obj_move_foreground(popup); return; }
    recovery_job = state.print_job_revision;
    endpoint_generation = moonraker_config_generation();
    send_command = send;
    snprintf(recovery_tool, sizeof(recovery_tool), "%s", state.active_hotend);
    tool_changed = false;

    selected_temperature = state.nozzle_target >= 170 && state.nozzle_target <= 255
        ? (int)state.nozzle_target : 205;
    popup = ui_form_dialog("Filament recovery", 840, 568, UI_POPUP_STANDARD);
    if (!popup) { send_command = NULL; return; }
    ui_form_divide_title(popup);
    ++popup_epoch;
    lv_obj_add_event_cb(popup, deleted, LV_EVENT_DELETE, NULL);
    lv_obj_t *body = ui_form_body(popup);
    lv_obj_set_scrollbar_mode(body, LV_SCROLLBAR_MODE_OFF);
    status_label = ui_form_text(body, "", UI_FONT_BODY, UI_TEXT_BRIGHT);
    sensor_label = ui_form_text(body, "", UI_FONT_CAPTION, UI_TEXT_DIM);
    message_label = ui_form_text(body,
        "Set the target and tap Heat. Replace filament, load and purge; remove the purge strand before Resume. Missing controls require HMI macros.",
        UI_FONT_CAPTION, UI_TEXT_DIM);
    lv_obj_t *labels[] = {status_label, sensor_label, message_label};
    for (unsigned i = 0; i < 3; ++i)
        lv_obj_add_event_cb(labels[i], field_deleted, LV_EVENT_DELETE, NULL);
    ui_form_text(body, "Nozzle target", UI_FONT_CAPTION, UI_TEXT_DIM);
    static const int32_t temperature_columns[] = {100, LV_GRID_FR(1), 100, LV_GRID_TEMPLATE_LAST};
    static const int32_t one_row[] = {LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
    lv_obj_t *temperature_row = lv_obj_create(body);
    lv_obj_remove_style_all(temperature_row);
    lv_obj_set_size(temperature_row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_grid_dsc_array(temperature_row, temperature_columns, one_row);
    lv_obj_set_style_pad_column(temperature_row, 8, 0);
    ui_apply_divider_border(temperature_row, LV_BORDER_SIDE_BOTTOM);
    lv_obj_set_style_pad_bottom(temperature_row, 8, 0);
    lv_obj_t *minus = ui_button_create(temperature_row, UI_BUTTON_OUTLINED, "-5 C");
    lv_obj_set_height(minus, 52);
    lv_obj_set_grid_cell(minus, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_START, 0, 1);
    lv_obj_add_event_cb(minus, ui_form_action_fit, LV_EVENT_SIZE_CHANGED, NULL);
    temperature_buttons[0] = minus;
    lv_obj_add_event_cb(minus, field_deleted, LV_EVENT_DELETE, NULL);
    lv_obj_add_event_cb(minus, temperature_change, LV_EVENT_CLICKED, (void *)(uintptr_t)0);
    temperature_label = ui_form_text(temperature_row, "", UI_FONT_BODY_LARGE, UI_TEXT_BRIGHT);
    lv_obj_set_style_text_align(temperature_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_grid_cell(temperature_label, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_CENTER, 0, 1);
    lv_obj_add_event_cb(temperature_label, field_deleted, LV_EVENT_DELETE, NULL);
    lv_obj_t *plus = ui_button_create(temperature_row, UI_BUTTON_OUTLINED, "+5 C");
    lv_obj_set_height(plus, 52);
    lv_obj_set_grid_cell(plus, LV_GRID_ALIGN_STRETCH, 2, 1, LV_GRID_ALIGN_START, 0, 1);
    lv_obj_add_event_cb(plus, ui_form_action_fit, LV_EVENT_SIZE_CHANGED, NULL);
    temperature_buttons[1] = plus;
    lv_obj_add_event_cb(plus, field_deleted, LV_EVENT_DELETE, NULL);
    lv_obj_add_event_cb(plus, temperature_change, LV_EVENT_CLICKED, (void *)(uintptr_t)1);
    char temperature_text[32];
    snprintf(temperature_text, sizeof(temperature_text), "%d C", selected_temperature);
    lv_label_set_text(temperature_label, temperature_text);
    static const int32_t columns[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static const int32_t rows[] = {LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
    lv_obj_t *row = lv_obj_create(body);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_grid_dsc_array(row, columns, rows);
    lv_obj_set_style_pad_column(row, 8, 0);
    lv_obj_set_style_pad_row(row, 8, 0);
    static const char *names[] = {"Heat", "Unload", "Load 30 mm", "Purge 10 mm", "Resume"};
    for (size_t i = 0; i < 5; ++i) {
        buttons[i] = ui_button_create(row, i == 4 ? UI_BUTTON_SUCCESS : UI_BUTTON_OUTLINED, names[i]);
        lv_obj_set_size(buttons[i], LV_PCT(100), 54);
        lv_obj_set_grid_cell(buttons[i], LV_GRID_ALIGN_STRETCH, i % 3, i == 4 ? 2 : 1, LV_GRID_ALIGN_START, i / 3, 1);
        lv_obj_add_event_cb(buttons[i], ui_form_action_fit, LV_EVENT_SIZE_CHANGED, NULL);
        lv_obj_add_event_cb(buttons[i], field_deleted, LV_EVENT_DELETE, NULL);
        lv_obj_add_event_cb(buttons[i], action, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
    }
    ui_form_action(popup, UI_BUTTON_CLOSE, "Close", close_event, NULL);
    refresh_timer = lv_timer_create(refresh, 500, NULL);
    refresh(NULL);
    ui_form_finish(popup);
    /* The grid has its final width now; fit from the preferred readable size
     * rather than retaining a font chosen during its initial empty layout. */
    for (unsigned i = 0; i < 5; ++i)
        ui_text_fit_single_line(lv_obj_get_child(buttons[i], 0), UI_FONT_BODY_LARGE);
}
