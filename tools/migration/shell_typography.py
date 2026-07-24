#!/usr/bin/env python3

from pathlib import Path

path = Path("main/ui_shell.c")

if not path.is_file():
    raise RuntimeError(f"missing file: {path}")

text = path.read_text()

replacements = [
    (
        """    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(title_label, UI_TEXT_BRIGHT, 0);""",
        """    ui_apply_text_title(title_label);
    ui_apply_label_bright(title_label);""",
    ),
    (
        """    lv_obj_set_style_text_color(shell_clock_label, UI_TEXT, 0);
    lv_obj_set_style_text_font(shell_clock_label, &lv_font_montserrat_22, 0);""",
        """    ui_apply_text_title(shell_clock_label);
    ui_apply_label_primary(shell_clock_label);""",
    ),
]

for old, new in replacements:
    found = text.count(old)

    if found != 1:
        raise RuntimeError(
            "expected one Shell typography block:\n"
            f"{old}\nfound {found}"
        )

    text = text.replace(old, new, 1)

path.write_text(text)

updated = path.read_text()

for obsolete in (
    "lv_obj_set_style_text_font(",
    "lv_obj_set_style_text_color(",
):
    if obsolete in updated:
        raise RuntimeError(
            f"obsolete direct Shell typography remains: {obsolete}"
        )

print("PASS: Shell typography uses semantic theme helpers")
print("Changed:")
print("  main/ui_shell.c")
