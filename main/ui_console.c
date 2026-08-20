#include "ui_console.h"
#include "ui_text.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include "console_controller.h"
#include "lvgl.h"
#include "moonraker.h"
#include "ui_button.h"
#include "ui_page_geometry.h"
#include "ui_popup.h"
#include "ui_theme.h"
#include "ui_toast.h"

static lv_obj_t *s_root = NULL;
static lv_obj_t *s_output = NULL;
static lv_obj_t *s_connection = NULL;
static lv_obj_t *s_follow_button = NULL;
static lv_obj_t *s_follow_label = NULL;
static lv_obj_t *s_command_popup = NULL;
static lv_obj_t *s_command_input = NULL;
static lv_timer_t *s_refresh_timer = NULL;
static ui_console_command_cb_t s_command_callback = NULL;
static uint32_t s_rendered_sequence = 0;
static size_t s_rendered_count = 0;
static size_t s_history_cursor = SIZE_MAX;
static bool s_follow = true;


static lv_color_t entry_color(console_entry_type_t type)
{
    switch (type) {
    case CONSOLE_ENTRY_COMMAND:
        return UI_ACCENT_CYAN;

    case CONSOLE_ENTRY_WARNING:
        return UI_WARN;

    case CONSOLE_ENTRY_ERROR:
        return UI_DANGER_BRIGHT;

    case CONSOLE_ENTRY_SYSTEM:
        return UI_TEXT_DIM;

    case CONSOLE_ENTRY_RESPONSE:
    default:
        return UI_TEXT_BRIGHT;
    }
}


static const char *entry_prefix(console_entry_type_t type)
{
    switch (type) {
    case CONSOLE_ENTRY_COMMAND:
        return ">";

    case CONSOLE_ENTRY_WARNING:
        return "!";

    case CONSOLE_ENTRY_ERROR:
        return "!!";

    case CONSOLE_ENTRY_SYSTEM:
        return "*";

    case CONSOLE_ENTRY_RESPONSE:
    default:
        return "<";
    }
}


static void format_entry_time(
    const console_entry_t *entry,
    char *output,
    size_t output_size)
{
    if (!entry || !output || output_size == 0) {
        return;
    }

    if (entry->timestamp >= 1700000000) {
        struct tm local_time = {0};
        localtime_r(
            &entry->timestamp,
            &local_time);
        strftime(
            output,
            output_size,
            "%H:%M:%S",
            &local_time);
        return;
    }

    uint32_t seconds = entry->uptime_seconds;
    snprintf(
        output,
        output_size,
        "+%02u:%02u:%02u",
        (unsigned)(seconds / 3600U),
        (unsigned)((seconds / 60U) % 60U),
        (unsigned)(seconds % 60U));
}


static void rebuild_output(void)
{
    if (!s_output) {
        return;
    }

    lv_obj_clean(s_output);

    size_t count = console_controller_count();
    s_rendered_count = count;
    s_rendered_sequence =
        console_controller_latest_sequence();

    if (count == 0) {
        lv_obj_t *empty = lv_label_create(s_output);
        lv_label_set_text(
            empty,
            ui_text("Console history is empty."));
        lv_obj_set_pos(empty, 18, 18);
        ui_apply_custom_label_style(
            empty,
            UI_FONT_BODY,
            UI_TEXT_DIM);
        return;
    }

    lv_obj_t *last = NULL;

    for (size_t row = 0; row < count; ++row) {
        size_t newest_index = count - 1 - row;
        console_entry_t entry;

        if (!console_controller_get(
                newest_index,
                &entry)) {
            continue;
        }

        char time_text[24] = "";
        char line[256];
        format_entry_time(
            &entry,
            time_text,
            sizeof(time_text));

        snprintf(
            line,
            sizeof(line),
            "%s  %-2s  %s",
            time_text,
            entry_prefix(entry.type),
            entry.message);

        lv_obj_t *label = lv_label_create(s_output);
        lv_label_set_text(label, line);
        lv_label_set_long_mode(
            label,
            LV_LABEL_LONG_DOT);
        lv_obj_set_size(label, 760, 34);
        lv_obj_set_pos(
            label,
            16,
            10 + (int32_t)row * 38);

        ui_apply_custom_label_style(
            label,
            UI_FONT_CAPTION,
            entry_color(entry.type));
        last = label;
    }

    lv_obj_update_layout(s_output);
    if (s_follow && last) {
        lv_obj_scroll_to_view(last, LV_ANIM_OFF);
    }
}


static void update_connection(void)
{
    if (!s_connection) {
        return;
    }

    moonraker_state_t state;
    moonraker_state_snapshot(&state);

    if (state.moonraker_ok) {
        lv_label_set_text(
            s_connection,
            ui_text("MOONRAKER LINKED"));
        ui_apply_custom_label_style(
            s_connection,
            UI_FONT_CAPTION,
            UI_OK_BRIGHT);
    } else {
        lv_label_set_text(
            s_connection,
            ui_text("MOONRAKER OFFLINE"));
        ui_apply_custom_label_style(
            s_connection,
            UI_FONT_CAPTION,
            UI_DANGER_BRIGHT);
    }
}


static void refresh_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    if (!s_root) {
        return;
    }

    size_t count = console_controller_count();
    uint32_t sequence =
        console_controller_latest_sequence();

    if (count != s_rendered_count ||
        sequence != s_rendered_sequence) {
        rebuild_output();
    }

    update_connection();
}


static void update_follow_button(void)
{
    if (s_follow_label) {
        lv_label_set_text(
            s_follow_label,
            s_follow
                ? ui_text("FOLLOW ON")
                : ui_text("FOLLOW OFF"));
    }

    if (s_follow_button) {
        ui_button_apply_kind(
            s_follow_button,
            s_follow
                ? UI_BUTTON_SUCCESS
                : UI_BUTTON_SECONDARY);
    }
}


static void follow_cb(lv_event_t *event)
{
    (void)event;
    s_follow = !s_follow;
    update_follow_button();
    rebuild_output();
}


static void clear_cb(lv_event_t *event)
{
    (void)event;
    console_controller_clear();
    rebuild_output();
}


static void close_command_popup(void)
{
    if (s_command_popup) {
        lv_obj_t *popup = s_command_popup;
        s_command_popup = NULL;
        s_command_input = NULL;
        lv_obj_delete(popup);
    }

    s_history_cursor = SIZE_MAX;
}


static void close_command_cb(lv_event_t *event)
{
    (void)event;
    close_command_popup();
}


static void history_prev_cb(lv_event_t *event)
{
    (void)event;

    size_t count =
        console_controller_history_count();

    if (!s_command_input || count == 0) {
        return;
    }

    if (s_history_cursor == SIZE_MAX) {
        s_history_cursor = 0;
    } else if (s_history_cursor + 1 < count) {
        ++s_history_cursor;
    }

    char command[CONSOLE_COMMAND_MAX + 1];
    if (console_controller_history_get(
            s_history_cursor,
            command,
            sizeof(command))) {
        lv_textarea_set_text(
            s_command_input,
            command);
        lv_textarea_set_cursor_pos(
            s_command_input,
            LV_TEXTAREA_CURSOR_LAST);
    }
}


static void history_next_cb(lv_event_t *event)
{
    (void)event;

    if (!s_command_input ||
        s_history_cursor == SIZE_MAX) {
        return;
    }

    if (s_history_cursor > 0) {
        --s_history_cursor;

        char command[CONSOLE_COMMAND_MAX + 1];
        if (console_controller_history_get(
                s_history_cursor,
                command,
                sizeof(command))) {
            lv_textarea_set_text(
                s_command_input,
                command);
        }
    } else {
        s_history_cursor = SIZE_MAX;
        lv_textarea_set_text(s_command_input, "");
    }

    lv_textarea_set_cursor_pos(
        s_command_input,
        LV_TEXTAREA_CURSOR_LAST);
}


static void send_command_cb(lv_event_t *event)
{
    (void)event;

    if (!s_command_input) {
        return;
    }

    const char *input =
        lv_textarea_get_text(s_command_input);

    while (input && (*input == ' ' || *input == '\t')) {
        ++input;
    }

    if (!input || !input[0]) {
        ui_toast_show(
            UI_STATUS_WARNING,
            "EMPTY COMMAND",
            "Enter a Klipper G-code command first.");
        return;
    }

    char command[CONSOLE_COMMAND_MAX + 1];
    snprintf(
        command,
        sizeof(command),
        "%.*s",
        CONSOLE_COMMAND_MAX,
        input);

    size_t length = strlen(command);
    while (length > 0 &&
           (command[length - 1] == ' ' ||
            command[length - 1] == '\t' ||
            command[length - 1] == '\r' ||
            command[length - 1] == '\n')) {
        command[--length] = '\0';
    }

    if (length == 0) {
        return;
    }

    console_controller_add_command(command);

    bool sent =
        s_command_callback &&
        s_command_callback(command);

    if (!sent) {
        console_controller_add(
            CONSOLE_ENTRY_ERROR,
            "Command was not accepted by Moonraker.");
    }

    close_command_popup();
    rebuild_output();
}


static void keyboard_event_cb(lv_event_t *event)
{
    lv_event_code_t code =
        lv_event_get_code(event);

    if (code == LV_EVENT_READY) {
        send_command_cb(event);
    } else if (code == LV_EVENT_CANCEL) {
        close_command_popup();
    }
}


static void open_command_cb(lv_event_t *event)
{
    (void)event;

    if (s_command_popup) {
        lv_obj_move_foreground(s_command_popup);
        return;
    }

    s_command_popup =
        ui_popup_create(
            lv_layer_top(),
            820,
            520,
            UI_POPUP_STANDARD);

    if (!s_command_popup) {
        return;
    }

    ui_popup_add_title(
        s_command_popup,
        ui_text("SEND KLIPPER COMMAND"),
        false,
        0);
    ui_popup_add_header_divider(
        s_command_popup,
        44);

    s_command_input =
        ui_popup_add_textarea(
            s_command_popup,
            772,
            54,
            LV_ALIGN_TOP_MID,
            0,
            58,
            true,
            false,
            CONSOLE_COMMAND_MAX,
            ui_text("G-code or macro, for example: STATUS"),
            ui_text(""),
            NULL);

    lv_obj_t *keyboard =
        ui_popup_add_keyboard(
            s_command_popup,
            s_command_input,
            772,
            292,
            LV_ALIGN_TOP_MID,
            0,
            122,
            LV_KEYBOARD_MODE_TEXT_LOWER);

    if (keyboard) {
        lv_obj_add_event_cb(
            keyboard,
            keyboard_event_cb,
            LV_EVENT_ALL,
            NULL);
    }

    ui_popup_add_standard_footer_divider(
        s_command_popup);

    ui_popup_add_action_at(
        s_command_popup,
        UI_POPUP_ACTION_CLOSE,
        ui_text(LV_SYMBOL_CLOSE " CLOSE"),
        24,
        452,
        140,
        44,
        close_command_cb,
        NULL,
        NULL);

    ui_popup_add_action_at(
        s_command_popup,
        UI_POPUP_ACTION_SECONDARY,
        ui_text(LV_SYMBOL_UP " PREV"),
        184,
        452,
        112,
        44,
        history_prev_cb,
        NULL,
        NULL);

    ui_popup_add_action_at(
        s_command_popup,
        UI_POPUP_ACTION_SECONDARY,
        ui_text(LV_SYMBOL_DOWN " NEXT"),
        306,
        452,
        112,
        44,
        history_next_cb,
        NULL,
        NULL);

    ui_popup_add_action_at(
        s_command_popup,
        UI_POPUP_ACTION_PRIMARY,
        ui_text(LV_SYMBOL_PLAY " SEND"),
        650,
        452,
        146,
        44,
        send_command_cb,
        NULL,
        NULL);

    if (s_command_input) {
        lv_obj_add_state(
            s_command_input,
            LV_STATE_FOCUSED);
    }
}


static lv_obj_t *page_button(
    const char *text,
    ui_button_kind_t kind,
    int32_t x,
    int32_t width,
    lv_event_cb_t callback,
    lv_obj_t **label_out)
{
    lv_obj_t *button =
        ui_button_create(
            s_root,
            kind,
            text);

    if (!button) {
        return NULL;
    }

    lv_obj_set_size(button, width, 46);
    lv_obj_set_pos(button, x, 16);
    lv_obj_add_event_cb(
        button,
        callback,
        LV_EVENT_CLICKED,
        NULL);

    if (label_out) {
        *label_out = lv_obj_get_child(button, 0);
    }

    return button;
}


void ui_console_show(
    ui_console_command_cb_t command_callback)
{
    s_command_callback = command_callback;

    if (s_root) {
        rebuild_output();
        update_connection();
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

    lv_obj_t *title = lv_label_create(s_root);
    lv_label_set_text(title, ui_text("CONSOLE"));
    lv_obj_set_pos(title, UI_PAGE_RAIL_X, 18);
    ui_apply_text_title(title);
    ui_apply_label_bright(title);

    lv_obj_t *subtitle = lv_label_create(s_root);
    lv_label_set_text(
        subtitle,
        ui_text("LIVE KLIPPER RESPONSES AND DIRECT G-CODE"));
    lv_obj_set_pos(subtitle, UI_PAGE_RAIL_X, 50);
    ui_apply_text_caption(subtitle);
    ui_apply_label_dim(subtitle);

    s_connection = lv_label_create(s_root);
    lv_obj_set_width(s_connection, 170);
    lv_obj_set_style_text_align(
        s_connection,
        LV_TEXT_ALIGN_RIGHT,
        0);
    lv_obj_set_pos(s_connection, 314, 31);

    page_button(
        "COMMAND",
        UI_BUTTON_PRIMARY,
        500,
        120,
        open_command_cb,
        NULL);

    s_follow_button =
        page_button(
            "FOLLOW ON",
            UI_BUTTON_SUCCESS,
            630,
            110,
            follow_cb,
            &s_follow_label);

    page_button(
        "CLEAR",
        UI_BUTTON_DANGER,
        750,
        84,
        clear_cb,
        NULL);

    s_output = lv_obj_create(s_root);
    lv_obj_set_size(s_output, 814, 426);
    lv_obj_set_pos(s_output, 20, 82);
    ui_apply_card_style(s_output);
    lv_obj_set_style_pad_all(s_output, 0, 0);
    lv_obj_set_scroll_dir(s_output, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(
        s_output,
        LV_SCROLLBAR_MODE_AUTO);

    update_follow_button();
    rebuild_output();
    update_connection();

    s_refresh_timer =
        lv_timer_create(
            refresh_timer_cb,
            250,
            NULL);
}


void ui_console_hide(void)
{
    close_command_popup();

    if (s_refresh_timer) {
        lv_timer_delete(s_refresh_timer);
        s_refresh_timer = NULL;
    }

    if (s_root) {
        lv_obj_delete(s_root);
    }

    s_root = NULL;
    s_output = NULL;
    s_connection = NULL;
    s_follow_button = NULL;
    s_follow_label = NULL;
    s_command_callback = NULL;
    s_rendered_sequence = 0;
    s_rendered_count = 0;
}
