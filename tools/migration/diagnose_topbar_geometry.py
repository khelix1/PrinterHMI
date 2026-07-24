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

marker = "TOPBAR_GEOMETRY_DIAGNOSTIC"
if marker in text:
    print("PASS: top-bar geometry diagnostic already installed")
    raise SystemExit(0)

old = '''    lv_label_set_text(shell_clock_label, buf);
}
'''

new = '''    lv_label_set_text(shell_clock_label, buf);

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
}
'''

text = replace_once(
    text,
    old,
    new,
    "shell clock callback tail",
)

SOURCE.write_text(text)

print("PASS: temporary top-bar geometry telemetry installed")
print("  - reuses existing one-second shell clock timer")
print("  - logs resolved top-bar/button coordinates and LVGL state")
print("Next: idf.py build")

