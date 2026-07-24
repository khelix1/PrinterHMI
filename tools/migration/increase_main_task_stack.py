#!/usr/bin/env python3
from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[2]
SDKCONFIG = ROOT / "sdkconfig"
DEFAULTS = ROOT / "sdkconfig.defaults"

SETTING = "CONFIG_ESP_MAIN_TASK_STACK_SIZE"
STACK_SIZE = 8192
MARKER = "# PrinterHMI application startup requires an 8 KB main-task stack."


if not SDKCONFIG.exists():
    raise RuntimeError(f"missing required file: {SDKCONFIG}")


def set_config_value(text: str, *, add_marker: bool) -> str:
    pattern = re.compile(
        rf"(?m)^(?:# )?{re.escape(SETTING)}(?:=| is not set).*$"
    )
    matches = pattern.findall(text)

    if len(matches) > 1:
        raise RuntimeError(
            f"expected at most one {SETTING} entry, found {len(matches)}"
        )

    replacement = f"{SETTING}={STACK_SIZE}"

    if matches:
        text = pattern.sub(replacement, text, count=1)
    else:
        if text and not text.endswith("\n"):
            text += "\n"
        text += replacement + "\n"

    if add_marker and MARKER not in text:
        text = text.replace(replacement, MARKER + "\n" + replacement, 1)

    return text


sdkconfig = set_config_value(
    SDKCONFIG.read_text(),
    add_marker=False,
)

defaults = DEFAULTS.read_text() if DEFAULTS.exists() else ""
defaults = set_config_value(defaults, add_marker=True)


expected = f"{SETTING}={STACK_SIZE}"

if sdkconfig.count(expected) != 1:
    raise RuntimeError("active sdkconfig stack verification failed")

if defaults.count(expected) != 1:
    raise RuntimeError("sdkconfig.defaults stack verification failed")


SDKCONFIG.write_text(sdkconfig)
DEFAULTS.write_text(defaults)

print("PASS: PrinterHMI main-task stack increased")
print(f"  - active sdkconfig: {STACK_SIZE} bytes")
print(f"  - persistent sdkconfig.defaults: {STACK_SIZE} bytes")
print("  - OTA rollback validation remains unchanged")
print("Next: idf.py fullclean && idf.py build")

