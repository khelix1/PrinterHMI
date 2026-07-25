#include "ui_dashboard_page_v32.h"

#include <string.h>

#include "ui_dashboard_layout_profile.h"

#include "ui_theme.h"
#include "ui_status_banner_v32.h"
#include "ui_active_print_v32.h"
#include "ui_machine_status_v32.h"
#include "ui_command_bar_v32.h"
#include "ui_page_title.h"
#include "ui_page_geometry_v32.h"

ui_dashboard_page_v32_t ui_dashboard_page_v32_create(
    lv_obj_t *parent)
{
    ui_dashboard_page_v32_t page = {0};

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
        ui_status_banner_v32_create(
            page.root,
            layout->banner.x,
            layout->banner.y,
            layout->banner.width,
            layout->banner.height);

    page.active_print_host =
        ui_active_print_v32_create_profile(
            page.root,
            &layout->active_print,
            &layout->active_content);

    page.machine_status_host =
        ui_machine_status_v32_create_profile(
            page.root,
            &layout->machine_status,
            &layout->machine_content);

    /* Print timing now lives in the Active Print card footer. */
    page.print_status = (ui_dashboard_status_v32_t){0};
    page.print_status_host = NULL;

    page.command_host =
        ui_command_bar_v32_create(
            page.root,
            layout->command_bar.x,
            layout->command_bar.y,
            layout->command_bar.width,
            layout->command_bar.height);

    return page;
}

void ui_dashboard_page_v32_destroy(
    ui_dashboard_page_v32_t *page)
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
