#!/usr/bin/env python3
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "main" / "thumbnail_session_v32.c"


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"expected one {label}, found {count}")
    return text.replace(old, new, 1)


if not SOURCE.exists():
    raise RuntimeError(f"missing required file: {SOURCE}")

text = SOURCE.read_text()

if "EXT_RAM_BSS_ATTR static char s_metadata_body" in text:
    print("PASS: thumbnail metadata body already resides in PSRAM")
    raise SystemExit(0)

if '#include "esp_attr.h"' not in text:
    include_anchor = '#include "esp_log.h"\n'

    if include_anchor not in text:
        raise RuntimeError("could not locate ESP include anchor")

    text = text.replace(
        include_anchor,
        '#include "esp_attr.h"\n' + include_anchor,
        1,
    )

text = replace_once(
    text,
    'static char s_metadata_body[8192] = "";\n',
    'EXT_RAM_BSS_ATTR static char s_metadata_body[8192];\n',
    "thumbnail metadata body",
)

SOURCE.write_text(text)

print("PASS: 8 KiB thumbnail metadata body relocated to PSRAM")
print("  - no API, lifetime, or parsing behavior changed")
print("  - buffer remains fixed-size and zero-initialized")
print("  - FreeRTOS stack and DMA reserve remain unchanged")
print("Next: idf.py fullclean && idf.py build")
