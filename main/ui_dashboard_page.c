#include "ui_dashboard_page.h"

#include <string.h>

#include "ui_dashboard_layout_profile.h"

#include "ui_theme.h"
#include "ui_status_banner.h"
#include "ui_active_print.h"
#include "ui_machine_status.h"
#include "ui_command_bar.h"
#include "ui_page_title.h"
#include "ui_page_geometry.h"


static lv_obj_t *future_orbital_node(
    lv_obj_t *parent,
    int32_t x,
    int32_t y,
    int32_t size,
    const char *title,
    const char *value,
    lv_color_t edge)
{
    lv_obj_t *node = lv_obj_create(parent);
    if (!node) return NULL;

    lv_obj_set_size(node, size, size);
    lv_obj_set_pos(node, x, y);
    lv_obj_clear_flag(node, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(node, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(node, UI_CARD_DARK, 0);
    lv_obj_set_style_bg_opa(node, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(node, edge, 0);
    lv_obj_set_style_border_width(node, 3, 0);
    lv_obj_set_style_shadow_color(node, edge, 0);
    lv_obj_set_style_shadow_width(node, 24, 0);
    lv_obj_set_style_shadow_opa(node, LV_OPA_50, 0);

    lv_obj_t *heading = lv_label_create(node);
    lv_label_set_text(heading, title);
    ui_apply_text_caption(heading);
    ui_apply_label_dim(heading);
    lv_obj_align(heading, LV_ALIGN_CENTER, 0, -18);

    lv_obj_t *reading = lv_label_create(node);
    lv_label_set_text(reading, value);
    ui_apply_text_title(reading);
    ui_apply_label_bright(reading);
    lv_obj_align(reading, LV_ALIGN_CENTER, 0, 14);
    return node;
}


static void future_orbital_dashboard(lv_obj_t *root)
{
    lv_obj_t *space = lv_obj_create(root);
    if (!space) return;

    lv_obj_set_size(space, UI_PAGE_ROOT_WIDTH, UI_PAGE_ROOT_HEIGHT);
    lv_obj_set_pos(space, 0, 0);
    lv_obj_clear_flag(space, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(space, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(space, 0, 0);
    lv_obj_move_foreground(space);

    lv_obj_t *orbit = lv_arc_create(space);
    lv_obj_set_size(orbit, 500, 500);
    lv_obj_align(orbit, LV_ALIGN_CENTER, 40, 16);
    lv_arc_set_range(orbit, 0, 100);
    lv_arc_set_value(orbit, 68);
    lv_obj_remove_style(orbit, NULL, LV_PART_KNOB);
    lv_obj_set_style_arc_color(orbit, UI_ACCENT_CYAN, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(orbit, 4, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(orbit, UI_BORDER_SOFT, LV_PART_MAIN);
    lv_obj_set_style_arc_width(orbit, 2, LV_PART_MAIN);
    lv_obj_clear_flag(orbit, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *core = lv_obj_create(space);
    lv_obj_set_size(core, 190, 190);
    lv_obj_align(core, LV_ALIGN_CENTER, 40, 16);
    lv_obj_clear_flag(core, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(core, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(core, UI_PANEL, 0);
    lv_obj_set_style_bg_opa(core, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(core, UI_ACCENT_PURPLE, 0);
    lv_obj_set_style_border_width(core, 4, 0);
    lv_obj_set_style_shadow_color(core, UI_ACCENT_PURPLE, 0);
    lv_obj_set_style_shadow_width(core, 34, 0);
    lv_obj_set_style_shadow_opa(core, LV_OPA_60, 0);

    lv_obj_t *core_title = lv_label_create(core);
    lv_label_set_text(core_title, "PRINT CELL");
    ui_apply_text_caption(core_title);
    ui_apply_label_dim(core_title);
    lv_obj_align(core_title, LV_ALIGN_CENTER, 0, -24);

    lv_obj_t *core_value = lv_label_create(core);
    lv_label_set_text(core_value, "READY");
    ui_apply_text_title(core_value);
    ui_apply_label_bright(core_value);
    lv_obj_align(core_value, LV_ALIGN_CENTER, 0, 8);

    lv_obj_t *core_detail = lv_label_create(core);
    lv_label_set_text(core_detail, "ORBITAL LINK");
    ui_apply_text_caption(core_detail);
    ui_apply_label_primary(core_detail);
    lv_obj_align(core_detail, LV_ALIGN_CENTER, 0, 38);

    future_orbital_node(space, 34, 54, 142, "NOZZLE", "-- / -- C", UI_ACCENT_CYAN);
    future_orbital_node(space, 676, 52, 142, "BED", "-- / -- C", UI_ACCENT_BRIGHT);
    future_orbital_node(space, 54, 344, 142, "PROGRESS", "0%", UI_ACCENT_PURPLE);
    future_orbital_node(space, 652, 344, 142, "CAMERA", "LIVE", UI_ACCENT_CYAN);

    lv_obj_t *footer = lv_label_create(space);
    lv_label_set_text(footer, "FUTURE OPERATING ENVIRONMENT  //  LIVE TELEMETRY");
    ui_apply_text_caption(footer);
    ui_apply_label_dim(footer);
    lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -18);
}


ui_dashboard_page_t ui_dashboard_page_create(
    lv_obj_t *parent)
{
    ui_dashboard_page_t page = {0};

    if (!parent) {
        return page;
    }

    /*
     * TEST11_CLEAN_DASHBOARD_PAGE
     *
     * Clean replacement Dashboard composition.
     * The page remains inactive until navigation is switched later.
     */
    page.root = lv_obj_create(parent);

    lv_obj_clear_flag(
        page.root,
        LV_OBJ_FLAG_SCROLLABLE);

    /*
     * TEST12_DASHBOARD_CONTENT_GEOMETRY
     *
     * Preserve the existing application shell:
     *   left navigation and gutter: 170 px
     *   top bar:          72 px
     *   Dashboard area:   854 x 528
     */
    lv_obj_set_size(
        page.root,
        UI_PAGE_ROOT_WIDTH,
        UI_PAGE_ROOT_HEIGHT);

    lv_obj_set_pos(
        page.root,
        UI_PAGE_ROOT_X,
        UI_PAGE_ROOT_Y);

    ui_apply_root_style(page.root);

    const ui_dashboard_layout_profile_t *layout =
        ui_dashboard_layout_profile_current();

    ui_page_title_create(
        page.root,
        LV_SYMBOL_HOME " DASHBOARD",
        layout->subtitle);

    page.banner_host =
        ui_status_banner_create(
            page.root,
            layout->banner.x,
            layout->banner.y,
            layout->banner.width,
            layout->banner.height);

    page.active_print_host =
        ui_active_print_create_profile(
            page.root,
            &layout->active_print,
            &layout->active_content);

    page.machine_status_host =
        ui_machine_status_create_profile(
            page.root,
            &layout->machine_status,
            &layout->machine_content);

    /* Print timing now lives in the Active Print card footer. */
    page.print_status = (ui_dashboard_status_t){0};
    page.print_status_host = NULL;

    page.command_host =
        ui_command_bar_create(
            page.root,
            layout->command_bar.x,
            layout->command_bar.y,
            layout->command_bar.width,
            layout->command_bar.height);

    if (ui_theme_get_active() == UI_THEME_FUTURE) {
        lv_obj_add_flag(page.banner_host, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(page.active_print_host, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(page.machine_status_host, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(page.command_host, LV_OBJ_FLAG_HIDDEN);
        future_orbital_dashboard(page.root);
    }

    return page;
}

void ui_dashboard_page_destroy(
    ui_dashboard_page_t *page)
{
    if (!page) {
        return;
    }

    if (page->root) {
        lv_obj_delete(page->root);
    }

    memset(
        page,
        0,
        sizeof(*page));
}
