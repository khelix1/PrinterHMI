#pragma once

#include "ui_theme.h"

typedef struct {
    int x;
    int y;
    int width;
    int height;
} ui_dashboard_rect_t;

typedef struct {
    int heading_x;
    int heading_y;
    int filename_x;
    int filename_y;
    int preview_x;
    int preview_y;
    int preview_width;
    int preview_height;
    int footer_x;
    int footer_bottom;
} ui_dashboard_active_print_layout_t;

typedef enum {
    UI_DASHBOARD_MACHINE_SINGLE_CARD = 0,
    UI_DASHBOARD_MACHINE_SPLIT_CARDS
} ui_dashboard_machine_composition_t;

typedef struct {
    ui_dashboard_machine_composition_t composition;
    int label_x;
    int value_x;
    int split_gap;
} ui_dashboard_machine_layout_t;

typedef struct {
    const char *subtitle;
    ui_dashboard_rect_t banner;
    ui_dashboard_rect_t active_print;
    ui_dashboard_rect_t machine_status;
    ui_dashboard_rect_t command_bar;
    ui_dashboard_active_print_layout_t active_content;
    ui_dashboard_machine_layout_t machine_content;
} ui_dashboard_layout_profile_t;

const ui_dashboard_layout_profile_t *
ui_dashboard_layout_profile_for_theme(ui_theme_id_t theme);

const ui_dashboard_layout_profile_t *
ui_dashboard_layout_profile_current(void);
