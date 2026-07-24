#!/usr/bin/env python3
from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[2]
PATH = ROOT / "main" / "main.c"

text = PATH.read_text()

match = re.search(
    r'(static void app_startup_show_initial_ui\(void\)\s*\{.*?\n\})',
    text,
    flags=re.S)

if not match:
    raise RuntimeError("could not locate app_startup_show_initial_ui")

function = match.group(1)

if "STARTUP_CHOOSER_FOREGROUND" in function:
    print("PASS: startup chooser foreground fix already installed")
    raise SystemExit(0)

dashboard_calls = list(re.finditer(
    r'(?m)^(?P<indent>[ \t]*)ui_dashboard_v32_create\(\);',
    function))

if len(dashboard_calls) != 1:
    raise RuntimeError(
        "expected one direct Dashboard creation in "
        "app_startup_show_initial_ui, found "
        f"{len(dashboard_calls)}")

call = dashboard_calls[0]
indent = call.group("indent")

replacement = (
    call.group(0) + "\n\n" +
    indent + "/* STARTUP_CHOOSER_FOREGROUND */\n" +
    indent + "ui_printer_chooser_v32_show(\n" +
    indent + "    printer_chooser_select_bridge,\n" +
    indent + "    printer_chooser_manage_bridge);")

function = (
    function[:call.start()] +
    replacement +
    function[call.end():])

updated = text[:match.start()] + function + text[match.end():]
PATH.write_text(updated)

print("PASS: chooser is now raised after the final startup Dashboard call")
print("Next: idf.py build")

