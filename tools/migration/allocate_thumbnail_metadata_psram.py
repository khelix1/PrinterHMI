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

if "ensure_metadata_body" in text:
    print("PASS: thumbnail metadata body already allocates lazily in PSRAM")
    raise SystemExit(0)

if '#include "esp_heap_caps.h"' not in text:
    text = replace_once(
        text,
        '#include "esp_err.h"\n',
        '#include "esp_err.h"\n#include "esp_heap_caps.h"\n',
        "ESP error include",
    )

declarations = (
    'EXT_RAM_BSS_ATTR static char s_metadata_body[8192];\n',
    'static char s_metadata_body[8192] = "";\n',
)

declaration = next((item for item in declarations if item in text), None)

if not declaration:
    raise RuntimeError("could not locate thumbnail metadata body declaration")

text = replace_once(
    text,
    declaration,
    '#define METADATA_BODY_SIZE 8192\n'
    'static char *s_metadata_body = NULL;\n',
    "thumbnail metadata body declaration",
)

helper_anchor = '''static void copy_text(char *destination,
'''

helper = '''static bool ensure_metadata_body(void)
{
    if (s_metadata_body) return true;

    s_metadata_body = heap_caps_calloc(
        1,
        METADATA_BODY_SIZE,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (!s_metadata_body) {
        s_metadata_body = heap_caps_calloc(
            1,
            METADATA_BODY_SIZE,
            MALLOC_CAP_8BIT);
    }

    if (!s_metadata_body) {
        ESP_LOGE(TAG, "Metadata buffer allocation failed");
        return false;
    }

    return true;
}


'''

text = replace_once(
    text,
    helper_anchor,
    helper + helper_anchor,
    "thumbnail helper anchor",
)

build_anchor = '''    memset(s_metadata_body, 0, sizeof(s_metadata_body));
'''

text = replace_once(
    text,
    build_anchor,
    '''    if (!ensure_metadata_body()) {
        snprintf(out, out_size, "Metadata buffer unavailable");
        return false;
    }

    memset(s_metadata_body, 0, METADATA_BODY_SIZE);
''',
    "metadata build allocation",
)

remaining_sizeof = text.count("sizeof(s_metadata_body)")

if remaining_sizeof != 1:
    raise RuntimeError(
        "expected one remaining metadata-body sizeof, "
        f"found {remaining_sizeof}"
    )

text = text.replace(
    "sizeof(s_metadata_body)",
    "METADATA_BODY_SIZE",
    1,
)

SOURCE.write_text(text)

print("PASS: 8 KiB metadata body converted to lazy PSRAM allocation")
print("  - internal BSS now retains only one pointer")
print("  - allocation occurs after PSRAM startup on first metadata request")
print("  - internal-memory fallback preserves operation if PSRAM is unavailable")
print("Next: idf.py fullclean && idf.py build")

