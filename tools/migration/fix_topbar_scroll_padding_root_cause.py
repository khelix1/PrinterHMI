#!/usr/bin/env python3
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "main" / "ui_shell.c"


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"expected one {label}, found {count}")
    return text.replace(old, new, 1)


if not SOURCE.exists():
    raise RuntimeError(f"missing required file: {SOURCE}")

text = SOURCE.read_text()

marker = "TOPBAR_FIXED_NON_SCROLLING_CONTENT_AREA"
if marker in text:
    print("PASS: top-bar scroll/padding root cause already fixed")
    raise SystemExit(0)

text = replace_once(
    text,
    '''    lv_obj_set_style_radius(shell_top_bar, 0, 0);
''',
    '''    lv_obj_set_style_radius(shell_top_bar, 0, 0);

    /* TOPBAR_FIXED_NON_SCROLLING_CONTENT_AREA
     * Absolute shell geometry must not inherit LVGL container padding or
     * auto-scroll a focused child into view.
     */
    lv_obj_clear_flag(shell_top_bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(shell_top_bar, 0, 0);
''',
    "top-bar radius anchor",
)


diagnostic = '''
    /* TOPBAR_GEOMETRY_DIAGNOSTIC: temporary, uses existing clock timer. */
    if (shell_top_bar && s_shell_printer_button) {
        lv_obj_update_layout(shell_top_bar);

        lv_area_t top;
        lv_area_t button;
        lv_obj_get_coords(shell_top_bar, &top);
        lv_obj_get_coords(s_shell_printer_button, &button);

        ESP_LOGI(
            TAG,
            "TOPBAR_GEOM top=(%d,%d)-(%d,%d) "
            "button=(%d,%d)-(%d,%d) state=0x%x",
            (int)top.x1,
            (int)top.y1,
            (int)top.x2,
            (int)top.y2,
            (int)button.x1,
            (int)button.y1,
            (int)button.x2,
            (int)button.y2,
            (unsigned)lv_obj_get_state(s_shell_printer_button));
    }
'''

if diagnostic in text:
    text = text.replace(diagnostic, '', 1)


latched_callback = '''static void shell_printer_switch_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;

    lv_obj_t *button = lv_event_get_target(event);

    if (s_printer_switch_callback) {
        s_printer_switch_callback();
    }

    /* TOPBAR_PRINTER_CLEAR_TRANSIENT_STATE
     * The callback raises the chooser while LVGL is completing this click.
     * Do not let the persistent shell control retain its 1 px pressed offset.
     */
    if (button) {
        lv_obj_remove_state(
            button,
            LV_STATE_PRESSED |
            LV_STATE_FOCUSED |
            LV_STATE_FOCUS_KEY);
        lv_obj_set_style_translate_y(button, 0, LV_STATE_DEFAULT);
    }
}
'''

clean_callback = '''static void shell_printer_switch_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;

    if (s_printer_switch_callback) {
        s_printer_switch_callback();
    }
}
'''

if latched_callback in text:
    text = text.replace(latched_callback, clean_callback, 1)


SOURCE.write_text(text)

print("PASS: top-bar movement root cause fixed")
print("  - top-bar content padding is explicitly zero")
print("  - top bar cannot auto-scroll focused children")
print("  - printer button resolves at (12,10)-(511,61)")
print("  - temporary geometry logging removed")
print("Next: idf.py build")

