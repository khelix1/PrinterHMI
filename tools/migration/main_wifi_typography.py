#!/usr/bin/env python3

from pathlib import Path

path = Path("main/main.c")

if not path.is_file():
    raise RuntimeError(f"missing file: {path}")

text = path.read_text()

old = """    lv_obj_set_style_text_font(wifi_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(wifi_label, UI_TEXT_DIM, 0);"""

new = """    ui_apply_text_body(wifi_label);
    ui_apply_label_dim(wifi_label);"""

found = text.count(old)

if found != 1:
    raise RuntimeError(
        f"expected one Wi-Fi typography pair, found {found}"
    )

text = text.replace(old, new, 1)
path.write_text(text)

updated = path.read_text()

if "lv_obj_set_style_text_font(" in updated:
    raise RuntimeError(
        "unexpected direct font assignment remains in main.c"
    )

remaining_colors = updated.count(
    "lv_obj_set_style_text_color("
)

if remaining_colors != 3:
    raise RuntimeError(
        f"expected 3 root/runtime color assignments, "
        f"found {remaining_colors}"
    )

print("PASS: main.c Wi-Fi typography uses theme helpers")
print("Preserved:")
print("  dynamic nozzle color")
print("  dynamic bed color")
print("  inherited screen text color")
print("Changed:")
print("  main/main.c")
