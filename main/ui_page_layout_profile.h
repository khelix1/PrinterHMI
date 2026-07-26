#pragma once

#include "ui_dashboard_layout_profile.h"

typedef struct {
    const char *subtitle;
    ui_dashboard_rect_t environment;
    ui_dashboard_rect_t drying_system;
    ui_dashboard_rect_t material_program;
} ui_drybox_layout_profile_t;

typedef struct {
    const char *subtitle;
    ui_dashboard_rect_t active;
    ui_dashboard_rect_t status;
    ui_dashboard_rect_t actions;
} ui_printer_layout_profile_t;

typedef struct {
    const char *subtitle;
    ui_dashboard_rect_t breadcrumb;
    ui_dashboard_rect_t up;
    ui_dashboard_rect_t search;
    ui_dashboard_rect_t sort;
    ui_dashboard_rect_t refresh;
    ui_dashboard_rect_t list;
} ui_files_layout_profile_t;

typedef struct {
    const char *subtitle;
    ui_dashboard_rect_t wifi;
    ui_dashboard_rect_t moonraker;
    ui_dashboard_rect_t networks;
    ui_dashboard_rect_t actions;
} ui_network_layout_profile_t;

typedef struct {
    const char *subtitle;
    ui_dashboard_rect_t banner;
    ui_dashboard_rect_t content;
} ui_settings_layout_profile_t;

typedef struct {
    const char *subtitle;
    int metric_x[4];
    ui_dashboard_rect_t charts;
} ui_telemetry_layout_profile_t;

typedef struct {
    ui_drybox_layout_profile_t drybox;
    ui_printer_layout_profile_t printer;
    ui_files_layout_profile_t files;
    ui_network_layout_profile_t network;
    ui_settings_layout_profile_t settings;
    ui_telemetry_layout_profile_t telemetry;
} ui_page_layout_profile_t;

const ui_page_layout_profile_t *ui_page_layout_profile_current(void);
const ui_page_layout_profile_t *
ui_page_layout_profile_for_theme(ui_theme_id_t theme);
