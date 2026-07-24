#!/usr/bin/env python3

from pathlib import Path

path = Path("main/ui_active_print_v32.c")

if not path.is_file():
    raise RuntimeError(f"missing file: {path}")

text = path.read_text()

replacements = [
    (
        """    lv_obj_set_style_text_font(
        ctx->filename,
        UI_FONT_BODY,
        0);

    lv_obj_set_style_text_color(
        ctx->filename,
        UI_TEXT_BRIGHT,
        0);""",
        """    ui_apply_text_body(
        ctx->filename);
    ui_apply_label_bright(
        ctx->filename);""",
    ),
    (
        """    lv_obj_set_style_text_font(
        ctx->preview_label,
        UI_FONT_BODY_LARGE,
        0);

    lv_obj_set_style_text_color(
        ctx->preview_label,
        UI_TEXT_DIM,
        0);""",
        """    ui_apply_text_body_large(
        ctx->preview_label);
    ui_apply_label_dim(
        ctx->preview_label);""",
    ),
    (
        """    lv_obj_set_style_text_font(
        ctx->footer,
        UI_FONT_CAPTION,
        0);""",
        """    ui_apply_text_caption(
        ctx->footer);""",
    ),
]

for old, new in replacements:
    found = text.count(old)

    if found != 1:
        raise RuntimeError(
            "expected one Active Print typography block:\n"
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
        f"expected one cyan footer color, found {remaining_colors}"
    )

print("PASS: Active Print typography uses semantic theme helpers")
print("Preserved: cyan status-strip accent")
print("Changed:")
print("  main/ui_active_print_v32.c")
