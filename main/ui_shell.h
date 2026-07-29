#pragma once

#include "lvgl.h"

typedef enum {
    UI_SHELL_PAGE_DASHBOARD = 0,
    UI_SHELL_PAGE_PRINTER,
    UI_SHELL_PAGE_FILES,
    UI_SHELL_PAGE_BED_MESH,
    UI_SHELL_PAGE_MACROS,
    UI_SHELL_PAGE_CONSOLE,
    UI_SHELL_PAGE_TELEMETRY,
    UI_SHELL_PAGE_DRYBOX,
    UI_SHELL_PAGE_NETWORK,
    UI_SHELL_PAGE_SETTINGS,
    UI_SHELL_PAGE_COUNT
} ui_shell_page_t;

void ui_shell_create(void);
void ui_shell_destroy(void);
void ui_shell_raise(void);

void ui_shell_raise_topbar(void);
void ui_shell_create_nav(void);
void ui_shell_raise_nav(void);
void ui_shell_set_active_nav(int idx);
void ui_shell_update_status_icons(void);
void ui_shell_refresh_clock(void);

/* Implemented by main.c during Phase 1. */
void ui_shell_page_action(ui_shell_page_t page);


/* Persistent active-machine identity displayed on every operator page. */
void ui_shell_set_active_printer_name(const char *printer_name);


/*
 * The shell owns the persistent top-bar control. Application routing owns
 * what opening the printer chooser does.
 */
typedef void (*ui_shell_printer_switch_cb_t)(void);

void ui_shell_set_printer_switch_callback(
    ui_shell_printer_switch_cb_t callback);
