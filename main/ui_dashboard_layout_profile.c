#include "ui_dashboard_layout_profile.h"

/*
 * Dashboard composition belongs here. The page model, live setters,
 * thumbnail session, and action callbacks remain shared by every theme.
 *
 * Operator deliberately retains the verified production geometry.
 */
static const ui_dashboard_layout_profile_t s_operator = {
    .subtitle = "Printer and Drybox Overview",
    .banner = {20, 52, 800, 54},
    .active_print = {20, 126, 390, 306},
    .machine_status = {430, 126, 390, 306},
    .command_bar = {20, 444, 800, 64},
    .active_content = {
        .heading_x = 18,
        .heading_y = 12,
        .filename_x = 140,
        .filename_y = 14,
        .preview_x = 20,
        .preview_y = 42,
        .preview_width = 0,
        .preview_height = 0,
        .footer_x = 18,
        .footer_bottom = 31,
    },
    .machine_content = {
        .composition = UI_DASHBOARD_MACHINE_SINGLE_CARD,
        .label_x = 20,
        .value_x = 188,
        .split_gap = 12,
    },
};

/*
 * Foundry treats the print as the primary workshop artifact. The preview
 * receives the larger card while machine instrumentation stays compact.
 */
static const ui_dashboard_layout_profile_t s_foundry = {
    .subtitle = "Workshop Production Overview",
    .banner = {20, 52, 800, 54},
    .active_print = {20, 126, 470, 306},
    .machine_status = {502, 126, 318, 306},
    .command_bar = {20, 444, 800, 64},
    .active_content = {
        .heading_x = 18,
        .heading_y = 12,
        .filename_x = 140,
        .filename_y = 14,
        .preview_x = 20,
        .preview_y = 42,
        .preview_width = 0,
        .preview_height = 0,
        .footer_x = 18,
        .footer_bottom = 31,
    },
    .machine_content = {
        .composition = UI_DASHBOARD_MACHINE_SINGLE_CARD,
        .label_x = 16,
        .value_x = 160,
        .split_gap = 12,
    },
};

/*
 * Dark Glass uses a compact print card and separates thermal and process
 * instrumentation into two tiles. This is the first composition with a
 * theme-specific card count.
 */
static const ui_dashboard_layout_profile_t s_glass = {
    .subtitle = "Glass Cell Instrumentation",
    .banner = {20, 52, 800, 54},
    .active_print = {20, 126, 350, 306},
    .machine_status = {382, 126, 438, 306},
    .command_bar = {20, 444, 800, 64},
    .active_content = {
        .heading_x = 18,
        .heading_y = 12,
        .filename_x = 118,
        .filename_y = 14,
        .preview_x = 20,
        .preview_y = 42,
        .preview_width = 0,
        .preview_height = 0,
        .footer_x = 18,
        .footer_bottom = 31,
    },
    .machine_content = {
        .composition = UI_DASHBOARD_MACHINE_SPLIT_CARDS,
        .label_x = 16,
        .value_x = 230,
        .split_gap = 12,
    },
};

const ui_dashboard_layout_profile_t *
ui_dashboard_layout_profile_for_theme(ui_theme_id_t theme)
{
    switch (theme) {
        case UI_THEME_CLASSIC:
            return &s_foundry;
        case UI_THEME_GLASS:
            return &s_glass;
        case UI_THEME_OPERATOR:
        default:
            return &s_operator;
    }
}

const ui_dashboard_layout_profile_t *
ui_dashboard_layout_profile_current(void)
{
    return ui_dashboard_layout_profile_for_theme(
        ui_theme_get_active());
}
