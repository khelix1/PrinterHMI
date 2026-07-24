#!/usr/bin/env python3

from pathlib import Path
import re

main_path = Path("main/main.c")
controller_path = Path("main/printer_controller.c")
header_path = Path("main/printer_controller.h")

for path in (main_path, controller_path, header_path):
    if not path.is_file():
        raise RuntimeError(f"missing file: {path}")

main_text = main_path.read_text()

if main_text.count("dash_banner_label") != 5:
    raise RuntimeError(
        "expected exactly 5 dash_banner_label references, found "
        f"{main_text.count('dash_banner_label')}"
    )

declaration = "static lv_obj_t *dash_banner_label = NULL;\n"

label_block = """    if (dash_banner_label) {
        lv_label_set_text(dash_banner_label, printer_banner_text());
    }

"""

color_block = """    if (dash_banner_label) {
        lv_obj_set_style_bg_color(
            lv_obj_get_parent(dash_banner_label),
            printer_controller_dashboard_banner_color(
                mr_state->printer_state,
                s_got_ip,
                s_moonraker_ok),
            0);
    }
"""

for description, old in (
    ("dead banner declaration", declaration),
    ("dead banner text block", label_block),
    ("dead banner color block", color_block),
):
    found = main_text.count(old)

    if found != 1:
        raise RuntimeError(
            f"expected one {description}, found {found}"
        )

    main_text = main_text.replace(old, "", 1)

if "dash_banner_label" in main_text:
    raise RuntimeError("dash_banner_label references remain in main.c")

main_path.write_text(main_text)

header_text = header_path.read_text()

prototype_pattern = re.compile(
    r"\nlv_color_t printer_controller_dashboard_banner_color"
    r"\(const char \*state,\s*"
    r"bool got_ip,\s*"
    r"bool moonraker_ok\);\n",
    re.MULTILINE,
)

header_text, count = prototype_pattern.subn("\n", header_text)

if count != 1:
    raise RuntimeError(
        f"expected one controller prototype, removed {count}"
    )

header_path.write_text(header_text)

controller_text = controller_path.read_text()

definition_pattern = re.compile(
    r"\nlv_color_t printer_controller_dashboard_banner_color"
    r"\(const char \*state,\s*"
    r"bool got_ip,\s*"
    r"bool moonraker_ok\)\s*"
    r"\{\s*"
    r"if \(!got_ip \|\| !moonraker_ok\) return UI_ACCENT;\s*"
    r"if \(printer_controller_is_paused\(state\)\) "
    r"return lv_color_hex\(0xa66a00\);\s*"
    r"if \(printer_controller_is_error\(state\)\) "
    r"return lv_color_hex\(0x9f1d1d\);\s*"
    r"return UI_OK;\s*"
    r"\}\n",
    re.MULTILINE,
)

controller_text, count = definition_pattern.subn(
    "\n",
    controller_text,
)

if count != 1:
    raise RuntimeError(
        f"expected one controller definition, removed {count}"
    )

controller_path.write_text(controller_text)

for path in (main_path, controller_path, header_path):
    text = path.read_text()

    if "printer_controller_dashboard_banner_color" in text:
        raise RuntimeError(
            f"{path}: obsolete controller function remains"
        )

print("PASS: dead Dashboard banner override removed")
print("Theme B status-banner ownership is now authoritative")
print("Changed:")
print("  main/main.c")
print("  main/printer_controller.c")
print("  main/printer_controller.h")
