#!/usr/bin/env python3

from pathlib import Path

path = Path("main/ui_splash_v32.c")

if not path.is_file():
    raise RuntimeError(f"missing file: {path}")

text = path.read_text()

replacements = [
    (
        """    lv_obj_set_style_text_font(title, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(title, UI_TEXT, 0);""",
        """    ui_apply_text_splash_title(title);
    ui_apply_label_primary(title);""",
    ),
    (
        """    lv_obj_set_style_text_font(sub, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(sub, UI_TEXT_DIM, 0);""",
        """    ui_apply_text_body_large(sub);
    ui_apply_label_dim(sub);""",
    ),
    (
        """    lv_obj_set_style_text_font(splash_status, &lv_font_montserrat_22, 0);""",
        """    ui_apply_text_title(splash_status);""",
    ),
    (
        """    lv_obj_set_style_text_font(splash_percent, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(splash_percent, UI_TEXT, 0);""",
        """    ui_apply_text_body_large(splash_percent);
    ui_apply_label_primary(splash_percent);""",
    ),
    (
        """    lv_obj_set_style_text_font(footer, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(footer, UI_TEXT_DIM, 0);""",
        """    ui_apply_text_body(footer);
    ui_apply_label_dim(footer);""",
    ),
]

for old, new in replacements:
    found = text.count(old)

    if found != 1:
        raise RuntimeError(
            "expected one Splash typography assignment:\n"
            f"{old}\nfound {found}"
        )

    text = text.replace(old, new, 1)

path.write_text(text)

updated = path.read_text()

remaining_fonts = updated.count("lv_obj_set_style_text_font(")
remaining_colors = updated.count("lv_obj_set_style_text_color(")

if remaining_fonts != 0:
    raise RuntimeError(
        f"expected zero direct font assignments, found {remaining_fonts}"
    )

if remaining_colors != 1:
    raise RuntimeError(
        f"expected one Splash status accent color, found {remaining_colors}"
    )

print("PASS: Splash typography uses semantic theme helpers")
print("Preserved: cyan Splash status accent")
print("Changed:")
print("  main/ui_splash_v32.c")
