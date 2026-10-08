#include "ui_motion_diagnostics.h"
#include <stdio.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "motion_diagnostics_controller.h"
#include "moonraker.h"
#include "moonraker_config_controller.h"
#include "ui_button.h"
#include "ui_popup.h"
#include "ui_theme.h"
#include "ui_toast.h"
#include "ui_value_update.h"

typedef enum { VIEW_LIMITS, VIEW_DISTANCE, VIEW_DRIVERS } motion_view_t;
typedef struct {
    lv_obj_t *popup, *body, *driver_selector, *status;
    lv_obj_t *limits[4];
    lv_obj_t *mode, *pitch, *count, *pitch_label, *count_label, *result, *reference;
    lv_obj_t *editor, *editor_value, *editor_target;
    lv_timer_t *timer;
    uint32_t owner;
    motion_view_t view;
    size_t selector_count;
    char selector_names[MOTION_DRIVER_MAX][MOTION_DRIVER_NAME_MAX];
    motion_diagnostics_snapshot_t snapshot;
    char text[1200];
} motion_ui_state_t;

static motion_ui_state_t *s;

static bool init(void)
{
    if (s) return true;
    s = heap_caps_calloc(1, sizeof(*s), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s) s = heap_caps_calloc(1, sizeof(*s), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    return s != NULL;
}

static void label_text(lv_obj_t *label, const char *text)
{
    ui_value_set_text(label, text);
}

static void close_editor(void)
{
    if (s->editor) lv_obj_delete(s->editor);
    s->editor = s->editor_value = s->editor_target = NULL;
}

static void close_popup(void)
{
    if (!s) return;
    close_editor();
    if (s->timer) lv_timer_delete(s->timer);
    if (s->popup) lv_obj_delete(s->popup);
    /* Permanent allocation remains; all LVGL references are retired. */
    memset(s, 0, sizeof(*s));
}
static void close_cb(lv_event_t *event) { (void)event; close_popup(); }
static void refresh(lv_timer_t *timer);

static bool owns_printer(void)
{
    return s->owner == moonraker_config_generation();
}

static void calculate_cb(lv_event_t *event)
{
    (void)event;
    if (!owns_printer()) {
        ui_toast_show(UI_STATUS_INFO, "PRINTER CHANGED", "Close and reopen the calculator.");
        return;
    }
    double result;
    const char *error;
    if (!motion_axis_distance_calculate(lv_textarea_get_text(s->pitch),
            lv_textarea_get_text(s->count), &result, &error)) {
        label_text(s->result, error);
        ui_apply_label_warning(s->result);
        return;
    }
    snprintf(s->text, sizeof(s->text), "rotation_distance: %.6f mm\nNo printer settings have been changed.", result);
    label_text(s->result, s->text);
    ui_apply_label_success(s->result);
}

static void mode_cb(lv_event_t *event)
{
    (void)event;
    bool screw = lv_dropdown_get_selected(s->mode) == 1;
    label_text(s->pitch_label, screw ? "SCREW THREAD PITCH (mm)" : "BELT PITCH (mm)");
    label_text(s->count_label, screw ? "NUMBER OF THREAD STARTS" : "PULLEY TEETH");
    lv_textarea_set_text(s->pitch, "2");
    lv_textarea_set_text(s->count, screw ? "4" : "20");
    label_text(s->result, "Enter the hardware measurements, then Calculate.");
    ui_apply_label_primary(s->result);
}

static void editor_done_cb(lv_event_t *event)
{
    (void)event;
    if (owns_printer() && s->editor_target && s->editor_value)
        lv_textarea_set_text(s->editor_target, lv_textarea_get_text(s->editor_value));
    close_editor();
    label_text(s->result, "Values changed. Tap Calculate for a new result.");
    ui_apply_label_primary(s->result);
}
static void editor_cancel_cb(lv_event_t *event) { (void)event; close_editor(); }
static void field_cb(lv_event_t *event)
{
    if (!owns_printer()) return;
    close_editor();
    s->editor_target = lv_event_get_target(event);
    s->editor = ui_popup_create(lv_layer_top(), 640, 400, UI_POPUP_STANDARD);
    if (!s->editor) { s->editor_target = NULL; return; }
    ui_popup_add_title(s->editor, "EDIT MEASUREMENT", false, 4);
    ui_popup_add_header_divider(s->editor, 48);
    s->editor_value = ui_popup_add_textarea(s->editor, 584, 48, LV_ALIGN_TOP_MID,
        0, 62, true, false, 18, "", lv_textarea_get_text(s->editor_target), "0123456789.");
    ui_popup_add_keyboard(s->editor, s->editor_value, 584, 182,
        LV_ALIGN_TOP_MID, 0, 124, LV_KEYBOARD_MODE_NUMBER);
    ui_popup_add_standard_footer_divider(s->editor);
    ui_popup_add_footer_action(s->editor, UI_POPUP_ACTION_CANCEL, "CANCEL", 150,
        UI_POPUP_FOOTER_LEFT, editor_cancel_cb, NULL, NULL);
    ui_popup_add_footer_action(s->editor, UI_POPUP_ACTION_CONFIRM, "DONE", 150,
        UI_POPUP_FOOTER_RIGHT, editor_done_cb, NULL, NULL);
}

static void format_value(char *out, size_t size, bool valid, double value, const char *unit)
{
    if (valid) snprintf(out, size, "%.2f %s", value, unit);
    else snprintf(out, size, "Not reported");
}

/* Compare the live widget so newly-created selectors and discovery transitions
 * populate correctly without a second cached options store. */
static void set_driver_options(const char *options)
{
    if (strcmp(lv_dropdown_get_options(s->driver_selector), options))
        lv_dropdown_set_options(s->driver_selector, options);
}

static void render_limits(bool live, const char *status_override)
{
    static const char *units[] = {"mm/s", "mm/s^2", "mm/s", "%"};
    for (size_t i = 0; i < 4; ++i) {
        char value[64];
        format_value(value, sizeof(value), live && s->snapshot.limit_valid[i],
            s->snapshot.limits[i] * (i == 3 ? 100 : 1), units[i]);
        label_text(s->limits[i], value);
    }
    label_text(s->status, status_override ? status_override : live ? "CURRENT RUNTIME LIMITS" : "LIVE LIMITS UNAVAILABLE");
    if (live) ui_value_set_color(s->status, UI_TEXT_BRIGHT, 0); else ui_value_set_color(s->status, UI_TEXT_DIM, 0);
}

static void driver_selected_cb(lv_event_t *event) { (void)event; refresh(NULL); }

static void render_drivers(bool live, const char *status_override)
{
    if (!live) {
        lv_obj_add_state(s->driver_selector, LV_STATE_DISABLED);
        label_text(s->status, status_override ? status_override : "LIVE DRIVER DATA UNAVAILABLE");
        ui_value_set_color(s->status, UI_TEXT_DIM, 0);
        label_text(s->body, "The printer is offline or not ready.\nWaiting for fresh driver readings...");
        return;
    }
    bool rebuild = s->selector_count != s->snapshot.driver_count;
    for (size_t i = 0; !rebuild && i < s->selector_count; ++i)
        rebuild = strcmp(s->selector_names[i], s->snapshot.drivers[i].name) != 0;
    if (rebuild) {
        size_t selected = lv_dropdown_get_selected(s->driver_selector);
        char previous[MOTION_DRIVER_NAME_MAX] = "";
        if (selected < s->selector_count) snprintf(previous, sizeof(previous), "%s", s->selector_names[selected]);
        char options[MOTION_DRIVER_MAX * (MOTION_DRIVER_NAME_MAX + 1)];
        size_t used = 0, restore = 0;
        options[0] = 0;
        for (size_t i = 0; i < s->snapshot.driver_count; ++i) {
            const char *name = s->snapshot.drivers[i].name;
            /* Fixed-size name arrays share the UI state; use an overlap-safe copy. */
            memmove(s->selector_names[i], name, sizeof(s->selector_names[i]));
            s->selector_names[i][sizeof(s->selector_names[i]) - 1] = '\0';
            if (!strcmp(previous, name)) restore = i;
            int written = snprintf(options + used, sizeof(options) - used, "%s%s", i ? "\n" : "", name);
            if (written > 0 && (size_t)written < sizeof(options) - used) used += (size_t)written;
        }
        s->selector_count = s->snapshot.driver_count;
        set_driver_options(options[0] ? options : "No TMC drivers reported");
        lv_dropdown_set_selected(s->driver_selector, (uint32_t)restore);
    }
    if (!s->snapshot.driver_count) {
        set_driver_options(s->snapshot.discovered ? "No TMC drivers reported" : "Waiting for drivers...");
        lv_obj_add_state(s->driver_selector, LV_STATE_DISABLED);
        label_text(s->status, s->snapshot.discovered ? "NO TMC DRIVERS DETECTED" : "WAITING FOR DRIVER DISCOVERY");
        ui_value_set_color(s->status, UI_TEXT_DIM, 0);
        label_text(s->body, "TMC diagnostics require drivers exposed by Klipper over UART or SPI.\nStandalone drivers do not report this data.");
        return;
    }
    lv_obj_remove_state(s->driver_selector, LV_STATE_DISABLED);
    size_t selected = lv_dropdown_get_selected(s->driver_selector);
    if (selected >= s->snapshot.driver_count) return;
    const motion_driver_snapshot_t *driver = &s->snapshot.drivers[selected];
    char run[48], hold[48], temperature[64];
    format_value(run, sizeof(run), driver->run_valid, driver->run_current, "A RMS");
    format_value(hold, sizeof(hold), driver->hold_valid, driver->hold_current, "A RMS");
    format_value(temperature, sizeof(temperature), driver->temperature_valid, driver->temperature, "C");
    const char *status = !driver->status_valid ? "NO DRIVER STATUS SAMPLE" :
        driver->fault ? "DRIVER FAULT REPORTED" : driver->warning ? "DRIVER WARNING REPORTED" : "NO MONITORED FAULT FLAGS REPORTED";
    label_text(s->status, status);
    if (driver->fault) ui_value_set_color(s->status, ui_theme_get_active() == UI_THEME_CLASSIC ? UI_TEXT_ERROR : UI_DANGER_BRIGHT, 0);
    else if (driver->warning) ui_value_set_color(s->status, UI_WARN, 0);
    else if (driver->status_valid) ui_value_set_color(s->status, UI_OK_BRIGHT, 0);
    else ui_value_set_color(s->status, UI_TEXT_DIM, 0);
    snprintf(s->text, sizeof(s->text),
        "Run current: %s\nHold current: %s\nDriver temperature: %s\n\n%s\n\n"
        "These are Klipper's last reported driver readings. Disabled drivers may have no status sample. "
        "Open-load flags can occur at low current or with motors disabled.%s",
        run, hold, temperature,
        driver->status_valid ? (driver->flags[0] ? driver->flags : "No monitored warning/fault flags in the sample.") :
            "Status unavailable: driver disabled, not yet sampled, or unsupported.",
        s->snapshot.drivers_truncated ? " Additional drivers omitted (12 shown)." : "");
    label_text(s->body, s->text);
}

static void refresh(lv_timer_t *timer)
{
    (void)timer;
    if (!s || !s->popup) return;
    if (!owns_printer()) {
        close_editor();
        if (s->view == VIEW_DISTANCE) {
            label_text(s->result, "Printer changed. Close and reopen the calculator.");
            label_text(s->reference, "");
        } else {
            if (s->view == VIEW_LIMITS) render_limits(false, "PRINTER CHANGED: CLOSE AND REOPEN");
            else render_drivers(false, "PRINTER CHANGED: CLOSE AND REOPEN");
        }
        return;
    }
    motion_diagnostics_controller_snapshot(&s->snapshot);
    moonraker_state_t state;
    moonraker_state_snapshot(&state);
    bool live = state.moonraker_ok && state.live_data_ok;
    if (s->view == VIEW_LIMITS) render_limits(live, NULL);
    else if (s->view == VIEW_DRIVERS) render_drivers(live, NULL);
    else {
        char x[40], y[40], z[40];
        format_value(x, sizeof(x), s->snapshot.rotation_valid[0], s->snapshot.rotation_distance[0], "mm");
        format_value(y, sizeof(y), s->snapshot.rotation_valid[1], s->snapshot.rotation_distance[1], "mm");
        format_value(z, sizeof(z), s->snapshot.rotation_valid[2], s->snapshot.rotation_distance[2], "mm");
        snprintf(s->text, sizeof(s->text), "Config reference: stepper_x %s / stepper_y %s / stepper_z %s", x, y, z);
        label_text(s->reference, s->text);
    }
}

static void show_view(motion_view_t view)
{
    if (!init()) return;
    close_popup();
    s->owner = moonraker_config_generation();
    s->view = view;
    s->popup = ui_popup_create(lv_layer_top(), view == VIEW_LIMITS ? 720 : 780,
        view == VIEW_LIMITS ? 440 : 500, UI_POPUP_STANDARD);
    if (!s->popup) return;
    ui_popup_add_title(s->popup, view == VIEW_LIMITS ? "MOTION LIMITS" :
        view == VIEW_DRIVERS ? "TMC DRIVER DIAGNOSTICS" : "AXIS DISTANCE CALCULATOR", false, 4);
    ui_popup_add_header_divider(s->popup, 48);
    if (view == VIEW_LIMITS) {
        s->status = ui_popup_add_body(s->popup, "WAITING FOR LIVE LIMITS", 28, 66, 664);
        ui_apply_text_caption(s->status);
        const char *labels[] = {"Maximum velocity", "Maximum acceleration", "Square-corner velocity", "Minimum cruise ratio"};
        for (size_t i = 0; i < 4; ++i) {
            ui_popup_add_body(s->popup, labels[i], 28, 106 + (int)i * 48, 350);
            s->limits[i] = ui_popup_add_body(s->popup, "Not reported", 406, 106 + (int)i * 48, 286);
        }
        ui_popup_add_body(s->popup, "Live limits may differ from printer.cfg during a print.", 28, 316, 664);
    } else if (view == VIEW_DRIVERS) {
        s->driver_selector = lv_dropdown_create(s->popup);
        lv_obj_set_size(s->driver_selector, 732, 44);
        lv_obj_set_pos(s->driver_selector, 24, 64);
        lv_dropdown_set_options(s->driver_selector, "Waiting for drivers...");
        lv_obj_add_event_cb(s->driver_selector, driver_selected_cb, LV_EVENT_VALUE_CHANGED, NULL);
        s->status = ui_popup_add_body(s->popup, "WAITING FOR DRIVER STATUS", 24, 122, 732);
        ui_apply_text_caption(s->status);
        lv_obj_t *list = ui_popup_add_list(s->popup, 24, 156, 732, 252);
        s->body = ui_popup_add_body(list, "Waiting for data...", 12, 12, 690);
    } else {
        ui_popup_add_body(s->popup, "Calculate from belt/pulley or leadscrew geometry.\nRetain gear_ratio for geared axes.", 24, 64, 732);
        s->mode = lv_dropdown_create(s->popup);
        lv_obj_set_size(s->mode, 732, 44);
        lv_obj_set_pos(s->mode, 24, 124);
        lv_dropdown_set_options(s->mode, "Belt and pulley\nLeadscrew");
        lv_obj_add_event_cb(s->mode, mode_cb, LV_EVENT_VALUE_CHANGED, NULL);
        s->pitch_label = ui_popup_add_body(s->popup, "BELT PITCH (mm)", 24, 184, 354);
        s->count_label = ui_popup_add_body(s->popup, "PULLEY TEETH", 402, 184, 354);
        ui_apply_text_caption(s->pitch_label); ui_apply_text_caption(s->count_label);
        s->pitch = ui_popup_add_textarea(s->popup, 354, 46, LV_ALIGN_TOP_LEFT,
            24, 216, true, false, 18, "Pitch", "2", "0123456789.");
        s->count = ui_popup_add_textarea(s->popup, 354, 46, LV_ALIGN_TOP_LEFT,
            402, 216, true, false, 18, "Teeth or thread starts", "20", "0123456789");
        lv_obj_add_event_cb(s->pitch, field_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_add_event_cb(s->count, field_cb, LV_EVENT_CLICKED, NULL);
        s->result = ui_popup_add_body(s->popup, "Enter the hardware measurements, then Calculate.", 24, 284, 732);
        s->reference = ui_popup_add_body(s->popup, "Config reference: waiting for printer data.", 24, 340, 732);
        ui_apply_text_caption(s->reference);
        lv_obj_t *note = ui_popup_add_body(s->popup, "For screws, enter thread pitch and thread-start count.\nNo motion or configuration changes are sent.", 24, 392, 732);
        ui_apply_text_caption(note);
    }
    ui_popup_add_standard_footer_divider(s->popup);
    ui_popup_add_footer_action(s->popup, UI_POPUP_ACTION_CLOSE, "CLOSE", 170,
        view == VIEW_DISTANCE ? UI_POPUP_FOOTER_LEFT : UI_POPUP_FOOTER_RIGHT, close_cb, NULL, NULL);
    if (view == VIEW_DISTANCE)
        ui_popup_add_footer_action(s->popup, UI_POPUP_ACTION_PRIMARY, "CALCULATE", 180,
            UI_POPUP_FOOTER_RIGHT, calculate_cb, NULL, NULL);
    s->timer = lv_timer_create(refresh, 500, NULL);
    refresh(NULL);
}

static void limits_cb(lv_event_t *event) { (void)event; show_view(VIEW_LIMITS); }
static void distance_cb(lv_event_t *event) { (void)event; show_view(VIEW_DISTANCE); }
static void drivers_cb(lv_event_t *event) { (void)event; show_view(VIEW_DRIVERS); }

void ui_motion_diagnostics_create(lv_obj_t *card)
{
    if (!card || !init()) return;
    const char *labels[] = {"LIMITS", "DISTANCE", "DRIVERS"};
    lv_event_cb_t callbacks[] = {limits_cb, distance_cb, drivers_cb};
    for (size_t i = 0; i < 3; ++i) {
        lv_obj_t *button = ui_button_create(card, UI_BUTTON_OUTLINED, labels[i]);
        if (!button) continue;
        lv_obj_set_size(button, 110, 34);
        lv_obj_set_pos(button, 16 + (int)i * 124, 88);
        lv_obj_add_event_cb(button, callbacks[i], LV_EVENT_CLICKED, NULL);
    }
}

void ui_motion_diagnostics_hide(void) { close_popup(); }
