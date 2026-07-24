#!/usr/bin/env python3
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "main" / "printer_preview_cache_v32.c"


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"expected one {label}, found {count}")
    return text.replace(old, new, 1)


if not SOURCE.exists():
    raise RuntimeError(f"missing required file: {SOURCE}")

text = SOURCE.read_text()

if "static bool ensure_slots(void)" in text:
    print("PASS: preview cache already uses lazy PSRAM bookkeeping")
    raise SystemExit(0)

text = replace_once(
    text,
    "static preview_slot_t\n"
    "    s_slots[MOONRAKER_CONFIG_MAX_PROFILES];\n",
    "static preview_slot_t *s_slots = NULL;\n",
    "static preview slot array",
)

valid_index = '''static bool valid_index(int index)
{
    return index >= 0 && index < MOONRAKER_CONFIG_MAX_PROFILES;
}
'''

ensure_slots = valid_index + '''

static bool ensure_slots(void)
{
    if (s_slots) return true;

    s_slots = heap_caps_calloc(
        MOONRAKER_CONFIG_MAX_PROFILES,
        sizeof(preview_slot_t),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (!s_slots) {
        s_slots = heap_caps_calloc(
            MOONRAKER_CONFIG_MAX_PROFILES,
            sizeof(preview_slot_t),
            MALLOC_CAP_8BIT);
    }

    if (!s_slots) {
        ESP_LOGE(TAG, "Preview slot allocation failed");
        return false;
    }

    return true;
}
'''

text = replace_once(
    text,
    valid_index,
    ensure_slots,
    "valid-index helper",
)

install_anchor = '''    preview_slot_t *slot = &s_slots[profile_index];

    if (slot->pixels) {
'''

text = replace_once(
    text,
    install_anchor,
    '''    if (!ensure_slots()) return false;

    preview_slot_t *slot = &s_slots[profile_index];

    if (slot->pixels) {
''',
    "pixel installation slot access",
)

matches_anchor = '''    if (!valid_index(profile_index) || !file || !file[0]) return false;

    preview_slot_t *slot = &s_slots[profile_index];
'''

text = replace_once(
    text,
    matches_anchor,
    '''    if (!valid_index(profile_index) || !file || !file[0]) return false;
    if (!ensure_slots()) return false;

    preview_slot_t *slot = &s_slots[profile_index];
''',
    "cache-match slot access",
)

image_anchor = '''    if (!valid_index(profile_index)) return NULL;

    preview_slot_t *slot = &s_slots[profile_index];
'''

text = replace_once(
    text,
    image_anchor,
    '''    if (!valid_index(profile_index)) return NULL;
    if (!ensure_slots()) return NULL;

    preview_slot_t *slot = &s_slots[profile_index];
''',
    "cache-image slot access",
)

invalidate_anchor = '''    if (!valid_index(profile_index)) return;

    preview_slot_t *slot = &s_slots[profile_index];
'''

text = replace_once(
    text,
    invalidate_anchor,
    '''    if (!valid_index(profile_index) || !s_slots) return;

    preview_slot_t *slot = &s_slots[profile_index];
''',
    "cache-invalidate slot access",
)

reset_anchor = '''void printer_preview_cache_v32_reset(void)
{
    for (int index = 0; index < MOONRAKER_CONFIG_MAX_PROFILES; ++index) {
'''

text = replace_once(
    text,
    reset_anchor,
    '''void printer_preview_cache_v32_reset(void)
{
    if (!s_slots) return;

    for (int index = 0; index < MOONRAKER_CONFIG_MAX_PROFILES; ++index) {
''',
    "cache reset",
)

SOURCE.write_text(text)

print("PASS: preview cache bookkeeping moved out of internal static RAM")
print("  - cache slots allocate lazily in PSRAM after startup")
print("  - no new task, timer, mutex, or scheduler object")
print("Next: idf.py fullclean && idf.py build")

