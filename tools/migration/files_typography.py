#!/usr/bin/env python3

from pathlib import Path

path = Path("main/ui_files_v32.c")

if not path.is_file():
    raise RuntimeError(f"missing file: {path}")

text = path.read_text()

replacements = [
    (
        """    lv_obj_set_style_text_font(
        label,
        &lv_font_montserrat_16,
        0);""",
        """    ui_apply_text_body(label);""",
    ),
    (
        """    lv_obj_set_style_text_color(
        label,
        UI_TEXT_BRIGHT,
        0);""",
        """    ui_apply_label_bright(label);""",
    ),
    (
        """    lv_obj_set_style_text_color(
        meta,
        UI_TEXT_DIM,
        0);""",
        """    ui_apply_label_dim(meta);""",
    ),
    (
        """    lv_obj_set_style_text_font(
        title,
        &lv_font_montserrat_22,
        0);""",
        """    ui_apply_text_title(title);""",
    ),
    (
        """    lv_obj_set_style_text_color(
        title,
        UI_TEXT_BRIGHT,
        0);""",
        """    ui_apply_label_bright(title);""",
    ),
    (
        """    lv_obj_set_style_text_font(
        subtitle,
        &lv_font_montserrat_14,
        0);""",
        """    ui_apply_text_caption(subtitle);""",
    ),
    (
        """    lv_obj_set_style_text_color(
        subtitle,
        UI_TEXT_DIM,
        0);""",
        """    ui_apply_label_dim(subtitle);""",
    ),
    (
        """    lv_obj_set_style_text_font(
        filename,
        &lv_font_montserrat_16,
        0);""",
        """    ui_apply_text_body(filename);""",
    ),
    (
        """    lv_obj_set_style_text_font(
        info,
        &lv_font_montserrat_16,
        0);""",
        """    ui_apply_text_body(info);""",
    ),
    (
        """    lv_obj_set_style_text_color(
        info,
        UI_TEXT,
        0);""",
        """    ui_apply_label_primary(info);""",
    ),
]

for old, new in replacements:
    found = text.count(old)

    if found != 1:
        raise RuntimeError(
            "expected one typography assignment:\n"
            f"{old}\nfound {found}"
        )

    text = text.replace(old, new, 1)

path.write_text(text)

updated = path.read_text()

remaining_fonts = updated.count("lv_obj_set_style_text_font(")
remaining_colors = updated.count("lv_obj_set_style_text_color(")

if remaining_fonts != 3:
    raise RuntimeError(
        f"expected 3 component-specific font assignments, "
        f"found {remaining_fonts}"
    )

if remaining_colors != 3:
    raise RuntimeError(
        f"expected 3 cyan accent assignments, "
        f"found {remaining_colors}"
    )

print("PASS: Files text roles use semantic theme helpers")
print("Preserved:")
print("  file icon sizing")
print("  compact 12 px metadata")
print("  navigation-arrow sizing")
print("  cyan feature accents")
print("Changed:")
print("  main/ui_files_v32.c")
