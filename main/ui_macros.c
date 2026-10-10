#include "ui_macros.h"
#include "ui_text.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "console_controller.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "lvgl.h"
#include "macro_controller.h"
#include "moonraker.h"
#include "moonraker_config_controller.h"
#include "ui_button.h"
#include "ui_page_geometry.h"
#include "ui_popup.h"
#include "ui_theme.h"
#include "ui_toast.h"
#include "ui_responsive_layout.h"
#include "ui_text_fit.h"
#include "ui_value_update.h"

static const char TAG[] = "ui_macros";

typedef struct {
    lv_obj_t *root;
    lv_obj_t *list;
    lv_obj_t *status;
    lv_obj_t *confirm;
    lv_obj_t *search_label, *editor, *editor_value, *editor_target;
    lv_obj_t *parameter_fields[MACRO_PARAMETER_MAX + MACRO_PARAMETER_EXTRA];
    lv_obj_t *parameter_names[MACRO_PARAMETER_EXTRA];
    macro_parameter_catalog_t parameters;
    uint32_t pending_owner;
    bool editing_search;
    lv_obj_t *rows[MACRO_CONTROLLER_MAX_MACROS];
    lv_obj_t *empty;
    char query[48];
    char pending_command[MACRO_COMMAND_MAX];
    lv_timer_t *refresh_timer;
    ui_macros_command_cb_t command_callback;
    uint32_t rendered_generation;
    char pending_macro[MACRO_CONTROLLER_NAME_MAX];
} ui_macros_state_t;

/*
 * This context is allocated once after the scheduler starts and remains
 * allocated for the application lifetime. Only s_macros occupies startup
 * internal RAM.
 */
static ui_macros_state_t *s_macros = NULL;

#define s_root                (s_macros->root)
#define s_list                (s_macros->list)
#define s_status              (s_macros->status)
#define s_confirm             (s_macros->confirm)
#define s_refresh_timer       (s_macros->refresh_timer)
#define s_command_callback    (s_macros->command_callback)
#define s_rendered_generation (s_macros->rendered_generation)
#define s_pending_macro       (s_macros->pending_macro)

static void macro_feedback(ui_status_kind_t kind, const char *title, const char *detail)
{
    if (!s_macros) return;
    if (s_confirm) ui_popup_feedback(lv_obj_get_child(s_confirm, 1), kind, title, detail);
    else if (s_status) {
        char message[512];
        snprintf(message,sizeof(message),"%s: %s",title,detail ? detail : "");
        ui_value_set_text(s_status,message);
        ui_value_set_color(s_status,ui_status_color(kind),0);
    }
}


bool ui_macros_init(void)
{
    if (s_macros) {
        return true;
    }

    s_macros = heap_caps_calloc(
        1,
        sizeof(*s_macros),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (s_macros) {
        ESP_LOGI(
            TAG,
            "Macro page state allocated permanently in PSRAM: %u bytes",
            (unsigned)sizeof(*s_macros));
        return true;
    }

    s_macros = heap_caps_calloc(
        1,
        sizeof(*s_macros),
        MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

    if (!s_macros) {
        ESP_LOGE(TAG, "Unable to allocate macro page state");
        return false;
    }

    ESP_LOGW(TAG, "Macro page state using internal RAM fallback");
    return true;
}


static void rebuild_macro_list(void);
static void close_editor(void)
{
    if (s_macros->editor) lv_obj_delete(s_macros->editor);
    s_macros->editor = s_macros->editor_value = s_macros->editor_target = NULL;
}

static void close_confirm(void)
{
    if (!s_macros) {
        return;
    }

    close_editor();
    memset(s_macros->parameter_fields, 0, sizeof(s_macros->parameter_fields));
    memset(s_macros->parameter_names, 0, sizeof(s_macros->parameter_names));
    s_macros->pending_command[0] = 0;
    if (s_confirm) {
        lv_obj_t *popup = s_confirm;
        s_confirm = NULL;
        lv_obj_delete(popup);
    }

    s_pending_macro[0] = '\0';
}


static void close_confirm_cb(lv_event_t *event)
{
    (void)event;
    close_confirm();
}


static void run_macro_cb(lv_event_t *event)
{
    (void)event;

    if (!s_pending_macro[0]) {
        return;
    }

    moonraker_state_t state;
    moonraker_state_snapshot(&state);
    if (s_macros->pending_owner != moonraker_config_generation() ||
        !state.moonraker_ok || !state.live_data_ok ||
        !strcmp(state.printer_state, "error") || !strcmp(state.printer_state, "shutdown")) {
        macro_feedback(UI_STATUS_DANGER, "MACRO NOT SENT", "Printer changed or is not ready. Reopen the macro.");
        return;
    }
    char command[MACRO_COMMAND_MAX];
    snprintf(command, sizeof(command), "%s", s_macros->pending_command);
    if (!command[0]) return;

    console_controller_add_command(command);

    bool sent =
        s_command_callback &&
        s_command_callback(command);

    if (!sent) {
        console_controller_add(
            CONSOLE_ENTRY_ERROR,
            "Macro %s was not accepted by Moonraker.",
            command);
        macro_feedback(
            UI_STATUS_DANGER,
            "MACRO NOT SENT",
            command);
        return;
    }

    close_confirm();
    macro_feedback(UI_STATUS_OK, "MACRO SENT", command);
}


static void rebuild_macro_list(void);

static void macro_favorite_cb(lv_event_t *event)
{
    uintptr_t encoded = (uintptr_t)lv_obj_get_user_data(lv_event_get_target(event));
    macro_controller_status_t status;
    macro_controller_status(&status);
    if (!encoded || status.generation != s_rendered_generation) { rebuild_macro_list(); return; }
    char name[MACRO_CONTROLLER_NAME_MAX];
    if (!macro_controller_get((size_t)(encoded - 1), name, sizeof(name))) return;
    bool favorite = macro_controller_toggle_favorite(name);
    macro_feedback(
        UI_STATUS_OK,
        favorite ? "FAVORITE SAVED" : "FAVORITE REMOVED",
        favorite ? "Long-press any macro to change Favorites."
                 : "The macro remains available in the full list.");
    rebuild_macro_list();
}

static void editor_done_cb(lv_event_t *event)
{
    (void)event;
    if (!s_macros->editor_value) return;
    if (s_macros->editing_search) {
        snprintf(s_macros->query, sizeof(s_macros->query), "%s", lv_textarea_get_text(s_macros->editor_value));
        close_editor();
        if (s_macros->search_label)
            lv_label_set_text(s_macros->search_label, s_macros->query[0] ? "SEARCH*" : "SEARCH");
        rebuild_macro_list();
    } else {
        if (s_macros->editor_target)
            lv_textarea_set_text(s_macros->editor_target, lv_textarea_get_text(s_macros->editor_value));
        close_editor();
    }
}
static void editor_cancel_cb(lv_event_t *event) { (void)event; close_editor(); }

/* Dialog owners use native columns: only the body scrolls, actions stay visible. */
static lv_obj_t *macro_dialog(const char *text, lv_obj_t **body)
{
    int32_t width = lv_display_get_horizontal_resolution(NULL) - 32;
    int32_t height = lv_display_get_vertical_resolution(NULL) - 32;
    if (width > 800) width = 800;
    if (height > 500) height = 500;
    lv_obj_t *popup = ui_popup_create(lv_layer_top(), width, height, UI_POPUP_STANDARD);
    if (!popup) return NULL;
    lv_obj_set_flex_flow(popup, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(popup, 16, 0);
    lv_obj_set_style_pad_row(popup, UI_GAP_ROW, 0);
    lv_obj_t *title = lv_label_create(popup);
    lv_label_set_text(title, text);
    ui_apply_custom_label_style(title, UI_FONT_TITLE, UI_TEXT);
    lv_obj_set_width(title, LV_PCT(100));
    lv_obj_set_height(title, UI_FONT_TITLE->line_height);
    lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
    *body = lv_obj_create(popup);
    lv_obj_remove_style_all(*body);
    lv_obj_set_size(*body, LV_PCT(100), 0);
    lv_obj_set_flex_grow(*body, 1);
    lv_obj_set_flex_flow(*body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(*body, UI_GAP_ROW, 0);
    lv_obj_set_scroll_dir(*body, LV_DIR_VER);
    return popup;
}

static void macro_actions(lv_obj_t *popup, const char *text,
                          lv_event_cb_t cancel_cb, lv_event_cb_t accept_cb)
{
    lv_obj_t *row = lv_obj_create(popup);
    ui_responsive_column(row);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row, UI_GAP_ROW, 0);
    lv_obj_t *cancel = ui_popup_add_action_at(row, UI_POPUP_ACTION_CANCEL,
        "CANCEL", 0, 0, LV_PCT(48), 48, cancel_cb, NULL, NULL);
    lv_obj_set_flex_grow(cancel, 1);
    lv_obj_t *accept = ui_popup_add_action_at(row, UI_POPUP_ACTION_PRIMARY,
        text, 0, 0, LV_PCT(48), 48, accept_cb, NULL, NULL);
    lv_obj_set_flex_grow(accept, 1);
}

static lv_obj_t *macro_body_text(lv_obj_t *body, const char *text)
{
    lv_obj_t *label = lv_label_create(body);
    lv_label_set_text(label, text);
    ui_apply_custom_label_style(label, UI_FONT_BODY, UI_TEXT_DIM);
    lv_obj_set_width(label, LV_PCT(100));
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    return label;
}

static void open_editor(lv_obj_t *target, bool search)
{
    close_editor();
    s_macros->editing_search = search;
    s_macros->editor_target = target;
    lv_obj_t *body;
    s_macros->editor = macro_dialog(search ? "SEARCH MACROS" : "EDIT PARAMETER", &body);
    if (!s_macros->editor) return;
    uint32_t maximum = search ? sizeof(s_macros->query) - 1 : lv_textarea_get_max_length(target);
    s_macros->editor_value = ui_popup_add_textarea(body, LV_PCT(100), 56,
        LV_ALIGN_TOP_LEFT, 0, 0, true, false, maximum, "",
        search ? s_macros->query : lv_textarea_get_text(target), NULL);
    ui_apply_custom_label_style(s_macros->editor_value, UI_FONT_BODY, UI_TEXT);
    lv_obj_set_height(s_macros->editor_value, 56);
    ui_popup_add_keyboard(body, s_macros->editor_value, LV_PCT(100), 224,
        LV_ALIGN_TOP_LEFT, 0, 0, LV_KEYBOARD_MODE_TEXT_LOWER);
    macro_actions(s_macros->editor, "DONE", editor_cancel_cb, editor_done_cb);
}
static void field_cb(lv_event_t *event) { open_editor(lv_event_get_target(event), false); }
static void search_cb(lv_event_t *event) { (void)event; open_editor(NULL, true); }
static void clear_search_cb(lv_event_t *event)
{
    (void)event;
    s_macros->query[0] = 0;
    if (s_macros->search_label) lv_label_set_text(s_macros->search_label, "SEARCH");
    rebuild_macro_list();
}

static void review_macro_cb(lv_event_t *event)
{
    (void)event;
    macro_parameter_value_t values[MACRO_PARAMETER_MAX + MACRO_PARAMETER_EXTRA];
    size_t count = s_macros->parameters.count;
    for (size_t i = 0; i < count; ++i) {
        values[i].name = s_macros->parameters.names[i];
        values[i].value = lv_textarea_get_text(s_macros->parameter_fields[i]);
    }
    for (size_t i = 0; i < MACRO_PARAMETER_EXTRA; ++i) {
        values[count + i].name = lv_textarea_get_text(s_macros->parameter_names[i]);
        values[count + i].value = lv_textarea_get_text(s_macros->parameter_fields[count + i]);
    }
    const char *error;
    if (!macro_parameter_build_command(s_pending_macro, values, count + MACRO_PARAMETER_EXTRA,
            s_macros->pending_command, sizeof(s_macros->pending_command), &error)) {
        macro_feedback(UI_STATUS_DANGER, "CHECK PARAMETERS", error);
        return;
    }
    close_editor();
    lv_obj_t *old = s_confirm;
    s_confirm = NULL;
    lv_obj_delete(old);
    memset(s_macros->parameter_fields, 0, sizeof(s_macros->parameter_fields));
    memset(s_macros->parameter_names, 0, sizeof(s_macros->parameter_names));
    lv_obj_t *body;
    s_confirm = macro_dialog("RUN MACRO?", &body);
    if (!s_confirm) { s_pending_macro[0] = 0; return; }
    macro_body_text(body, "Review the command before running:");
    macro_body_text(body, s_macros->pending_command);
    macro_actions(s_confirm, LV_SYMBOL_PLAY " RUN", close_confirm_cb, run_macro_cb);
}

static void macro_button_cb(lv_event_t *event)
{
    if (s_confirm) { lv_obj_move_foreground(s_confirm); return; }
    uintptr_t encoded = (uintptr_t)lv_obj_get_user_data(lv_event_get_target(event));
    macro_controller_status_t status;
    macro_controller_status(&status);
    if (!encoded || status.generation != s_rendered_generation) { rebuild_macro_list(); return; }
    if (!macro_controller_get((size_t)(encoded - 1), s_pending_macro, sizeof(s_pending_macro))) return;
    s_macros->pending_owner = moonraker_config_generation();
    macro_controller_parameters(s_pending_macro, &s_macros->parameters);
    lv_obj_t *body;
    s_confirm = macro_dialog(s_pending_macro, &body);
    if (!s_confirm) { s_pending_macro[0] = 0; return; }
    macro_body_text(body,
        s_macros->parameters.truncated
            ? "Some fields omitted. Add other names below or use Console. Blank values are not sent."
            : "Tap a value to edit. Blank values are not sent. Additional named parameters can be added below.");
    lv_obj_t *list = lv_obj_create(body);
    ui_responsive_column(list);
    size_t count = s_macros->parameters.count;
    for (size_t i = 0; i < count + MACRO_PARAMETER_EXTRA; ++i) {
        lv_obj_t *cell = lv_obj_create(list);
        ui_responsive_column(cell);
        if (!cell) { close_confirm(); return; }
        if (i < count) {
            lv_obj_t *label = macro_body_text(cell, s_macros->parameters.names[i]);
            lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
        } else {
            lv_obj_t *name = ui_popup_add_textarea(cell, LV_PCT(100), 48, LV_ALIGN_TOP_LEFT,
                0, 0, true, false, MACRO_PARAMETER_NAME_MAX - 1, "Additional NAME", "", NULL);
            ui_apply_custom_label_style(name, UI_FONT_BODY, UI_TEXT);
            lv_obj_set_height(name, 56);
            s_macros->parameter_names[i - count] = name;
            lv_obj_add_event_cb(name, field_cb, LV_EVENT_CLICKED, NULL);
        }
        lv_obj_t *value = ui_popup_add_textarea(cell, LV_PCT(100), 56, LV_ALIGN_TOP_LEFT,
            0, 0, true, false, MACRO_PARAMETER_VALUE_MAX - 1, "Value (optional)", "", NULL);
        ui_apply_custom_label_style(value, UI_FONT_BODY, UI_TEXT);
        lv_obj_set_height(value, 56);
        s_macros->parameter_fields[i] = value;
        lv_obj_add_event_cb(value, field_cb, LV_EVENT_CLICKED, NULL);
    }
    macro_actions(s_confirm, "REVIEW", close_confirm_cb, review_macro_cb);
}


static void rebuild_macro_list(void)
{
    if (!s_list) {
        return;
    }

    for (size_t i = 0; i < MACRO_CONTROLLER_MAX_MACROS; ++i)
        if (s_macros->rows[i]) lv_obj_add_flag(s_macros->rows[i], LV_OBJ_FLAG_HIDDEN);
    if (s_macros->empty) lv_obj_add_flag(s_macros->empty, LV_OBJ_FLAG_HIDDEN);

    macro_controller_status_t status;
    macro_controller_status(&status);
    s_rendered_generation = status.generation;

    if (s_status) {
        char status_text[96];

        if (!status.discovered) {
            snprintf(
                status_text,
                sizeof(status_text),
                "WAITING FOR PRINTER DISCOVERY");
        } else if (status.truncated) {
            snprintf(
                status_text,
                sizeof(status_text),
                "%u OF %u PUBLIC MACROS",
                (unsigned)status.count,
                (unsigned)status.total_count);
        } else {
            snprintf(
                status_text,
                sizeof(status_text),
                "%u PUBLIC MACRO%s",
                (unsigned)status.count,
                status.count == 1 ? "" : "S");
        }

        lv_label_set_text(
            s_status,
            status_text);
    }

    if (!status.discovered || status.count == 0) {
        if (!s_macros->empty) s_macros->empty = lv_label_create(s_list);
        lv_obj_t *empty = s_macros->empty;
        lv_obj_remove_flag(empty, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(
            empty,
            status.discovered
                ? ui_text("No public gcode_macro objects were detected.")
                : ui_text("Waiting for Moonraker object discovery..."));
        lv_obj_set_width(empty, lv_pct(100));
        lv_obj_set_style_text_align(
            empty,
            LV_TEXT_ALIGN_CENTER,
            0);
        lv_obj_add_flag(empty, LV_OBJ_FLAG_FLOATING);
        lv_obj_align(empty, LV_ALIGN_TOP_MID, 0, 24);
        ui_apply_custom_label_style(
            empty,
            UI_FONT_BODY_LARGE,
            UI_TEXT_DIM);
        return;
    }

    size_t displayed = 0;
    for (unsigned pass = 0; pass < 2; ++pass) {
    for (size_t index = 0;
         index < status.count;
         ++index) {
        char name[MACRO_CONTROLLER_NAME_MAX];

        if (!macro_controller_get(
                index,
                name,
                sizeof(name))) {
            continue;
        }

        if (!macro_parameter_matches(name, s_macros->query)) continue;
        bool favorite = macro_controller_is_favorite(name);
        if ((pass == 0 && !favorite) || (pass == 1 && favorite)) continue;

        lv_obj_t *button = s_macros->rows[displayed];
        if (!button) {
            button = ui_button_create_icon(s_list, UI_BUTTON_OUTLINED,
                favorite ? LV_SYMBOL_OK : LV_SYMBOL_PLAY, name, UI_OK_BRIGHT, UI_BUTTON_ICON_HORIZONTAL);
            if (!button) continue;
            s_macros->rows[displayed] = button;
            lv_obj_add_event_cb(button, macro_button_cb, LV_EVENT_CLICKED, NULL);
            lv_obj_add_event_cb(button, macro_favorite_cb, LV_EVENT_LONG_PRESSED, NULL);
        }
        lv_obj_remove_flag(button, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_user_data(button, (void *)(uintptr_t)(index + 1));
        lv_obj_t *icon = lv_obj_get_child(button, 0);
        lv_obj_t *label = lv_obj_get_child(button, 1);
        const char *symbol = favorite ? LV_SYMBOL_OK : LV_SYMBOL_PLAY;
        if (strcmp(lv_label_get_text(icon), symbol)) lv_label_set_text(icon, symbol);
        if (strcmp(lv_label_get_text(label), name)) lv_label_set_text(label, name);
        lv_obj_set_height(button, LV_SIZE_CONTENT);
        lv_obj_set_style_min_height(button, 56, 0);
        lv_obj_set_style_pad_ver(button, 12, 0);
        lv_obj_set_flex_grow(label, 1);
        lv_obj_set_width(label, 0);
        lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
        ++displayed;
    }
    }
    lv_obj_send_event(s_list, LV_EVENT_SIZE_CHANGED, NULL);
    if (s_macros->query[0]) {
        char text[96];
        snprintf(text, sizeof(text), "%u MATCHES: %.47s", (unsigned)displayed, s_macros->query);
        lv_label_set_text(s_status, text);
    }
    if (!displayed) {
        if (!s_macros->empty) s_macros->empty = ui_popup_add_body(s_list, "", 0, 24, lv_pct(100));
        lv_obj_add_flag(s_macros->empty, LV_OBJ_FLAG_FLOATING);
        lv_obj_remove_flag(s_macros->empty, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(s_macros->empty, "No matching macros. Clear or change the search.");
    }
}


static void refresh_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    if (!s_macros || !s_root) {
        return;
    }

    macro_controller_status_t status;
    macro_controller_status(&status);

    if (s_confirm) {
        moonraker_state_t state;
        moonraker_state_snapshot(&state);
        if (s_macros->pending_owner != moonraker_config_generation() ||
            !state.moonraker_ok || !state.live_data_ok) {
            macro_feedback(UI_STATUS_WARNING, "MACRO NOT READY", "Printer changed or disconnected. Cancel and reopen when ready.");
        }
    }
    if (status.generation !=
        s_rendered_generation) {
        rebuild_macro_list();
    }
}


void ui_macros_show(
    ui_macros_command_cb_t command_callback)
{
    if (!ui_macros_init()) {
        return;
    }

    s_command_callback = command_callback;

    if (s_root) {
        rebuild_macro_list();
        lv_obj_move_foreground(s_root);
        return;
    }

    s_root = lv_obj_create(lv_screen_active());
    lv_obj_set_size(
        s_root,
        UI_PAGE_ROOT_WIDTH,
        UI_PAGE_ROOT_HEIGHT);
    lv_obj_set_pos(
        s_root,
        UI_PAGE_ROOT_X,
        UI_PAGE_ROOT_Y);
    lv_obj_clear_flag(
        s_root,
        LV_OBJ_FLAG_SCROLLABLE);
    ui_apply_root_style(s_root);

    lv_obj_set_flex_flow(s_root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(s_root, ui_theme_is_studio()?0:20, 0);
    lv_obj_set_style_pad_row(s_root, UI_GAP_ROW, 0);
    lv_obj_t *header = lv_obj_create(s_root);
    ui_responsive_column(header);
    lv_obj_t *title = lv_label_create(header);
    lv_label_set_text(title, ui_text("MACROS"));
    ui_apply_text_title(title);
    ui_apply_label_bright(title);
    lv_obj_t *subtitle = lv_label_create(header);
    lv_label_set_text(subtitle, ui_text("DETECTED PUBLIC KLIPPER ACTIONS"));
    lv_obj_set_width(subtitle, lv_pct(100));
    ui_apply_text_caption(subtitle);
    ui_apply_label_dim(subtitle);
    s_status = lv_label_create(header);
    lv_obj_set_width(s_status, lv_pct(100));
    lv_label_set_long_mode(s_status, LV_LABEL_LONG_WRAP);
    ui_apply_custom_label_style(s_status, UI_FONT_CAPTION, UI_ACCENT_CYAN);

    lv_obj_t *actions = lv_obj_create(s_root);
    ui_responsive_column(actions);
    lv_obj_set_flex_flow(actions, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_column(actions, UI_GAP_ROW, 0);
    lv_obj_t *search = ui_button_create(actions, UI_BUTTON_OUTLINED, "SEARCH");
    ui_responsive_action(search);
    s_macros->search_label = lv_obj_get_child(search, 0);
    lv_label_set_text(s_macros->search_label, s_macros->query[0] ? "SEARCH*" : "SEARCH");
    lv_obj_add_event_cb(search, search_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *clear = ui_button_create(actions, UI_BUTTON_OUTLINED, "CLEAR SEARCH");
    ui_responsive_action(clear);
    lv_label_set_long_mode(lv_obj_get_child(search, 0), LV_LABEL_LONG_CLIP);
    lv_label_set_long_mode(lv_obj_get_child(clear, 0), LV_LABEL_LONG_CLIP);
    lv_obj_add_event_cb(clear, clear_search_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *hint = lv_label_create(header);
    lv_label_set_text(hint, "Long-press a macro to change Favorites.");
    lv_obj_set_width(hint, lv_pct(100));
    ui_apply_text_caption(hint);
    ui_apply_label_dim(hint);

    s_list = lv_obj_create(s_root);
    lv_obj_set_size(s_list, lv_pct(100), 0);
    lv_obj_set_flex_grow(s_list, 1);
    ui_apply_card_style(s_list);
    lv_obj_set_style_pad_all(s_list, ui_theme_is_studio()?0:10, 0);
    lv_obj_set_scroll_dir(s_list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_list, LV_SCROLLBAR_MODE_AUTO);
    ui_responsive_cards(s_list, 320);
    if(ui_theme_is_studio()) {
        lv_obj_set_style_pad_row(s_root,4,0);
        lv_obj_set_layout(header,LV_LAYOUT_NONE);lv_obj_set_size(header,lv_pct(100),76);
        lv_obj_set_pos(title,0,0);lv_obj_set_width(title,200);ui_apply_custom_label_style(title,&ui_studio_font_32,UI_TEXT);
        lv_obj_add_flag(subtitle,LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_pos(s_status,220,10);lv_obj_set_size(s_status,380,26);lv_label_set_long_mode(s_status,LV_LABEL_LONG_DOT);
        lv_obj_set_pos(hint,0,50);lv_obj_set_width(hint,976);
        lv_obj_set_parent(search,header);lv_obj_set_parent(clear,header);
        lv_obj_set_flex_grow(search,0);lv_obj_set_flex_grow(clear,0);
        lv_obj_set_style_min_width(search,0,0);lv_obj_set_style_min_width(clear,0,0);
        lv_obj_set_size(search,158,44);lv_obj_set_pos(search,626,0);
        lv_obj_set_size(clear,180,44);lv_obj_set_pos(clear,796,0);
        lv_obj_set_style_pad_ver(search,3,0);lv_obj_set_style_pad_ver(clear,3,0);
        lv_obj_delete(actions);
        ui_text_fit_single_line(lv_obj_get_child(search,0),UI_FONT_BODY);
        ui_text_fit_single_line(lv_obj_get_child(clear,0),UI_FONT_BODY);
    }
    lv_obj_update_layout(s_root);

    rebuild_macro_list();

    s_refresh_timer =
        lv_timer_create(
            refresh_timer_cb,
            500,
            NULL);
}


void ui_macros_hide(void)
{
    if (!s_macros) {
        return;
    }

    close_confirm();

    if (s_refresh_timer) {
        lv_timer_delete(s_refresh_timer);
        s_refresh_timer = NULL;
    }

    if (s_root) {
        lv_obj_delete(s_root);
    }

    s_root = NULL;
    s_list = NULL;
    s_status = NULL;
    memset(s_macros->rows, 0, sizeof(s_macros->rows));
    s_macros->empty = NULL;
    s_macros->search_label = NULL;
    s_command_callback = NULL;
    s_rendered_generation = 0;
}
