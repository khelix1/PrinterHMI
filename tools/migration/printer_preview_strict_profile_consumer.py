#!/usr/bin/env python3
from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "main" / "ui_printer_v32.c"


if not SOURCE.exists():
    raise RuntimeError(f"missing required file: {SOURCE}")

text = SOURCE.read_text()

marker = "PRINTER_PREVIEW_STRICT_PROFILE_CONSUMER"
if marker in text:
    print("PASS: Printer preview is already a strict profile-cache consumer")
    raise SystemExit(0)

function_anchor = "void ui_printer_v32_preview_show("
function_start = text.find(function_anchor)
if function_start < 0:
    raise RuntimeError("missing ui_printer_v32_preview_show")

fallback_start_text = "    if (!preview_file || !preview_file[0]) {"
fallback_start = text.find(fallback_start_text, function_start)
if fallback_start < 0:
    raise RuntimeError("missing legacy Printer global-thumbnail fallback start")

function_end_anchor = "\n}\n\nvoid ui_printer_v32_preview_reset"
fallback_end = text.find(function_end_anchor, fallback_start)
if fallback_end < 0:
    raise RuntimeError("missing ui_printer_v32_preview_show end anchor")

legacy = text[fallback_start:fallback_end]

required_legacy_tokens = (
    "thumbnail_manager_v32_has_png()",
    "thumbnail_manager_v32_image_dsc()",
    "thumbnail_render_v32_to_rgb565(",
    "lv_canvas_set_buffer(",
)

missing = [token for token in required_legacy_tokens if token not in legacy]
if missing:
    raise RuntimeError(
        "legacy fallback shape changed; missing: " + ", ".join(missing)
    )

replacement = '''    /* PRINTER_PREVIEW_STRICT_PROFILE_CONSUMER
     * A Printer page may display only the image owned by its active profile.
     * The process-wide thumbnail manager is a transient decode pipeline and
     * must never be used as a UI fallback after a profile cache miss or file
     * identity mismatch.
     */
    preview_show_placeholder();
    return;
'''

text = text[:fallback_start] + replacement + text[fallback_end:]

# The removed fallback was the sole owner of this legacy render buffer.
buffer_pattern = re.compile(
    r"^static uint16_t \*s_preview_canvas_buffer = NULL;\n",
    re.MULTILINE,
)
text, buffer_removals = buffer_pattern.subn("", text)
if buffer_removals > 1:
    raise RuntimeError(
        f"unexpected preview canvas buffer declaration count: {buffer_removals}"
    )

# Remove headers only when their APIs are no longer used by this module.
optional_headers = {
    "thumbnail_manager_v32.h": "thumbnail_manager_v32_",
    "thumbnail_render_v32.h": "thumbnail_render_v32_",
    "esp_heap_caps.h": "heap_caps_",
}

for header, symbol_prefix in optional_headers.items():
    include = f'#include "{header}"\n'
    if include in text and symbol_prefix not in text.replace(include, ""):
        text = text.replace(include, "", 1)

SOURCE.write_text(text)

print("PASS: Printer preview leakage path removed")
print("  - Printer renders only the active profile's validated cache image")
print("  - cache miss or filename mismatch now shows the placeholder")
print("  - process-wide thumbnail manager remains a producer, not a UI fallback")
print("Next: idf.py build")

