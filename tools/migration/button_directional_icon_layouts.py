#!/usr/bin/env python3

from pathlib import Path

header_path = Path("main/ui_button.h")
source_path = Path("main/ui_button.c")
motion_path = Path("main/ui_printer_motion.c")

for path in (header_path, source_path, motion_path):
    if not path.is_file():
        raise RuntimeError(f"missing file: {path}")

header = header_path.read_text()

old_enum = """typedef enum {
    UI_BUTTON_ICON_HORIZONTAL = 0,
    UI_BUTTON_ICON_VERTICAL
} ui_button_icon_layout_t;"""

new_enum = """typedef enum {
    UI_BUTTON_ICON_HORIZONTAL = 0,
    UI_BUTTON_ICON_VERTICAL,
    UI_BUTTON_ICON_HORIZONTAL_REVERSE,
    UI_BUTTON_ICON_VERTICAL_REVERSE
} ui_button_icon_layout_t;"""

if header.count(old_enum) != 1:
    raise RuntimeError(
        f"expected one icon-layout enum, "
        f"found {header.count(old_enum)}"
    )

header = header.replace(
    old_enum,
    new_enum,
    1,
)

header_path.write_text(header)

source = source_path.read_text()

old_layout = """    if (layout == UI_BUTTON_ICON_VERTICAL) {
        ui_apply_text_title(icon);

        lv_obj_align(
            icon,
            LV_ALIGN_TOP_MID,
            0,
            10);

        lv_obj_align(
            label,
            LV_ALIGN_BOTTOM_MID,
            0,
            -10);
    } else {
        ui_apply_text_button(icon);

        lv_obj_set_flex_flow(
            button,
            LV_FLEX_FLOW_ROW);

        lv_obj_set_flex_align(
            button,
            LV_FLEX_ALIGN_CENTER,
            LV_FLEX_ALIGN_CENTER,
            LV_FLEX_ALIGN_CENTER);

        lv_obj_set_style_pad_left(
            button,
            7,
            0);

        lv_obj_set_style_pad_right(
            button,
            7,
            0);

        lv_obj_set_style_pad_top(
            button,
            0,
            0);

        lv_obj_set_style_pad_bottom(
            button,
            0,
            0);

        lv_obj_set_style_pad_column(
            button,
            6,
            0);
    }"""

new_layout = """    if (layout == UI_BUTTON_ICON_VERTICAL ||
        layout == UI_BUTTON_ICON_VERTICAL_REVERSE) {
        ui_apply_text_title(icon);

        if (layout == UI_BUTTON_ICON_VERTICAL_REVERSE) {
            lv_obj_align(
                label,
                LV_ALIGN_TOP_MID,
                0,
                10);

            lv_obj_align(
                icon,
                LV_ALIGN_BOTTOM_MID,
                0,
                -10);
        } else {
            lv_obj_align(
                icon,
                LV_ALIGN_TOP_MID,
                0,
                10);

            lv_obj_align(
                label,
                LV_ALIGN_BOTTOM_MID,
                0,
                -10);
        }
    } else {
        ui_apply_text_button(icon);

        lv_obj_set_flex_flow(
            button,
            layout == UI_BUTTON_ICON_HORIZONTAL_REVERSE
                ? LV_FLEX_FLOW_ROW_REVERSE
                : LV_FLEX_FLOW_ROW);

        lv_obj_set_flex_align(
            button,
            LV_FLEX_ALIGN_CENTER,
            LV_FLEX_ALIGN_CENTER,
            LV_FLEX_ALIGN_CENTER);

        lv_obj_set_style_pad_left(
            button,
            7,
            0);

        lv_obj_set_style_pad_right(
            button,
            7,
            0);

        lv_obj_set_style_pad_top(
            button,
            0,
            0);

        lv_obj_set_style_pad_bottom(
            button,
            0,
            0);

        lv_obj_set_style_pad_column(
            button,
            6,
            0);
    }"""

if source.count(old_layout) != 1:
    raise RuntimeError(
        f"expected one shared icon-layout block, "
        f"found {source.count(old_layout)}"
    )

source = source.replace(
    old_layout,
    new_layout,
    1,
)

source_path.write_text(source)

motion = motion_path.read_text()

motion_replacements = [
    (
        """        LV_SYMBOL_UP,
        "Y+",
        UI_ACCENT_CYAN,
        UI_BUTTON_ICON_HORIZONTAL,""",
        """        LV_SYMBOL_UP,
        "Y+",
        UI_ACCENT_CYAN,
        UI_BUTTON_ICON_VERTICAL,""",
    ),
    (
        """        LV_SYMBOL_RIGHT,
        "X+",
        UI_ACCENT_CYAN,
        UI_BUTTON_ICON_HORIZONTAL,""",
        """        LV_SYMBOL_RIGHT,
        "X+",
        UI_ACCENT_CYAN,
        UI_BUTTON_ICON_HORIZONTAL_REVERSE,""",
    ),
    (
        """        LV_SYMBOL_DOWN,
        "Y-",
        UI_ACCENT_CYAN,
        UI_BUTTON_ICON_HORIZONTAL,""",
        """        LV_SYMBOL_DOWN,
        "Y-",
        UI_ACCENT_CYAN,
        UI_BUTTON_ICON_VERTICAL_REVERSE,""",
    ),
]

for old, new in motion_replacements:
    found = motion.count(old)

    if found != 1:
        raise RuntimeError(
            "expected one directional Motion layout:\n"
            f"{old}\nfound {found}"
        )

    motion = motion.replace(old, new, 1)

motion_path.write_text(motion)

updated_header = header_path.read_text()
updated_source = source_path.read_text()
updated_motion = motion_path.read_text()

for token in (
    "UI_BUTTON_ICON_HORIZONTAL_REVERSE",
    "UI_BUTTON_ICON_VERTICAL_REVERSE",
):
    if token not in updated_header:
        raise RuntimeError(
            f"missing shared layout enum: {token}"
        )

    if token not in updated_source:
        raise RuntimeError(
            f"missing shared layout implementation: {token}"
        )

if updated_motion.count(
    "UI_BUTTON_ICON_HORIZONTAL_REVERSE"
) != 1:
    raise RuntimeError("X+ reverse layout missing")

if updated_motion.count(
    "UI_BUTTON_ICON_VERTICAL_REVERSE"
) != 1:
    raise RuntimeError("Y- reverse layout missing")

print("PASS: directional icon layouts added")
print("X- left, X+ right, Y+ above, Y- below")
print("Changed:")
print("  main/ui_button.h")
print("  main/ui_button.c")
print("  main/ui_printer_motion.c")
