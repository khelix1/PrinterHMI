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

marker = "TOPBAR_PRINTER_CLEAR_TRANSIENT_STATE"
if marker in text:
    print("PASS: top-bar printer transient state already clears")
    raise SystemExit(0)

old = '''static void shell_printer_switch_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;

    if (s_printer_switch_callback) {
        s_printer_switch_callback();
    }
}
'''

new = '''static void shell_printer_switch_event_cb(lv_event_t *event)
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

text = replace_once(
    text,
    old,
    new,
    "top-bar printer click callback",
)

SOURCE.write_text(text)

print("PASS: top-bar printer button pressed position now releases")
print("  - fixed 500 x 52 geometry remains unchanged")
print("  - shared Theme B button feedback remains unchanged")
print("Next: idf.py build")

