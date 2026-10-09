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

static void editor_deleted(lv_event_t *event)
{
    if (s && lv_event_get_target(event) == s->editor)
        s->editor = s->editor_value = s->editor_target = NULL;
}
static void popup_deleted(lv_event_t *event)
{
    if (!s || lv_event_get_target(event) != s->popup) return;
    close_editor();
    if (s->timer) lv_timer_delete(s->timer);
    memset(s, 0, sizeof(*s));
}

static void close_popup(void)
{
    if (!s) return;
    close_editor();
    if (s->timer) lv_timer_delete(s->timer);
    s->timer = NULL;
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
static int32_t popup_width(int32_t preferred)
{
    int32_t available = lv_display_get_horizontal_resolution(NULL) - 32;
    return available < preferred ? available : preferred;
}
static int32_t popup_height(int32_t preferred)
{
    int32_t available = lv_display_get_vertical_resolution(NULL) - 32;
    return available < preferred ? available : preferred;
}
static lv_obj_t *layout_body(lv_obj_t *popup, const char *title)
{
    lv_obj_set_flex_flow(popup, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(popup, UI_PAD_POPUP, 0);
    lv_obj_set_style_pad_row(popup, UI_GAP_CARD, 0);
    lv_obj_t *label = lv_label_create(popup);
    lv_label_set_text(label, title);
    ui_apply_custom_label_style(label, UI_FONT_TITLE, UI_TEXT_BRIGHT);
    lv_obj_set_width(label, LV_PCT(100));
    lv_obj_t *body = lv_obj_create(popup);
    lv_obj_remove_style_all(body);
    lv_obj_set_size(body, LV_PCT(100), 0);
    lv_obj_set_flex_grow(body, 1);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(body, UI_GAP_CARD, 0);
    lv_obj_add_flag(body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(body, LV_DIR_VER);
    return body;
}
static lv_obj_t *layout_label(lv_obj_t *parent, const char *text, bool caption)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    ui_apply_custom_label_style(label, caption ? UI_FONT_CAPTION : UI_FONT_BODY,
        caption ? UI_TEXT_MUTED : UI_TEXT);
    lv_obj_set_width(label, LV_PCT(100));
    return label;
}
static lv_obj_t *layout_footer(lv_obj_t *popup)
{
    lv_obj_t *footer = lv_obj_create(popup);
    lv_obj_remove_style_all(footer);
    lv_obj_set_size(footer, LV_PCT(100), 48);
    lv_obj_set_flex_flow(footer, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(footer, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(footer, UI_GAP_CARD, 0);
    return footer;
}
static const int32_t layout_columns[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
static const int32_t layout_rows[] = {LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
static lv_obj_t *layout_pair(lv_obj_t *parent)
{
    lv_obj_t *pair = lv_obj_create(parent);
    lv_obj_remove_style_all(pair);
    lv_obj_set_size(pair, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_grid_dsc_array(pair, layout_columns, layout_rows);
    lv_obj_set_style_pad_column(pair, UI_GAP_CARD, 0);
    return pair;
}
static void layout_cell(lv_obj_t *object, unsigned column)
{
    lv_obj_set_grid_cell(object, LV_GRID_ALIGN_STRETCH, column, 1, LV_GRID_ALIGN_START, 0, 1);
}
static void field_cb(lv_event_t *event)
{
    if (!owns_printer()) return;
    close_editor();
    s->editor_target = lv_event_get_target(event);
    s->editor = ui_popup_create(lv_layer_top(), popup_width(640), popup_height(420), UI_POPUP_STANDARD);
    if (!s->editor) { s->editor_target = NULL; return; }
    lv_obj_add_event_cb(s->editor, editor_deleted, LV_EVENT_DELETE, NULL);
    lv_obj_t *body = layout_body(s->editor, "EDIT MEASUREMENT");
    s->editor_value = ui_popup_add_textarea(body, 584, 56, LV_ALIGN_TOP_MID,
        0, 0, true, false, 18, "", lv_textarea_get_text(s->editor_target), "0123456789.");
    lv_obj_set_width(s->editor_value, LV_PCT(100));
    lv_obj_t *keyboard = ui_popup_add_keyboard(body, s->editor_value, 584, 182,
        LV_ALIGN_TOP_MID, 0, 0, LV_KEYBOARD_MODE_NUMBER);
    lv_obj_set_width(keyboard, LV_PCT(100));
    lv_obj_t *footer = layout_footer(s->editor);
    lv_obj_t *cancel = ui_popup_add_action_at(footer, UI_POPUP_ACTION_CANCEL, "CANCEL",
        0, 0, 150, 48, editor_cancel_cb, NULL, NULL);
    lv_obj_t *done = ui_popup_add_action_at(footer, UI_POPUP_ACTION_CONFIRM, "DONE",
        0, 0, 150, 48, editor_done_cb, NULL, NULL);
    lv_obj_set_flex_grow(cancel, 1);
    lv_obj_set_flex_grow(done, 1);
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
    s->popup = ui_popup_create(lv_layer_top(), popup_width(view == VIEW_LIMITS ? 720 : 780),
        popup_height(view == VIEW_LIMITS ? 440 : 500), UI_POPUP_STANDARD);
    if (!s->popup) return;
    lv_obj_add_event_cb(s->popup, popup_deleted, LV_EVENT_DELETE, NULL);
    lv_obj_t *body = layout_body(s->popup, view == VIEW_LIMITS ? "MOTION LIMITS" :
        view == VIEW_DRIVERS ? "TMC DRIVER DIAGNOSTICS" : "AXIS DISTANCE CALCULATOR");
    if (view == VIEW_LIMITS) {
        s->status = layout_label(body, "WAITING FOR LIVE LIMITS", true);
        const char *labels[] = {"Maximum velocity", "Maximum acceleration", "Square-corner velocity", "Minimum cruise ratio"};
        for (size_t i = 0; i < 4; ++i) {
            lv_obj_t *pair = layout_pair(body);
            lv_obj_t *name = layout_label(pair, labels[i], false);
            s->limits[i] = layout_label(pair, "Not reported", false);
            layout_cell(name, 0); layout_cell(s->limits[i], 1);
        }
        layout_label(body, "Live limits may differ from printer.cfg during a print.", false);
    } else if (view == VIEW_DRIVERS) {
        s->driver_selector = lv_dropdown_create(body);
        lv_obj_set_size(s->driver_selector, LV_PCT(100), 54);
        lv_obj_set_style_text_font(s->driver_selector, UI_FONT_BODY, 0);
        lv_dropdown_set_options(s->driver_selector, "Waiting for drivers...");
        lv_obj_add_event_cb(s->driver_selector, driver_selected_cb, LV_EVENT_VALUE_CHANGED, NULL);
        s->status = layout_label(body, "WAITING FOR DRIVER STATUS", true);
        s->body = layout_label(body, "Waiting for data...", false);
    } else {
        layout_label(body, "Calculate from belt/pulley or leadscrew geometry.\nRetain gear_ratio for geared axes.", false);
        s->mode = lv_dropdown_create(body);
        lv_obj_set_size(s->mode, LV_PCT(100), 54);
        lv_obj_set_style_text_font(s->mode, UI_FONT_BODY, 0);
        lv_dropdown_set_options(s->mode, "Belt and pulley\nLeadscrew");
        lv_obj_add_event_cb(s->mode, mode_cb, LV_EVENT_VALUE_CHANGED, NULL);
        lv_obj_t *pair = layout_pair(body);
        for (unsigned i = 0; i < 2; ++i) {
            lv_obj_t *cell = lv_obj_create(pair);
            lv_obj_remove_style_all(cell);
            lv_obj_set_height(cell, LV_SIZE_CONTENT);
            lv_obj_set_flex_flow(cell, LV_FLEX_FLOW_COLUMN);
            lv_obj_set_style_pad_row(cell, UI_GAP_ROW, 0);
            layout_cell(cell, i);
            lv_obj_t *label = layout_label(cell, i == 0 ? "BELT PITCH (mm)" : "PULLEY TEETH", true);
            lv_obj_t *field = ui_popup_add_textarea(cell, 354, 56, LV_ALIGN_TOP_LEFT,
                0, 0, true, false, 18, i == 0 ? "Pitch" : "Teeth or thread starts",
                i == 0 ? "2" : "20", i == 0 ? "0123456789." : "0123456789");
            lv_obj_set_width(field, LV_PCT(100));
            lv_obj_add_event_cb(field, field_cb, LV_EVENT_CLICKED, NULL);
            if (i == 0) { s->pitch_label = label; s->pitch = field; }
            else { s->count_label = label; s->count = field; }
        }
        s->result = layout_label(body, "Enter the hardware measurements, then Calculate.", false);
        s->reference = layout_label(body, "Config reference: waiting for printer data.", true);
        layout_label(body, "For screws, enter thread pitch and thread-start count.\nNo motion or configuration changes are sent.", true);
    }
    lv_obj_t *footer = layout_footer(s->popup);
    lv_obj_t *close = ui_popup_add_action_at(footer, UI_POPUP_ACTION_CLOSE, "CLOSE",
        0, 0, 170, 48, close_cb, NULL, NULL);
    if (view == VIEW_DISTANCE) {
        lv_obj_t *calculate = ui_popup_add_action_at(footer, UI_POPUP_ACTION_PRIMARY, "CALCULATE",
            0, 0, 180, 48, calculate_cb, NULL, NULL);
        lv_obj_set_flex_grow(close, 1); lv_obj_set_flex_grow(calculate, 1);
    }
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
