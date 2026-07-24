#!/usr/bin/env python3
from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[2]
MAIN = ROOT / "main"


def read(name: str) -> str:
    path = MAIN / name
    if not path.exists():
        raise RuntimeError(f"missing required file: {path}")
    return path.read_text()


def write(name: str, text: str) -> None:
    (MAIN / name).write_text(text)


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"expected one {label}, found {count}")
    return text.replace(old, new, 1)


chooser_h = r'''#pragma once

#include "lvgl.h"

typedef void (*ui_printer_chooser_select_cb_t)(int profile_index);
typedef void (*ui_printer_chooser_manage_cb_t)(lv_event_t *event);

/*
 * Theme-owned multi-printer landing page.
 *
 * Only lightweight /server/info probes are made here. Full Moonraker polling
 * remains exclusively owned by the active printer.
 */
void ui_printer_chooser_v32_show(
    ui_printer_chooser_select_cb_t select_cb,
    ui_printer_chooser_manage_cb_t manage_cb);

void ui_printer_chooser_v32_hide(void);
bool ui_printer_chooser_v32_is_visible(void);
void ui_printer_chooser_v32_refresh(void);
'''


chooser_c = r'''#include "ui_printer_chooser_v32.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "moonraker.h"
#include "moonraker_config_controller.h"
#include "moonraker_probe.h"
#include "ui_button.h"
#include "ui_theme.h"

#define CHOOSER_PROBE_PERIOD_TICKS 20

typedef struct {
    lv_obj_t *root;
    lv_obj_t *name;
    lv_obj_t *endpoint;
    lv_obj_t *status;
    lv_obj_t *preview;
    lv_obj_t *active;
} chooser_card_t;

typedef struct {
    bool configured;
    char host[MOONRAKER_CONFIG_HOST_LENGTH];
    int port;
} chooser_probe_target_t;

static lv_obj_t *s_root = NULL;
static lv_timer_t *s_timer = NULL;
static chooser_card_t s_cards[MOONRAKER_CONFIG_MAX_PROFILES];

static ui_printer_chooser_select_cb_t s_select_cb = NULL;
static ui_printer_chooser_manage_cb_t s_manage_cb = NULL;

static chooser_probe_target_t
    s_probe_targets[MOONRAKER_CONFIG_MAX_PROFILES];

static bool s_probe_online[MOONRAKER_CONFIG_MAX_PROFILES];
static bool s_displayed_online[MOONRAKER_CONFIG_MAX_PROFILES];
static bool s_probe_running = false;
static bool s_probe_ready = false;
static uint32_t s_probe_generation = 0;
static uint32_t s_probe_ticks = CHOOSER_PROBE_PERIOD_TICKS;
static portMUX_TYPE s_probe_lock = portMUX_INITIALIZER_UNLOCKED;


static void apply_status_style(lv_obj_t *label, bool configured, bool online)
{
    if (!label) return;

    if (!configured) {
        ui_apply_label_dim(label);
    } else if (online) {
        ui_apply_label_success(label);
    } else {
        ui_apply_label_error(label);
    }
}


static void card_clicked_cb(lv_event_t *event)
{
    int index = (int)(intptr_t)lv_event_get_user_data(event);

    if (index < 0 || index >= MOONRAKER_CONFIG_MAX_PROFILES) return;

    const moonraker_profile_t *profile = moonraker_config_profile(index);

    if (!profile || !profile->configured) {
        if (s_manage_cb) s_manage_cb(event);
        return;
    }

    if (s_select_cb) s_select_cb(index);
}


static lv_obj_t *make_label(
    lv_obj_t *parent,
    const char *text,
    int x,
    int y,
    int width)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text ? text : "");
    lv_obj_set_width(label, width);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(label, x, y);
    return label;
}


static void create_card(int index, int x, int y)
{
    chooser_card_t *card = &s_cards[index];

    card->root = lv_obj_create(s_root);
    lv_obj_set_size(card->root, 390, 184);
    lv_obj_set_pos(card->root, x, y);
    lv_obj_clear_flag(card->root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(card->root, LV_OBJ_FLAG_CLICKABLE);
    ui_apply_card_style(card->root);

    lv_obj_add_event_cb(
        card->root,
        card_clicked_cb,
        LV_EVENT_CLICKED,
        (void *)(intptr_t)index);

    lv_obj_t *preview_box = lv_obj_create(card->root);
    lv_obj_set_size(preview_box, 116, 116);
    lv_obj_set_pos(preview_box, 12, 34);
    lv_obj_clear_flag(preview_box, LV_OBJ_FLAG_SCROLLABLE);
    ui_apply_preview_style(preview_box);

    lv_obj_t *preview_icon = lv_label_create(preview_box);
    lv_label_set_text(preview_icon, LV_SYMBOL_FILE);
    ui_apply_text_popup_title(preview_icon);
    ui_apply_label_dim(preview_icon);
    lv_obj_align(preview_icon, LV_ALIGN_TOP_MID, 0, 16);

    card->preview = lv_label_create(preview_box);
    lv_label_set_text(card->preview, "NO LIVE\nPREVIEW");
    lv_obj_set_width(card->preview, 96);
    lv_label_set_long_mode(card->preview, LV_LABEL_LONG_DOT);
    ui_apply_text_caption(card->preview);
    ui_apply_label_dim(card->preview);
    lv_obj_set_style_text_align(card->preview, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(card->preview, LV_ALIGN_BOTTOM_MID, 0, -14);

    card->name = make_label(card->root, "PRINTER", 146, 20, 210);
    ui_apply_text_title(card->name);
    ui_apply_label_bright(card->name);

    card->endpoint = make_label(card->root, "--", 146, 55, 220);
    ui_apply_text_caption(card->endpoint);
    ui_apply_label_dim(card->endpoint);

    card->status = make_label(card->root, "CHECKING...", 146, 88, 220);
    ui_apply_text_body_large(card->status);
    ui_apply_label_dim(card->status);

    lv_obj_t *hint = make_label(card->root, "TAP TO OPEN", 146, 128, 210);
    ui_apply_text_caption(hint);
    ui_apply_label_dim(hint);

    card->active = make_label(card->root, "ACTIVE", 292, 12, 72);
    ui_apply_text_caption(card->active);
    ui_apply_label_success(card->active);
    lv_obj_set_style_text_align(card->active, LV_TEXT_ALIGN_RIGHT, 0);
}


static void refresh_cards(void)
{
    int active = moonraker_config_active_profile_index();
    const moonraker_state_t *state = moonraker_state_get();

    for (int index = 0; index < MOONRAKER_CONFIG_MAX_PROFILES; ++index) {
        chooser_card_t *card = &s_cards[index];
        const moonraker_profile_t *profile = moonraker_config_profile(index);
        bool configured = profile && profile->configured;
        bool online = configured && s_displayed_online[index];

        if (configured && index == active && state && state->moonraker_ok) {
            online = true;
        }

        if (!card->root) continue;

        if (!configured) {
            char empty_name[32];
            snprintf(empty_name, sizeof(empty_name), "ADD PRINTER %d", index + 1);
            lv_label_set_text(card->name, empty_name);
            lv_label_set_text(card->endpoint, "EMPTY PROFILE SLOT");
            lv_label_set_text(card->status, "NOT CONFIGURED");
            lv_label_set_text(card->preview, "ADD A\nPRINTER");
            lv_obj_add_flag(card->active, LV_OBJ_FLAG_HIDDEN);
            apply_status_style(card->status, false, false);
            continue;
        }

        char endpoint[96];
        snprintf(endpoint, sizeof(endpoint), "%s:%d", profile->host, profile->port);

        lv_label_set_text(card->name, profile->name);
        lv_label_set_text(card->endpoint, endpoint);
        const char *status_text = online ? "ONLINE" : "OFFLINE";

        if (index == active &&
            state &&
            state->moonraker_ok &&
            state->printer_state[0] &&
            strcmp(state->printer_state, "--") != 0) {
            status_text = state->printer_state;
        }

        lv_label_set_text(card->status, status_text);
        apply_status_style(card->status, true, online);

        if (index == active) {
            lv_obj_clear_flag(card->active, LV_OBJ_FLAG_HIDDEN);

            if (state && state->live_data_ok && state->printer_file[0]) {
                lv_label_set_text(card->preview, state->printer_file);
                ui_apply_label_bright(card->preview);
            } else {
                lv_label_set_text(card->preview, online ? "READY FOR\nLIVE DATA" : "NO LIVE\nPREVIEW");
                ui_apply_label_dim(card->preview);
            }
        } else {
            lv_obj_add_flag(card->active, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(card->preview, online ? "AVAILABLE\nTO OPEN" : "NO LIVE\nPREVIEW");
            ui_apply_label_dim(card->preview);
        }
    }
}


static void probe_task(void *argument)
{
    (void)argument;

    bool results[MOONRAKER_CONFIG_MAX_PROFILES] = {0};

    for (int index = 0; index < MOONRAKER_CONFIG_MAX_PROFILES; ++index) {
        if (s_probe_targets[index].configured) {
            results[index] = moonraker_probe_host(
                s_probe_targets[index].host,
                s_probe_targets[index].port);
        }
    }

    portENTER_CRITICAL(&s_probe_lock);

    memcpy(s_probe_online, results, sizeof(results));
    s_probe_ready = true;
    s_probe_running = false;

    portEXIT_CRITICAL(&s_probe_lock);
    vTaskDelete(NULL);
}


static void start_probe(void)
{
    portENTER_CRITICAL(&s_probe_lock);

    if (s_probe_running) {
        portEXIT_CRITICAL(&s_probe_lock);
        return;
    }

    memset(s_probe_targets, 0, sizeof(s_probe_targets));

    for (int index = 0; index < MOONRAKER_CONFIG_MAX_PROFILES; ++index) {
        const moonraker_profile_t *profile = moonraker_config_profile(index);

        if (profile && profile->configured) {
            s_probe_targets[index].configured = true;
            strlcpy(
                s_probe_targets[index].host,
                profile->host,
                sizeof(s_probe_targets[index].host));
            s_probe_targets[index].port = profile->port;
        }
    }

    s_probe_generation = moonraker_config_generation();
    s_probe_ready = false;
    s_probe_running = true;

    portEXIT_CRITICAL(&s_probe_lock);

    BaseType_t created = xTaskCreate(
        probe_task,
        "printer_probe",
        4096,
        NULL,
        4,
        NULL);

    if (created != pdPASS) {
        portENTER_CRITICAL(&s_probe_lock);
        s_probe_running = false;
        portEXIT_CRITICAL(&s_probe_lock);
    }
}


static void chooser_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    bool ready = false;
    bool results[MOONRAKER_CONFIG_MAX_PROFILES] = {0};
    uint32_t result_generation = 0;

    portENTER_CRITICAL(&s_probe_lock);

    if (s_probe_ready) {
        memcpy(results, s_probe_online, sizeof(results));
        result_generation = s_probe_generation;
        s_probe_ready = false;
        ready = true;
    }

    portEXIT_CRITICAL(&s_probe_lock);

    if (ready && result_generation == moonraker_config_generation()) {
        memcpy(s_displayed_online, results, sizeof(results));
    }

    refresh_cards();

    if (++s_probe_ticks >= CHOOSER_PROBE_PERIOD_TICKS) {
        s_probe_ticks = 0;
        start_probe();
    }
}


void ui_printer_chooser_v32_refresh(void)
{
    if (!s_root) return;
    refresh_cards();
    s_probe_ticks = CHOOSER_PROBE_PERIOD_TICKS;
}


void ui_printer_chooser_v32_show(
    ui_printer_chooser_select_cb_t select_cb,
    ui_printer_chooser_manage_cb_t manage_cb)
{
    s_select_cb = select_cb;
    s_manage_cb = manage_cb;

    if (s_root) {
        refresh_cards();
        lv_obj_move_foreground(s_root);
        return;
    }

    s_root = lv_obj_create(lv_screen_active());
    lv_obj_set_size(s_root, 854, 528);
    lv_obj_set_pos(s_root, 162, 72);
    lv_obj_clear_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);
    ui_apply_root_style(s_root);

    lv_obj_t *title = make_label(s_root, "PRINTERS", 20, 12, 360);
    ui_apply_text_heading(title);
    ui_apply_label_bright(title);

    lv_obj_t *subtitle = make_label(
        s_root,
        "Choose a printer to open its live operator dashboard.",
        20,
        48,
        560);
    ui_apply_text_caption(subtitle);
    ui_apply_label_dim(subtitle);

    lv_obj_t *manage = ui_button_create_icon(
        s_root,
        UI_BUTTON_OUTLINED,
        LV_SYMBOL_SETTINGS,
        "MANAGE PRINTERS",
        UI_ACCENT_CYAN,
        UI_BUTTON_ICON_HORIZONTAL);

    if (manage) {
        lv_obj_set_size(manage, 220, 46);
        lv_obj_set_pos(manage, 606, 12);

        if (s_manage_cb) {
            lv_obj_add_event_cb(
                manage,
                s_manage_cb,
                LV_EVENT_CLICKED,
                NULL);
        }
    }

    create_card(0, 20, 76);
    create_card(1, 424, 76);
    create_card(2, 20, 274);
    create_card(3, 424, 274);

    refresh_cards();

    s_probe_ticks = CHOOSER_PROBE_PERIOD_TICKS;
    s_timer = lv_timer_create(chooser_timer_cb, 500, NULL);
    chooser_timer_cb(s_timer);

    lv_obj_move_foreground(s_root);
}


void ui_printer_chooser_v32_hide(void)
{
    if (s_timer) {
        lv_timer_delete(s_timer);
        s_timer = NULL;
    }

    if (s_root) {
        lv_obj_delete(s_root);
        s_root = NULL;
    }

    memset(s_cards, 0, sizeof(s_cards));
    s_select_cb = NULL;
    s_manage_cb = NULL;
}


bool ui_printer_chooser_v32_is_visible(void)
{
    return s_root != NULL;
}
'''


cmake = read("CMakeLists.txt")
if '"ui_printer_chooser_v32.c"' not in cmake:
    cmake = replace_once(
        cmake,
        '        "ui_printer_profiles.c"\n',
        '        "ui_printer_profiles.c"\n'
        '        "ui_printer_chooser_v32.c"\n',
        "profile-manager CMake registration")
shell_h = read("ui_shell.h")
if "ui_shell_set_active_printer_name" not in shell_h:
    shell_h += r'''

/* Persistent active-machine identity displayed on every operator page. */
void ui_shell_set_active_printer_name(const char *printer_name);
'''
shell_c = read("ui_shell.c")
if "ui_shell_set_active_printer_name" not in shell_c:
    title_create = re.compile(
        r'(?P<indent>[ \t]*)lv_obj_t \*title_label\s*=\s*\n?'
        r'(?P=indent)[ \t]*lv_label_create\(shell_top_bar\);')

    matches = list(title_create.finditer(shell_c))
    if len(matches) != 1:
        raise RuntimeError(
            f"expected one shell title-label creation, found {len(matches)}")

    match = matches[0]
    indent = match.group("indent")
    replacement = (
        f"{indent}s_shell_title_label =\n"
        f"{indent}    lv_label_create(shell_top_bar);\n\n"
        f"{indent}lv_obj_t *title_label =\n"
        f"{indent}    s_shell_title_label;")
    shell_c = shell_c[:match.start()] + replacement + shell_c[match.end():]

    static_anchor = re.compile(r'(static lv_obj_t \*shell_top_bar\s*=\s*NULL;\s*)')
    shell_c, count = static_anchor.subn(
        r'\1\nstatic lv_obj_t *s_shell_title_label = NULL;\n',
        shell_c,
        count=1)
    if count != 1:
        raise RuntimeError("expected one shell top-bar static anchor")

    shell_c += r'''


void ui_shell_set_active_printer_name(const char *printer_name)
{
    if (!s_shell_title_label) return;

    char title[96];

    if (printer_name && printer_name[0]) {
        lv_snprintf(
            title,
            sizeof(title),
            "PRINTERHMI  |  %s",
            printer_name);
    } else {
        lv_snprintf(title, sizeof(title), "PRINTERHMI");
    }

    lv_label_set_text(s_shell_title_label, title);
}
'''
main = read("main.c")

if '#include "ui_printer_chooser_v32.h"' not in main:
    main = replace_once(
        main,
        '#include "ui_printer_profiles.h"\n',
        '#include "ui_printer_profiles.h"\n'
        '#include "ui_printer_chooser_v32.h"\n',
        "printer-profiles include")


bridge_old = r'''static void printer_profiles_active_changed_bridge(void)
{
    reset_active_printer_runtime_state();
'''

bridge_new = r'''static void printer_profiles_active_changed_bridge(void)
{
    reset_active_printer_runtime_state();

    ui_shell_set_active_printer_name(
        moonraker_config_active_profile_name());

    ui_printer_chooser_v32_refresh();
'''

if "ui_printer_chooser_v32_refresh();" not in main:
    main = replace_once(
        main,
        bridge_old,
        bridge_new,
        "active-profile switch bridge")


chooser_bridges = r'''

static void printer_chooser_select_bridge(int profile_index)
{
    int previous = moonraker_config_active_profile_index();

    if (!moonraker_config_select_profile(profile_index)) {
        return;
    }

    if (previous != moonraker_config_active_profile_index()) {
        printer_profiles_active_changed_bridge();
    } else {
        ui_shell_set_active_printer_name(
            moonraker_config_active_profile_name());
    }

    ui_printer_chooser_v32_hide();
    ui_dashboard_v32_create();
    ui_shell_set_active_nav(UI_SHELL_PAGE_DASHBOARD);
}


static void printer_chooser_manage_bridge(lv_event_t *event)
{
    (void)event;

    ui_printer_profiles_show(
        printer_profiles_active_changed_bridge);
}
'''

if "static void printer_chooser_select_bridge" not in main:
    anchor = "static void show_settings_tab(void);\n"
    main = replace_once(
        main,
        anchor,
        anchor + chooser_bridges,
        "page-routing declaration anchor")


routing_old = r'''void ui_shell_page_action(ui_shell_page_t page)
{
    switch (page) {
    case UI_SHELL_PAGE_DASHBOARD:
        ui_telemetry_v32_hide();
        ui_files_v32_hide();
        ui_printer_v32_hide();
        ui_drybox_v32_hide();
        ui_network_v32_hide();
        hide_settings_tab();
        return;
'''

routing_new = r'''void ui_shell_page_action(ui_shell_page_t page)
{
    if (page != UI_SHELL_PAGE_DASHBOARD) {
        ui_printer_chooser_v32_hide();
    }

    switch (page) {
    case UI_SHELL_PAGE_DASHBOARD:
        ui_telemetry_v32_hide();
        ui_files_v32_hide();
        ui_printer_v32_hide();
        ui_drybox_v32_hide();
        ui_network_v32_hide();
        hide_settings_tab();

        ui_printer_chooser_v32_show(
            printer_chooser_select_bridge,
            printer_chooser_manage_bridge);
        return;
'''

if "if (page != UI_SHELL_PAGE_DASHBOARD)" not in main:
    main = replace_once(
        main,
        routing_old,
        routing_new,
        "Dashboard shell routing block")


load_old = "    moonraker_config_load();\n"
load_new = (
    "    moonraker_config_load();\n\n"
    "    ui_shell_set_active_printer_name(\n"
    "        moonraker_config_active_profile_name());\n")

if "moonraker_config_load();\n\n    ui_shell_set_active_printer_name" not in main:
    main = replace_once(
        main,
        load_old,
        load_new,
        "Moonraker profile load")


# The startup Dashboard is constructed before Wi-Fi/profile loading. Place the
# chooser above it immediately; its cards refresh when profiles finish loading.
build_match = re.search(
    r'(static void build_drybox_dashboard\(void\)\s*\{.*?\n\})',
    main,
    flags=re.S)
if not build_match:
    raise RuntimeError("could not locate build_drybox_dashboard")

build = build_match.group(1)
if "ui_printer_chooser_v32_show(" not in build:
    dashboard_calls = list(re.finditer(
        r'(?m)^(?P<i>[ \t]*)ui_dashboard_v32_create\(\);',
        build))
    if len(dashboard_calls) != 1:
        raise RuntimeError(
            "expected one Dashboard creation in build_drybox_dashboard, "
            f"found {len(dashboard_calls)}")

    call = dashboard_calls[0]
    indent = call.group("i")
    addition = (
        call.group(0) + "\n\n" +
        indent + "ui_printer_chooser_v32_show(\n" +
        indent + "    printer_chooser_select_bridge,\n" +
        indent + "    printer_chooser_manage_bridge);")
    build = build[:call.start()] + addition + build[call.end():]
    main = main[:build_match.start()] + build + main[build_match.end():]

write("ui_printer_chooser_v32.h", chooser_h)
write("ui_printer_chooser_v32.c", chooser_c)
write("CMakeLists.txt", cmake)
write("ui_shell.h", shell_h)
write("ui_shell.c", shell_c)
write("main.c", main)


print("PASS: multi-printer Dashboard chooser installed")
print("  - four Theme B profile cards")
print("  - staggered background online/offline probes")
print("  - active printer live file preview summary")
print("  - Dashboard nav returns to chooser")
print("  - active printer name persists in the shell top bar")
print("Next: idf.py build")
