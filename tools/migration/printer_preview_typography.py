#!/usr/bin/env python3

from pathlib import Path

path = Path("main/ui_printer_v32.c")

if not path.is_file():
    raise RuntimeError(f"missing file: {path}")

text = path.read_text()

old = """        lv_obj_set_style_text_font(
            s_preview_label,
            &lv_font_montserrat_18,
            0);

        lv_obj_set_style_text_color(
            s_preview_label,
            UI_TEXT_DIM,
            0);"""

new = """        ui_apply_text_body_large(
            s_preview_label);
        ui_apply_label_dim(
            s_preview_label);"""

found = text.count(old)

if found != 1:
    raise RuntimeError(
        f"expected one Printer preview typography block, found {found}"
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
            f"obsolete Printer preview typography remains: {obsolete}"
        )

print("PASS: Printer preview typography uses theme helpers")
print("Changed:")
print("  main/ui_printer_v32.c")
