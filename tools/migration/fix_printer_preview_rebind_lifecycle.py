#!/usr/bin/env python3
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "main" / "ui_printer_v32.c"


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"expected one {label}, found {count}")
    return text.replace(old, new, 1)


if not SOURCE.exists():
    raise RuntimeError(f"missing required file: {SOURCE}")

text = SOURCE.read_text()

fixed = '''    s_preview_canvas_file[0] = '\\0';
    s_preview_cache_revision = 0;
    s_preview_cache_profile_index = -1;
}
'''

destroy_start = text.find("void ui_printer_v32_preview_destroy_refs(void)")
if destroy_start < 0:
    raise RuntimeError("missing Printer preview destroy_refs function")

destroy_end = text.find("\n}\n", destroy_start)
if destroy_end < 0:
    raise RuntimeError("could not locate end of destroy_refs function")

destroy_block = text[destroy_start:destroy_end + 3]

if ("s_preview_cache_revision = 0;" in destroy_block and
        "s_preview_cache_profile_index = -1;" in destroy_block):
    print("PASS: Printer preview lifecycle identity already resets")
    raise SystemExit(0)

old_tail = '''    s_preview_canvas = NULL;
    s_preview_canvas_file[0] = '\\0';
}
'''

new_tail = '''    s_preview_canvas = NULL;
    s_preview_canvas_file[0] = '\\0';
    s_preview_cache_revision = 0;
    s_preview_cache_profile_index = -1;
}
'''

updated_block = replace_once(
    destroy_block,
    old_tail,
    new_tail,
    "destroy_refs identity tail",
)

text = (
    text[:destroy_start] +
    updated_block +
    text[destroy_end + 3:]
)

SOURCE.write_text(text)

print("PASS: Printer preview rebind lifecycle fixed")
print("  - PSRAM/profile cache remains untouched")
print("  - deleted LVGL image identity is cleared")
print("  - returning to Printer always reapplies lv_image_set_src")
print("Next: idf.py build")

