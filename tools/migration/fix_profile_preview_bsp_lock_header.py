#!/usr/bin/env python3
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "main" / "printer_profile_preview_worker_v32.c"

if not SOURCE.exists():
    raise RuntimeError(f"missing required file: {SOURCE}")

text = SOURCE.read_text()

if '#include "bsp/esp-bsp.h"' in text:
    print("PASS: BSP lock declarations already installed")
    raise SystemExit(0)

anchor = '#include "bsp/display.h"\n'
count = text.count(anchor)

if count != 1:
    raise RuntimeError(
        f"expected one BSP display include, found {count}"
    )

text = text.replace(
    anchor,
    '#include "bsp/esp-bsp.h"\n#include "bsp/display.h"\n',
    1,
)

SOURCE.write_text(text)

print("PASS: profile preview worker now imports BSP lock declarations")
print("Next: idf.py build")
