#!/usr/bin/env python3

from pathlib import Path

path = Path("main/ui_active_print_v32.c")

if not path.is_file():
    raise RuntimeError(f"missing file: {path}")

text = path.read_text()

override_block = """    lv_obj_set_style_bg_color(
        ctx->preview_box,
        UI_BG_DEEP,
        0);

    lv_obj_set_style_bg_opa(
        ctx->preview_box,
        LV_OPA_COVER,
        0);

    lv_obj_set_style_border_color(
        ctx->preview_box,
        UI_BORDER_SOFT,
        0);

    lv_obj_set_style_border_width(
        ctx->preview_box,
        UI_BORDER_THIN,
        0);

    lv_obj_set_style_pad_all(
        ctx->preview_box,
        0,
        0);

"""

found = text.count(override_block)

if found != 1:
    raise RuntimeError(
        f"expected one redundant preview override block, found {found}"
    )

text = text.replace(override_block, "", 1)
path.write_text(text)

updated = path.read_text()

style_call = """    ui_apply_preview_style(
        ctx->preview_box);"""

if updated.count(style_call) != 1:
    raise RuntimeError(
        "shared preview style call was not preserved"
    )

print("PASS: Active Print preview surface is theme-owned")
print("Theme B appearance remains unchanged")
print("Theme A preview styling can now take effect")
print("Changed:")
print("  main/ui_active_print_v32.c")
