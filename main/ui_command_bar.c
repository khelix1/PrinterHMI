#include "ui_command_bar.h"
#include "ui_button.h"
#include "ui_theme.h"
#include "ui_widgets.h"
#include <string.h>

static lv_obj_t *s_command_bar = NULL;
static lv_obj_t *s_pause_button = NULL;
static lv_obj_t *s_resume_button = NULL;
static lv_obj_t *s_object_button = NULL;
static lv_obj_t *s_cancel_button = NULL;

static void cmd_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    const char *action = (const char *)lv_event_get_user_data(e);
    if (!action) return;

    ui_command_bar_v32_action(action);
}

/*
 * TEST13_OPERATOR_COMMAND_BAR
 *
 * Dashboard-specific command layout using the shared Operator visual
 * language established by Drybox:
 *
 *   - dark control surface
 *   - semantic colored outline
 *   - bright text
 *   - restrained pressed state
 *
 * Button geometry and command behavior remain Dashboard-owned.
 */
static lv_obj_t *make_cmd(
    lv_obj_t *parent,
    const char *symbol,
    const char *text,
    const char *action,
    int x,
    int w,
    lv_color_t icon_color)
{
    /*
     * Dashboard and Drybox now create the exact same shared button.
     *
     * UI_BUTTON_OUTLINED owns the complete frame. Dashboard changes
     * only geometry and the horizontal icon/text arrangement.
     */
    lv_obj_t *btn =
        ui_button_create_icon(
            parent,
            UI_BUTTON_OUTLINED,
            symbol,
            text,
            icon_color,
            UI_BUTTON_ICON_HORIZONTAL);

    if (!btn) {
        return NULL;
    }

    lv_obj_set_size(
        btn,
        w,
        44);

    lv_obj_set_pos(
        btn,
        x,
        0);

    lv_obj_add_event_cb(
        btn,
        cmd_event_cb,
        LV_EVENT_CLICKED,
        (void *)action);

    return btn;
}


static void command_bar_deleted_cb(lv_event_t *event)
{
    if (event && lv_event_get_target(event) == s_command_bar) {
        s_command_bar = NULL;
        s_pause_button = NULL;
        s_resume_button = NULL;
        s_object_button = NULL;
        s_cancel_button = NULL;
    }
}


static void set_button_enabled(lv_obj_t *button, bool enabled)
{
    if (!button) return;

    if (enabled) {
        lv_obj_clear_state(button, LV_STATE_DISABLED);
    } else {
        lv_obj_add_state(button, LV_STATE_DISABLED);
    }
}


void ui_command_bar_v32_update(
    const char *printer_state,
    bool cancel_object_available)
{
    bool printing = printer_state &&
        strcmp(printer_state, "printing") == 0;
    bool paused = printer_state &&
        strcmp(printer_state, "paused") == 0;
    bool active = printing || paused;

    if (s_pause_button && s_resume_button) {
        if (paused) {
            lv_obj_add_flag(s_pause_button, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(s_resume_button, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(s_pause_button, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(s_resume_button, LV_OBJ_FLAG_HIDDEN);
        }
    }

    set_button_enabled(s_pause_button, printing);
    set_button_enabled(s_resume_button, paused);
    set_button_enabled(
        s_object_button,
        active && cancel_object_available);
    set_button_enabled(s_cancel_button, active);
}

lv_obj_t *ui_command_bar_v32_create(lv_obj_t *parent, int x, int y, int w, int h)
{
    /*
     * TEST3_SHARED_OPERATOR_PANEL
     *
     * The shared widget owns the Operator panel surface, border,
     * and radius. Command-bar dimensions and internal padding
     * remain local to preserve the existing Dashboard layout.
     */
    lv_obj_t *bar = ui_create_panel(parent);
    if (!bar) return NULL;

    s_command_bar = bar;
    lv_obj_set_size(bar, w, h);
    lv_obj_set_pos(bar, x, y);
    lv_obj_set_style_pad_all(
        bar,
        UI_PAD_CARD,
        0);

    lv_obj_add_event_cb(
        bar,
        command_bar_deleted_cb,
        LV_EVENT_DELETE,
        NULL);

    /*
     * Three wide, print-specific controls replace the old six-button
     * navigation/command mix. PAUSE and RESUME occupy the same slot and
     * swap visibility as printer state changes.
     */
    s_pause_button = make_cmd(
        bar,
        LV_SYMBOL_PAUSE,
        "PAUSE",
        "PAUSE",
        13,
        244,
        UI_ACCENT_CYAN);

    s_resume_button = make_cmd(
        bar,
        LV_SYMBOL_PLAY,
        "RESUME",
        "RESUME",
        13,
        244,
        UI_OK_BRIGHT);

    s_object_button = make_cmd(
        bar,
        LV_SYMBOL_LIST,
        "CANCEL OBJECT",
        "CANCEL_OBJECT",
        269,
        244,
        UI_ACCENT_INFO);

    s_cancel_button = make_cmd(
        bar,
        LV_SYMBOL_STOP,
        "CANCEL PRINT",
        "CANCEL_PRINT",
        525,
        244,
        UI_DANGER_BRIGHT);

    ui_command_bar_v32_update(NULL, false);

    return bar;
}
