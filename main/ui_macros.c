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
#include "ui_button.h"
#include "ui_page_geometry.h"
#include "ui_popup.h"
#include "ui_theme.h"
#include "ui_toast.h"

static const char TAG[] = "ui_macros";

typedef struct {
    lv_obj_t *root;
    lv_obj_t *list;
    lv_obj_t *status;
    lv_obj_t *confirm;
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


static void close_confirm(void)
{
    if (!s_macros) {
        return;
    }

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

    char command[MACRO_CONTROLLER_NAME_MAX];
    snprintf(
        command,
        sizeof(command),
        "%s",
        s_pending_macro);

    close_confirm();
    console_controller_add_command(command);

    bool sent =
        s_command_callback &&
        s_command_callback(command);

    if (!sent) {
        console_controller_add(
            CONSOLE_ENTRY_ERROR,
            "Macro %s was not accepted by Moonraker.",
            command);
        ui_toast_show(
            UI_STATUS_DANGER,
            "MACRO NOT SENT",
            command);
        return;
    }

    ui_toast_show(
        UI_STATUS_OK,
        "MACRO SENT",
        command);
}


static void rebuild_macro_list(void);

static void macro_favorite_cb(lv_event_t *event)
{
    uintptr_t encoded = (uintptr_t)lv_event_get_user_data(event);
    if (!encoded) return;
    char name[MACRO_CONTROLLER_NAME_MAX];
    if (!macro_controller_get((size_t)(encoded - 1), name, sizeof(name))) return;
    bool favorite = macro_controller_toggle_favorite(name);
    ui_toast_show(
        UI_STATUS_OK,
        favorite ? "FAVORITE SAVED" : "FAVORITE REMOVED",
        favorite ? "Long-press any macro to change Favorites."
                 : "The macro remains available in the full list.");
    rebuild_macro_list();
}

static void macro_button_cb(lv_event_t *event)
{
    uintptr_t encoded =
        (uintptr_t)lv_event_get_user_data(event);

    if (encoded == 0) {
        return;
    }

    size_t index = (size_t)(encoded - 1);
    if (!macro_controller_get(
            index,
            s_pending_macro,
            sizeof(s_pending_macro))) {
        s_pending_macro[0] = '\0';
        return;
    }

    if (s_confirm) {
        lv_obj_move_foreground(s_confirm);
        return;
    }

    s_confirm =
        ui_popup_create(
            lv_layer_top(),
            560,
            300,
            UI_POPUP_STANDARD);

    if (!s_confirm) {
        s_pending_macro[0] = '\0';
        return;
    }

    ui_popup_add_title(
        s_confirm,
        ui_text("RUN MACRO?"),
        false,
        4);
    ui_popup_add_header_divider(
        s_confirm,
        48);

    char body[256];
    snprintf(
        body,
        sizeof(body),
        "%s\n\nRun this detected Klipper macro?",
        s_pending_macro);

    ui_popup_add_body(
        s_confirm,
        body,
        28,
        76,
        504);

    ui_popup_add_standard_footer_divider(
        s_confirm);

    ui_popup_add_footer_action(
        s_confirm,
        UI_POPUP_ACTION_CANCEL,
        LV_SYMBOL_LEFT " BACK",
        170,
        UI_POPUP_FOOTER_LEFT,
        close_confirm_cb,
        NULL,
        NULL);

    ui_popup_add_footer_action(
        s_confirm,
        UI_POPUP_ACTION_PRIMARY,
        LV_SYMBOL_PLAY " RUN",
        170,
        UI_POPUP_FOOTER_RIGHT,
        run_macro_cb,
        NULL,
        NULL);
}


static void rebuild_macro_list(void)
{
    if (!s_list) {
        return;
    }

    lv_obj_clean(s_list);

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
        lv_obj_t *empty = lv_label_create(s_list);
        lv_label_set_text(
            empty,
            status.discovered
                ? ui_text("No public gcode_macro objects were detected.")
                : ui_text("Waiting for Moonraker object discovery..."));
        lv_obj_set_width(empty, 730);
        lv_obj_set_style_text_align(
            empty,
            LV_TEXT_ALIGN_CENTER,
            0);
        lv_obj_set_pos(empty, 28, 42);
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

        bool favorite = macro_controller_is_favorite(name);
        if ((pass == 0 && !favorite) || (pass == 1 && favorite)) continue;
        int32_t column = (int32_t)(displayed % 2);
        int32_t row = (int32_t)(displayed / 2);

        lv_obj_t *button =
            ui_button_create_icon(
                s_list,
                UI_BUTTON_OUTLINED,
                favorite ? LV_SYMBOL_OK : LV_SYMBOL_PLAY,
                name,
                UI_OK_BRIGHT,
                UI_BUTTON_ICON_HORIZONTAL);

        if (!button) {
            continue;
        }

        lv_obj_set_size(button, 382, 54);
        lv_obj_set_pos(
            button,
            12 + column * 394,
            12 + row * 62);

        lv_obj_add_event_cb(
            button,
            macro_button_cb,
            LV_EVENT_CLICKED,
            (void *)(uintptr_t)(index + 1));
        lv_obj_add_event_cb(button, macro_favorite_cb, LV_EVENT_LONG_PRESSED,
                            (void *)(uintptr_t)(index + 1));
        ++displayed;
    }
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

    lv_obj_t *title = lv_label_create(s_root);
    lv_label_set_text(title, ui_text("MACROS"));
    lv_obj_set_pos(title, UI_PAGE_RAIL_X, 18);
    ui_apply_text_title(title);
    ui_apply_label_bright(title);

    lv_obj_t *subtitle = lv_label_create(s_root);
    lv_label_set_text(
        subtitle,
        ui_text("DETECTED PUBLIC KLIPPER ACTIONS"));
    lv_obj_set_pos(subtitle, UI_PAGE_RAIL_X, 50);
    ui_apply_text_caption(subtitle);
    ui_apply_label_dim(subtitle);

    s_status = lv_label_create(s_root);
    lv_obj_set_width(s_status, 300);
    lv_obj_set_style_text_align(
        s_status,
        LV_TEXT_ALIGN_RIGHT,
        0);
    lv_obj_set_pos(s_status, 530, 30);
    ui_apply_custom_label_style(
        s_status,
        UI_FONT_CAPTION,
        UI_ACCENT_CYAN);

    s_list = lv_obj_create(s_root);
    lv_obj_set_size(s_list, 814, 426);
    lv_obj_set_pos(s_list, 20, 82);
    ui_apply_card_style(s_list);
    lv_obj_set_style_pad_all(s_list, 0, 0);
    lv_obj_set_scroll_dir(s_list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(
        s_list,
        LV_SCROLLBAR_MODE_AUTO);

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
    s_command_callback = NULL;
    s_rendered_generation = 0;
}
