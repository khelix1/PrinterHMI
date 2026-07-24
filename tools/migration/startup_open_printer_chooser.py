#!/usr/bin/env python3
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "main" / "main.c"


if not SOURCE.exists():
    raise RuntimeError(f"missing required file: {SOURCE}")

text = SOURCE.read_text()
marker = "STARTUP_OPEN_PRINTER_CHOOSER"

if marker in text:
    print("PASS: startup already opens the Printers chooser")
    raise SystemExit(0)

function_anchor = "static void app_startup_show_initial_ui(void)"
start = text.find(function_anchor)
if start < 0:
    raise RuntimeError("missing app_startup_show_initial_ui")

next_function = text.find("\nvoid app_main(void)", start)
if next_function < 0:
    raise RuntimeError("missing app_main anchor after startup UI function")

startup = text[start:next_function]

if startup.count("ui_dashboard_v32_create();") != 1:
    raise RuntimeError(
        "expected one Dashboard creation in startup function, found "
        f"{startup.count('ui_dashboard_v32_create();')}"
    )

if "ui_printer_chooser_v32_show(" in startup:
    raise RuntimeError(
        "startup already contains an unmarked printer chooser call; audit it "
        "before applying this migration"
    )

anchor = "    ui_dashboard_v32_create();\n"
replacement = '''    ui_dashboard_v32_create();

    /* STARTUP_OPEN_PRINTER_CHOOSER
     * Keep the active Dashboard built behind the startup splash, then place
     * the multi-printer chooser in front. Selecting a printer continues into
     * that profile's Dashboard through printer_chooser_select_bridge().
     */
    ui_printer_chooser_v32_show(
        printer_chooser_select_bridge,
        printer_chooser_manage_bridge);
'''

startup = startup.replace(anchor, replacement, 1)
text = text[:start] + startup + text[next_function:]
SOURCE.write_text(text)

print("PASS: startup destination changed to Printers chooser")
print("  - active Dashboard remains initialized behind the chooser")
print("  - printer selection still opens the selected Dashboard")
print("  - Dashboard sidebar routing is unchanged")
print("  - top-bar printer-name routing is unchanged")
print("Next: idf.py build")

