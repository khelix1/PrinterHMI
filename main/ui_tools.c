#include "ui_tools.h"

#include <stdint.h>

#include "lvgl.h"
#include "ui_theme.h"
#include "ui_page_geometry.h"
#include "ui_page_title.h"
#include "ui_button.h"
#include "ui_shell.h"

static lv_obj_t *s_root;
static ui_tools_open_cb_t s_callbacks[4];

static void tile_cb(lv_event_t *event)
{
    int index = (int)(intptr_t)lv_event_get_user_data(event);
    if (lv_event_get_code(event) == LV_EVENT_CLICKED &&
        index >= 0 && index < 4 && s_callbacks[index]) {
        s_callbacks[index]();
    }
}

static void add_tile(const char *icon, lv_color_t icon_color,
                     const char *title, const char *body,
                     int x, int y, int index)
{
    lv_obj_t *button = ui_button_create_empty(s_root, UI_BUTTON_OUTLINED);
    lv_obj_set_size(button, 380, 172);
    lv_obj_set_pos(button, x, y);
    lv_obj_clear_flag(button, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *icon_label = lv_label_create(button);
    lv_label_set_text(icon_label, icon);
    ui_apply_text_title(icon_label);
    lv_obj_set_style_text_color(icon_label, icon_color, 0);
    lv_obj_set_pos(icon_label, 26, 30);

    lv_obj_t *title_label = lv_label_create(button);
    lv_label_set_text(title_label, title);
    ui_apply_text_title(title_label);
    ui_apply_label_bright(title_label);
    lv_obj_set_pos(title_label, 92, 26);

    lv_obj_t *body_label = lv_label_create(button);
    lv_label_set_text(body_label, body);
    ui_apply_text_body(body_label);
    lv_obj_set_width(body_label, 254);
    lv_label_set_long_mode(body_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(body_label, 92, 68);

    lv_obj_add_event_cb(button, tile_cb, LV_EVENT_CLICKED,
                        (void *)(intptr_t)index);
}

void ui_tools_set_callbacks(ui_tools_open_cb_t calibration,
                            ui_tools_open_cb_t mesh,
                            ui_tools_open_cb_t devices,
                            ui_tools_open_cb_t macros)
{
    s_callbacks[0] = calibration;
    s_callbacks[1] = mesh;
    s_callbacks[2] = devices;
    s_callbacks[3] = macros;
}

void ui_tools_show(void)
{
    if (s_root) {
        lv_obj_clear_flag(s_root, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(s_root);
        return;
    }

    s_root = lv_obj_create(lv_screen_active());
    lv_obj_set_size(s_root, UI_PAGE_ROOT_WIDTH, UI_PAGE_ROOT_HEIGHT);
    lv_obj_set_pos(s_root, UI_PAGE_ROOT_X, UI_PAGE_ROOT_Y);
    lv_obj_clear_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);
    ui_apply_root_style(s_root);
    ui_page_title_create(s_root, LV_SYMBOL_SETTINGS " TOOLS",
                         "Calibration and operator utilities");

    add_tile(LV_SYMBOL_REFRESH, UI_OK_BRIGHT,
             "CALIBRATION", "Tune and validate your printer.", 20, 88, 0);
    add_tile(LV_SYMBOL_IMAGE, UI_ACCENT_BRIGHT,
             "BED MESH", "Probe and visualize the bed surface.", 420, 88, 1);
    add_tile(LV_SYMBOL_CHARGE, UI_WARN,
             "DEVICES", "Manage connected devices and diagnostics.", 20, 276, 2);
    add_tile(LV_SYMBOL_PLAY, UI_OK_BRIGHT,
             "MACROS", "Run available printer macros.", 420, 276, 3);
}

void ui_tools_hide(void)
{
    if (s_root) lv_obj_add_flag(s_root, LV_OBJ_FLAG_HIDDEN);
    ui_shell_raise_topbar();
    ui_shell_raise_nav();
}
