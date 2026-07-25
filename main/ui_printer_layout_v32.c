#include "ui_printer_layout_v32.h"
#include "ui_page_layout_profile.h"

#include <string.h>

#include "ui_theme.h"
#include "ui_widgets.h"
#include "ui_page_geometry_v32.h"

static lv_obj_t *make_transparent_container(
    lv_obj_t *parent,
    int x,
    int y,
    int width,
    int height)
{
    lv_obj_t *container = lv_obj_create(parent);

    lv_obj_clear_flag(
        container,
        LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_size(container, width, height);
    lv_obj_set_pos(container, x, y);

    lv_obj_set_style_bg_opa(
        container,
        LV_OPA_TRANSP,
        0);

    lv_obj_set_style_border_width(
        container,
        0,
        0);

    lv_obj_set_style_radius(
        container,
        0,
        0);

    lv_obj_set_style_pad_all(
        container,
        0,
        0);

    return container;
}

bool ui_printer_layout_v32_create(
    lv_obj_t *page,
    ui_printer_layout_v32_t *layout)
{
    if (!page || !layout) {
        return false;
    }

    memset(layout, 0, sizeof(*layout));

    const ui_printer_layout_profile_t *profile =
        &ui_page_layout_profile_current()->printer;

    /*
     * Drybox uses an unpadded 854x528 page root, then places every
     * content surface on a 20px / 800px grid.
     */
    lv_obj_set_style_pad_all(page, 0, 0);

    layout->active_panel =
        ui_create_operator_card(
            page,
            profile->active.x,
            profile->active.y,
            profile->active.width,
            profile->active.height);

    if (!layout->active_panel) {
        return false;
    }

    layout->status_panel =
        make_transparent_container(
            page,
            profile->status.x,
            profile->status.y,
            profile->status.width,
            profile->status.height);

    layout->action_panel =
        make_transparent_container(
            page,
            profile->actions.x,
            profile->actions.y,
            profile->actions.width,
            profile->actions.height);

    if (!layout->status_panel ||
        !layout->action_panel) {
        return false;
    }

    return true;
}

void ui_printer_layout_v32_clear_refs(
    ui_printer_layout_v32_t *layout)
{
    if (!layout) {
        return;
    }

    memset(layout, 0, sizeof(*layout));
}
