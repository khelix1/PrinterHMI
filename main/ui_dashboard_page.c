#include "ui_dashboard_page.h"

#include <string.h>
#include <stdio.h>

#include "ui_dashboard_layout_profile.h"

#include "ui_theme.h"
#include "ui_status_banner.h"
#include "ui_active_print.h"
#include "ui_machine_status.h"
#include "ui_command_bar.h"
#include "ui_page_title.h"
#include "ui_page_geometry.h"
#include "ui_command_bar.h"
#include "ui_shell.h"
static void future_orbital_camera_cb(lv_event_t *event);
static lv_obj_t *future_orbital_action(
    lv_obj_t *parent,
    int32_t x,
    int32_t y,
    const char *label,
    const char *action,
    lv_color_t edge);



static lv_obj_t *future_orbital_node(
    lv_obj_t *parent,
    int32_t x,
    int32_t y,
    int32_t size,
    const char *title,
    const char *value,
    lv_color_t edge,
    lv_obj_t **reading_out)
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
    if (reading_out) *reading_out = reading;
    return node;
}


static void future_orbital_action_cb(lv_event_t *event)
{
    if (!event || lv_event_get_code(event) != LV_EVENT_CLICKED) return;
    const char *action = (const char *)lv_event_get_user_data(event);
    if (action) ui_command_bar_action(action);
}


static void future_orbital_camera_cb(lv_event_t *event)
{
    (void)event;
    ui_shell_page_action(UI_SHELL_PAGE_CAMERA);
}


static lv_obj_t *future_orbital_action(
    lv_obj_t *parent,
    int32_t x,
    int32_t y,
    const char *label,
    const char *action,
    lv_color_t edge)
{
    lv_obj_t *button = lv_obj_create(parent);
    if (!button) return NULL;

    lv_obj_set_size(button, 78, 78);
    lv_obj_set_pos(button, x, y);
    lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(button, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(button, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(button, UI_CONTROL, 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(button, edge, 0);
    lv_obj_set_style_border_width(button, 2, 0);
    lv_obj_set_style_shadow_color(button, edge, 0);
    lv_obj_set_style_shadow_width(button, 18, 0);
    lv_obj_set_style_shadow_opa(button, LV_OPA_50, 0);

    lv_obj_t *text = lv_label_create(button);
    lv_label_set_text(text, label ? label : "");
    ui_apply_text_caption(text);
    ui_apply_label_bright(text);
    lv_obj_center(text);

    lv_obj_add_event_cb(button, future_orbital_action_cb,
                        LV_EVENT_CLICKED, (void *)action);
    lv_obj_add_flag(button, LV_OBJ_FLAG_HIDDEN);
    return button;
}


static void future_orbital_dashboard(lv_obj_t *root, ui_dashboard_page_t *page)
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
    lv_obj_set_size(orbit, 420, 420);
    lv_obj_align(orbit, LV_ALIGN_CENTER, 0, 16);
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
    lv_obj_align(core, LV_ALIGN_CENTER, 0, 16);
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
    if (page) page->future_core = core_value;

    lv_obj_t *core_detail = lv_label_create(core);
    lv_label_set_text(core_detail, "ORBITAL LINK");
    ui_apply_text_caption(core_detail);
    ui_apply_label_primary(core_detail);
    lv_obj_align(core_detail, LV_ALIGN_CENTER, 0, 38);

    /* Keep telemetry in the annulus between the outer orbit and core ring. */
    lv_obj_t *core_metrics = lv_label_create(space);
    lv_label_set_text(core_metrics, "SPD --\nFLOW --");
    ui_apply_text_caption(core_metrics);
    ui_apply_label_dim(core_metrics);
    lv_label_set_long_mode(core_metrics, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(core_metrics, 100);
    lv_obj_set_style_text_align(core_metrics, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(core_metrics, 224, 246);
    if (page) page->future_metrics = core_metrics;

    lv_obj_t *core_environment = lv_label_create(space);
    lv_label_set_text(core_environment, "FAN --\nCHM --\nHUM --");
    ui_apply_text_caption(core_environment);
    ui_apply_label_dim(core_environment);
    lv_label_set_long_mode(core_environment, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(core_environment, 100);
    lv_obj_set_style_text_align(core_environment, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(core_environment, 530, 234);
    if (page) page->future_environment = core_environment;

    future_orbital_node(space, 54, 54, 142, "NOZZLE", "-- / -- C", UI_ACCENT_CYAN,
                            page ? &page->future_nozzle : NULL);
    future_orbital_node(space, 658, 54, 142, "BED", "-- / -- C", UI_ACCENT_BRIGHT,
                            page ? &page->future_bed : NULL);
    future_orbital_node(space, 38, 350, 116, "PROGRESS", "0%", UI_ACCENT_PURPLE,
                            page ? &page->future_progress : NULL);
    lv_obj_t *camera_node =
        future_orbital_node(space, 700, 350, 116, "CAMERA", "LIVE", UI_ACCENT_CYAN,
                            page ? &page->future_camera : NULL);
    if (camera_node) {
        lv_obj_add_flag(camera_node, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(camera_node, future_orbital_camera_cb, LV_EVENT_CLICKED, NULL);
    }

    if (page) {
        page->future_pause = future_orbital_action(
            space, 232, 369, "PAUSE", "PAUSE", UI_ACCENT_BRIGHT);
        page->future_resume = future_orbital_action(
            space, 388, 369, "RESUME", "RESUME", UI_ACCENT_CYAN);
        page->future_cancel = future_orbital_action(
            space, 544, 369, "CANCEL", "CANCEL_PRINT", UI_TEXT_ERROR);
    }

    lv_obj_t *footer = lv_label_create(space);
    lv_label_set_text(footer, "FUTURE OPERATING ENVIRONMENT  //  LIVE TELEMETRY");
    ui_apply_text_caption(footer);
    ui_apply_label_dim(footer);
    lv_obj_add_flag(footer, LV_OBJ_FLAG_HIDDEN);
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
        future_orbital_dashboard(page.root, &page);
    }

    return page;
}

void ui_dashboard_page_future_update(
    ui_dashboard_page_t *page,
    const char *nozzle,
    const char *bed,
    const char *progress,
    const char *camera,
    const char *speed,
    const char *flow,
    const char *fan,
    const char *chamber,
    const char *humidity,
    const char *state)
{
    if (!page) return;
    if (page->future_nozzle && nozzle) lv_label_set_text(page->future_nozzle, nozzle);
    if (page->future_bed && bed) lv_label_set_text(page->future_bed, bed);
    if (page->future_progress && progress) lv_label_set_text(page->future_progress, progress);
    if (page->future_camera && camera) lv_label_set_text(page->future_camera, camera);
    if (page->future_core && state) lv_label_set_text(page->future_core, state);

    char metrics[64];
    snprintf(metrics, sizeof(metrics), "SPD %s\nFLOW %s",
             speed ? speed : "--", flow ? flow : "--");
    if (page->future_metrics) lv_label_set_text(page->future_metrics, metrics);
    char environment[96];
    snprintf(environment, sizeof(environment), "FAN %s\nCHM %s\nHUM %s",
             fan ? fan : "--", chamber ? chamber : "--", humidity ? humidity : "--");
    if (page->future_environment) lv_label_set_text(page->future_environment, environment);

    bool actions_visible = state && (strstr(state, "PRINTING") || strstr(state, "PAUSED"));
    lv_obj_t *actions[] = { page->future_pause, page->future_resume, page->future_cancel };
    for (size_t index = 0; index < sizeof(actions) / sizeof(actions[0]); ++index) {
        if (!actions[index]) continue;
        if (actions_visible) lv_obj_clear_flag(actions[index], LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(actions[index], LV_OBJ_FLAG_HIDDEN);
    }
    if (page->future_progress && page->future_camera) {
        if (actions_visible) {
            lv_obj_set_pos(page->future_progress, 38, 350);
            lv_obj_set_pos(page->future_camera, 700, 350);
        } else {
            lv_obj_set_pos(page->future_progress, 210, 350);
            lv_obj_set_pos(page->future_camera, 528, 350);
        }
    }
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

/* Future orbital controls already fit. */

/* Future orbital spacing normalized. */

/* Future lower band edge spacing normalized. */

/* Future dashboard fit and idle gating complete. */

/* Future Dashboard reconciled layout. */

/* Future Dashboard annular telemetry placement. */
