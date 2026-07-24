#!/usr/bin/env python3
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "main" / "main.c"


def remove_exact(text: str, old: str, expected: int, label: str) -> str:
    count = text.count(old)
    if count != expected:
        raise RuntimeError(f"expected {expected} {label}, found {count}")
    return text.replace(old, "")


if not SOURCE.exists():
    raise RuntimeError(f"missing required file: {SOURCE}")

text = SOURCE.read_text()

marker = "BOOT_PREVIEW_PROFILE_STORE_ONLY"
if marker in text:
    print("PASS: boot preview restoration is already profile-store-only")
    raise SystemExit(0)

text = remove_exact(
    text,
    "static volatile bool dash_thumb_restore_pending = false;\n",
    1,
    "global thumbnail pending flag",
)

# The current source contains this declaration twice. Neither declaration is
# needed after removal of the unsafe boot restore helper.
text = remove_exact(
    text,
    "static void hmi_restore_last_selected_file_from_sd(void);\n",
    2,
    "global thumbnail restore declarations",
)

restore_helper = '''static void hmi_restore_last_selected_file_from_sd(void)
{
    thumbnail_session_v32_restore_result_t result =
        thumbnail_session_v32_restore_last_selected_file(
            sd_card_ok);

    if (result ==
        THUMBNAIL_SESSION_V32_RESTORE_CACHE_READY) {
        dash_thumb_restore_pending = true;
    }
}


'''

text = remove_exact(
    text,
    restore_helper,
    1,
    "global thumbnail restore helper",
)

text = remove_exact(
    text,
    "    hmi_restore_last_selected_file_from_sd();\n",
    1,
    "global thumbnail restore call",
)

pending_ui = '''            if (dash_thumb_restore_pending) {
                dash_thumb_restore_pending = false;
                dashboard_show_loaded_thumbnail();
                ESP_LOGI(TAG, "DASH_RESTORE_PENDING_UI shown");
            }

'''

text = remove_exact(
    text,
    pending_ui,
    1,
    "global thumbnail pending UI block",
)

profile_restore = \
    "        printer_preview_store_v32_restore_one(sd_card_ok);\n"

if text.count(profile_restore) != 1:
    raise RuntimeError(
        "expected one profile preview store restore call, found "
        f"{text.count(profile_restore)}"
    )

text = text.replace(
    profile_restore,
    '''        /* BOOT_PREVIEW_PROFILE_STORE_ONLY
         * Reboot restoration is profile-indexed and endpoint-validated.
         * The legacy global last-file cache must not publish into whichever
         * printer profile happens to be active at boot.
         */
        printer_preview_store_v32_restore_one(sd_card_ok);
''',
    1,
)

for forbidden in (
    "dash_thumb_restore_pending",
    "hmi_restore_last_selected_file_from_sd",
    "DASH_RESTORE_PENDING_UI",
):
    if forbidden in text:
        raise RuntimeError(f"unsafe boot preview hook remains: {forbidden}")

SOURCE.write_text(text)

print("PASS: boot preview ownership corrected")
print("  - removed global last-file thumbnail boot publication")
print("  - removed pending Dashboard global-thumbnail restore")
print("  - retained per-profile endpoint-validated SD restoration")
print("  - explicit selected-file preview behavior remains available")
print("Next: idf.py build")

