#!/usr/bin/env python3
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
MAIN = ROOT / "main"


def read(name: str) -> str:
    path = MAIN / name
    if not path.exists():
        raise RuntimeError(f"missing required file: {path}")
    return path.read_text()


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"expected one {label}, found {count}")
    return text.replace(old, new, 1)


shell_h = read("ui_shell.h")
shell_c = read("ui_shell.c")
main = read("main.c")


if "ui_shell_set_printer_switch_callback" in shell_h:
    print("PASS: top-bar printer switch routing already installed")
    raise SystemExit(0)


shell_h += r'''

/*
 * The shell owns the persistent top-bar control. Application routing owns
 * what opening the printer chooser does.
 */
typedef void (*ui_shell_printer_switch_cb_t)(void);

void ui_shell_set_printer_switch_callback(
    ui_shell_printer_switch_cb_t callback);
'''


if '#include "ui_button.h"' not in shell_c:
    shell_c = replace_once(
        shell_c,
        '#include "ui_shell.h"\n',
        '#include "ui_shell.h"\n#include "ui_button.h"\n',
        "shell header include",
    )


shell_c = replace_once(
    shell_c,
    '''static lv_obj_t *s_shell_title_label = NULL;
static lv_obj_t *shell_clock_label = NULL;
''',
    '''static lv_obj_t *s_shell_printer_button = NULL;
static lv_obj_t *s_shell_title_label = NULL;
static ui_shell_printer_switch_cb_t
    s_printer_switch_callback = NULL;
static lv_obj_t *shell_clock_label = NULL;
''',
    "shell title globals",
)


event_anchor = '''static void shell_clock_timer_cb(lv_timer_t *timer)
'''

event_handler = r'''static void shell_printer_switch_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;

    if (s_printer_switch_callback) {
        s_printer_switch_callback();
    }
}


'''

shell_c = replace_once(
    shell_c,
    event_anchor,
    event_handler + event_anchor,
    "shell clock callback anchor",
)


title_old = '''    s_shell_title_label =
     lv_label_create(shell_top_bar);

 lv_obj_t *title_label =
     s_shell_title_label;
    lv_label_set_text(title_label, "Printer HMI v3.2  |  ESP32-P4 + C6 WiFi");
    ui_apply_text_title(title_label);
    ui_apply_label_bright(title_label);
    lv_obj_align(title_label, LV_ALIGN_LEFT_MID, 18, 0);
'''

title_new = '''    s_shell_printer_button =
        ui_button_create_empty(
            shell_top_bar,
            UI_BUTTON_OUTLINED);

    if (s_shell_printer_button) {
        lv_obj_set_size(s_shell_printer_button, 500, 52);
        lv_obj_set_pos(s_shell_printer_button, 12, 10);
        lv_obj_clear_flag(
            s_shell_printer_button,
            LV_OBJ_FLAG_SCROLLABLE);

        s_shell_title_label =
            ui_button_create_label(
                s_shell_printer_button,
                "PRINTERHMI  |  SELECT PRINTER  " LV_SYMBOL_DOWN);

        lv_obj_add_event_cb(
            s_shell_printer_button,
            shell_printer_switch_event_cb,
            LV_EVENT_CLICKED,
            NULL);
    }
'''

shell_c = replace_once(
    shell_c,
    title_old,
    title_new,
    "top-bar title construction",
)


name_old = '''    if (printer_name && printer_name[0]) {
        lv_snprintf(
            title,
            sizeof(title),
            "PRINTERHMI  |  %s",
            printer_name);
    } else {
        lv_snprintf(title, sizeof(title), "PRINTERHMI");
    }
'''

name_new = '''    if (printer_name && printer_name[0]) {
        lv_snprintf(
            title,
            sizeof(title),
            "PRINTERHMI  |  %s  %s",
            printer_name,
            LV_SYMBOL_DOWN);
    } else {
        lv_snprintf(
            title,
            sizeof(title),
            "PRINTERHMI  |  SELECT PRINTER  %s",
            LV_SYMBOL_DOWN);
    }
'''

shell_c = replace_once(
    shell_c,
    name_old,
    name_new,
    "active printer button text",
)


shell_c += r'''


void ui_shell_set_printer_switch_callback(
    ui_shell_printer_switch_cb_t callback)
{
    s_printer_switch_callback = callback;
}
'''


manage_anchor = '''static void printer_chooser_manage_bridge(lv_event_t *event)
{
    (void)event;

    ui_printer_profiles_show(
        printer_profiles_active_changed_bridge);
}
'''

open_bridge = manage_anchor + r'''


static void printer_chooser_open_from_topbar(void)
{
    ui_telemetry_v32_hide();
    ui_files_v32_hide();
    ui_printer_v32_hide();
    ui_drybox_v32_hide();
    ui_network_v32_hide();
    hide_settings_tab();

    ui_printer_chooser_v32_show(
        printer_chooser_select_bridge,
        printer_chooser_manage_bridge);

    /* The chooser is a destination, not the Dashboard nav selection. */
    ui_shell_set_active_nav(-1);
}
'''

main = replace_once(
    main,
    manage_anchor,
    open_bridge,
    "top-bar chooser routing bridge",
)


routing_old = '''void ui_shell_page_action(ui_shell_page_t page)
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

routing_new = '''void ui_shell_page_action(ui_shell_page_t page)
{
    /* Every sidebar destination closes the explicit printer chooser. */
    ui_printer_chooser_v32_hide();

    switch (page) {
    case UI_SHELL_PAGE_DASHBOARD:
        ui_telemetry_v32_hide();
        ui_files_v32_hide();
        ui_printer_v32_hide();
        ui_drybox_v32_hide();
        ui_network_v32_hide();
        hide_settings_tab();

        ui_dashboard_v32_create();
        dashboard_restore_active_profile_preview();
        return;
'''

main = replace_once(
    main,
    routing_old,
    routing_new,
    "Dashboard sidebar routing",
)


build_chooser = '''    ui_printer_chooser_v32_show(
        printer_chooser_select_bridge,
        printer_chooser_manage_bridge);


'''

main = replace_once(
    main,
    build_chooser,
    '',
    "startup build chooser",
)


startup_chooser = '''
    /* STARTUP_CHOOSER_FOREGROUND */
    ui_printer_chooser_v32_show(
        printer_chooser_select_bridge,
        printer_chooser_manage_bridge);
'''

main = replace_once(
    main,
    startup_chooser,
    '',
    "startup foreground chooser",
)


main = replace_once(
    main,
    '''    /* Top bar now belongs to ui_shell. */
    ui_shell_create();
''',
    '''    /* Top bar now belongs to ui_shell. */
    ui_shell_create();
    ui_shell_set_printer_switch_callback(
        printer_chooser_open_from_topbar);
''',
    "top-bar chooser callback registration",
)


(MAIN / "ui_shell.h").write_text(shell_h)
(MAIN / "ui_shell.c").write_text(shell_c)
(MAIN / "main.c").write_text(main)

print("PASS: printer chooser moved to top-bar printer button")
print("  - Theme B outlined control owns the persistent printer name")
print("  - top-bar printer name opens the chooser")
print("  - Dashboard sidebar opens the active Dashboard")
print("  - startup opens the active Dashboard, not the chooser")
print("  - chooser selection still switches profile and returns Dashboard")
print("Next: idf.py build")

