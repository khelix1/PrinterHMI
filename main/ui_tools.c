#include "ui_tools.h"
#include "printer_connection_policy.h"
#include "ui_value_update.h"

#include <stdint.h>

#include "lvgl.h"
#include "ui_theme.h"
#include "ui_page_geometry.h"
#include "ui_page_title.h"
#include "ui_button.h"
#include "ui_widgets.h"
#include "ui_shell.h"
#include "ui_responsive_layout.h"
#include "ui_studio_layout.h"

static lv_obj_t *s_root;
static lv_obj_t *s_tiles;
static lv_obj_t *s_center_rule;
static lv_obj_t *s_body_labels[4];
static lv_timer_t *s_connection_timer;
static const char *const s_descriptions[]={"Tune and validate your printer.","Probe and visualize the bed surface.","Inspect devices and live readings.","Run available printer macros."};
static void tools_connection_refresh(lv_timer_t *timer)
{
    (void)timer;if(!s_root)return;
    moonraker_state_t state;moonraker_state_snapshot(&state);
    for(unsigned i=0;i<4;i++) {
        const char *reason=i<2?printer_idle_reason(&state):printer_connection_reason(&state);
        ui_value_set_text(s_body_labels[i],reason[0]?reason:s_descriptions[i]);
    }
}
static ui_tools_open_cb_t s_callbacks[4];

static void tools_deleted(lv_event_t *event){
    if(lv_event_get_target_obj(event)!=s_root)return;
    if(s_connection_timer)lv_timer_delete(s_connection_timer);
    s_connection_timer=NULL;s_root=NULL;s_tiles=NULL;s_center_rule=NULL;
    for(unsigned i=0;i<4;i++)s_body_labels[i]=NULL;
}

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
                     int index)
{
    lv_obj_t *button = ui_button_create_empty(s_tiles, UI_BUTTON_OUTLINED);
    static const int32_t columns[] = {40, LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static const int32_t rows[] = {LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
    lv_obj_set_size(button, lv_pct(48), LV_SIZE_CONTENT);
    lv_obj_set_style_min_height(button, 156, 0);
    lv_obj_set_style_pad_all(button, UI_PAD_POPUP, 0);
    lv_obj_set_style_pad_column(button, UI_GAP_CARD, 0);
    lv_obj_set_style_pad_row(button, UI_GAP_ROW, 0);
    /* Flat entry sections retain the familiar two-by-two arrangement. */
    ui_apply_flat_entry(button);
    lv_obj_set_grid_dsc_array(button, columns, rows);
    lv_obj_clear_flag(button, LV_OBJ_FLAG_SCROLLABLE);
    if (ui_theme_is_studio()) {
        lv_obj_set_size(button, 480, 168);
        lv_obj_set_pos(button, (index % 2) * 496, (index / 2) * 184);
        lv_obj_set_style_min_height(button, 168, 0);
        lv_obj_set_style_pad_all(button, 16, 0);
        lv_obj_set_style_pad_column(button, 16, 0);
        lv_obj_set_style_pad_row(button, 8, 0);
    }

    lv_obj_t *icon_label = lv_label_create(button);
    lv_label_set_text(icon_label, icon);
    ui_apply_text_title(icon_label);
    lv_obj_set_style_text_font(icon_label, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(icon_label, icon_color, 0);
    lv_obj_set_grid_cell(icon_label, LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_START, 0, 2);

    lv_obj_t *title_label = lv_label_create(button);
    lv_label_set_text(title_label, title);
    ui_apply_text_title(title_label);
    ui_apply_label_bright(title_label);
    lv_label_set_long_mode(title_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_grid_cell(title_label, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_START, 0, 1);

    lv_obj_t *body_label = lv_label_create(button);
    s_body_labels[index]=body_label;
    lv_label_set_text(body_label, body);
    ui_apply_text_body(body_label);

    lv_label_set_long_mode(body_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_grid_cell(body_label, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_START, 1, 1);

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

static void tools_rule_update(lv_event_t *event)
{
    (void)event;
    if(!s_root || !s_tiles || !s_center_rule || lv_obj_get_child_count(s_tiles)<2)return;
    lv_area_t left,right,viewport,root;
    lv_obj_get_coords(lv_obj_get_child(s_tiles,0),&left);
    lv_obj_get_coords(lv_obj_get_child(s_tiles,1),&right);
    lv_obj_get_content_coords(s_tiles,&viewport);
    lv_obj_get_content_coords(s_root,&root);
    if(left.y1!=right.y1 || left.x2>=right.x1 || viewport.y2-viewport.y1<16){
        lv_obj_add_flag(s_center_rule,LV_OBJ_FLAG_HIDDEN);return;
    }
    int32_t x=(left.x2+right.x1)/2-root.x1;
    int32_t y=viewport.y1+8-root.y1;
    int32_t height=viewport.y2-viewport.y1+1-16;
    if(lv_obj_get_x(s_center_rule)!=x || lv_obj_get_y(s_center_rule)!=y)
        lv_obj_set_pos(s_center_rule,x,y);
    if(lv_obj_get_width(s_center_rule)!=1 || lv_obj_get_height(s_center_rule)!=height)
        lv_obj_set_size(s_center_rule,1,height);
    lv_obj_remove_flag(s_center_rule,LV_OBJ_FLAG_HIDDEN);
}
static void tools_rule_deleted(lv_event_t *event)
{
    if(lv_event_get_target_obj(event)==s_center_rule)s_center_rule=NULL;
}
static void tools_rule_create(void)
{
    s_center_rule=ui_create_divider(s_root,0,0,1,1);
    if(!s_center_rule)return;
    lv_obj_add_flag(s_center_rule,LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(s_center_rule,tools_rule_deleted,LV_EVENT_DELETE,NULL);
    lv_obj_add_event_cb(s_tiles,tools_rule_update,LV_EVENT_LAYOUT_CHANGED,NULL);
    lv_obj_add_event_cb(s_tiles,tools_rule_update,LV_EVENT_SIZE_CHANGED,NULL);
    lv_obj_update_layout(s_root);
    tools_rule_update(NULL);
}

static void tools_size_changed(lv_event_t *event)
{
    (void)event;
    if (!s_tiles) return;
    lv_obj_set_size(s_tiles, lv_obj_get_content_width(s_root) - 40,
                    lv_obj_get_content_height(s_root) - 108);
}

static void tools_style_scroll_surface(void)
{
    ui_apply_scroll_surface(s_tiles);
}

static void studio_tools_create(void)
{
    studio_text(s_root,"Tools",0,0,976,&ui_studio_font_48,UI_TEXT);
    s_tiles=studio_plane(s_root,0,72,976,352);
    tools_style_scroll_surface();
    add_tile(LV_SYMBOL_REFRESH, UI_OK_BRIGHT,
             "Calibration", "Tune and validate your printer.", 0);
    add_tile(LV_SYMBOL_IMAGE, UI_ACCENT_BRIGHT,
             "Bed mesh", "Probe and visualize the bed surface.", 1);
    add_tile(LV_SYMBOL_CHARGE, UI_WARN,
             "Devices", "Inspect devices and live readings.", 2);
    add_tile(LV_SYMBOL_PLAY, UI_OK_BRIGHT,
             "Macros", "Run available printer macros.", 3);
}

void ui_tools_show(void)
{
    if (s_root) {
        lv_obj_clear_flag(s_root, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(s_root);
        return;
    }

    s_root = lv_obj_create(lv_screen_active());
    lv_obj_add_event_cb(s_root,tools_deleted,LV_EVENT_DELETE,NULL);
    lv_obj_set_size(s_root, UI_PAGE_ROOT_WIDTH, UI_PAGE_ROOT_HEIGHT);
    lv_obj_set_pos(s_root, UI_PAGE_ROOT_X, UI_PAGE_ROOT_Y);
    lv_obj_clear_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);
    ui_apply_root_style(s_root);
    if(ui_theme_is_studio()){studio_tools_create();tools_rule_create();s_connection_timer=lv_timer_create(tools_connection_refresh,250,NULL);tools_connection_refresh(NULL);return;}
    ui_page_title_create(s_root, LV_SYMBOL_SETTINGS " Tools",
                         "Calibration and operator utilities");

    s_tiles = lv_obj_create(s_root);
    ui_apply_surface_role(s_tiles, UI_SURFACE_TRANSPARENT);
    tools_style_scroll_surface();
    lv_obj_set_style_pad_all(s_tiles, 0, 0);
    lv_obj_set_pos(s_tiles, 20, 88);
    lv_obj_set_size(s_tiles, lv_pct(100), lv_pct(100));
    lv_obj_set_style_max_width(s_tiles, UI_PAGE_ROOT_WIDTH - 40, 0);
    lv_obj_set_style_max_height(s_tiles, UI_PAGE_ROOT_HEIGHT - 108, 0);
    lv_obj_set_scroll_dir(s_tiles, LV_DIR_VER);
    ui_responsive_cards(s_tiles, 340);
    lv_obj_add_event_cb(s_root, tools_size_changed, LV_EVENT_SIZE_CHANGED, NULL);
    lv_obj_update_layout(s_root);
    tools_size_changed(NULL);

    add_tile(LV_SYMBOL_REFRESH, UI_OK_BRIGHT,
             "Calibration", "Tune and validate your printer.", 0);
    add_tile(LV_SYMBOL_IMAGE, UI_ACCENT_BRIGHT,
             "Bed mesh", "Probe and visualize the bed surface.", 1);
    add_tile(LV_SYMBOL_CHARGE, UI_WARN,
             "Devices", "Inspect devices and live readings.", 2);
    add_tile(LV_SYMBOL_PLAY, UI_OK_BRIGHT,
             "Macros", "Run available printer macros.", 3);
    tools_rule_create();
    lv_obj_send_event(s_tiles, LV_EVENT_SIZE_CHANGED, NULL);
    s_connection_timer=lv_timer_create(tools_connection_refresh,250,NULL);tools_connection_refresh(NULL);
}

void ui_tools_hide(void)
{
    if(s_connection_timer)lv_timer_delete(s_connection_timer);
    s_connection_timer=NULL;
    for(unsigned i=0;i<4;i++)s_body_labels[i]=NULL;
    /* Reopen under the current theme/density, as Files and Macros do. */
    if (s_root) lv_obj_delete(s_root);
    s_root = NULL;
    s_tiles = s_center_rule = NULL;
    ui_shell_raise_topbar();
    ui_shell_raise_nav();
}
