#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(pwd)"
ARCHIVE_ROOT="$PROJECT_ROOT/archive/project_history_20260718"
MANIFEST="$ARCHIVE_ROOT/MOVED_FILES_PASS2.txt"
APPLY="${APPLY:-0}"

if [[ ! -f CMakeLists.txt || ! -d main ]]; then
    echo "ERROR: Run from the PrinterHMI_v3_2 project root."
    exit 1
fi

mkdir -p \
    "$ARCHIVE_ROOT/document_snapshots" \
    "$ARCHIVE_ROOT/test_scripts" \
    "$ARCHIVE_ROOT/rollback_helpers" \
    "$ARCHIVE_ROOT/ui_change_backups" \
    "$ARCHIVE_ROOT/audits_and_logs" \
    "$ARCHIVE_ROOT/config_snapshots" \
    "$ARCHIVE_ROOT/miscellaneous"

: > "$MANIFEST"

move_item()
{
    local source="$1"
    local destination="$2"

    [[ -e "$source" ]] || return 0

    printf '%-8s %-65s -> %s/\n' \
        "$([[ "$APPLY" == "1" ]] && echo MOVE || echo PREVIEW)" \
        "${source#./}" \
        "${destination#"$PROJECT_ROOT/"}"

    printf '%s -> %s/\n' \
        "${source#./}" \
        "${destination#"$PROJECT_ROOT/"}" >> "$MANIFEST"

    if [[ "$APPLY" == "1" ]]; then
        mkdir -p "$destination"
        mv -- "$source" "$destination/"
    fi
}

echo
echo "===== DOCUMENT SNAPSHOTS ====="

while IFS= read -r -d '' item; do
    move_item "$item" "$ARCHIVE_ROOT/document_snapshots"
done < <(
    find . -maxdepth 1 -type f \
        \( \
            -name '*.md.before_*' -o \
            -name '*.md.freeze_*' -o \
            -name '*.md.pre_*' -o \
            -name '*.md.POST_*' \
        \) \
        -print0
)

echo
echo "===== NUMBERED TEST SCRIPTS ====="

while IFS= read -r -d '' item; do
    move_item "$item" "$ARCHIVE_ROOT/test_scripts"
done < <(
    find . -maxdepth 1 -type f \
        \( \
            -name 'test[0-9]*' -o \
            -name 'test[0-9]*_*' -o \
            -name 'stage2b*' \
        \) \
        -print0
)

echo
echo "===== ROLLBACK HELPERS ====="

while IFS= read -r -d '' item; do
    move_item "$item" "$ARCHIVE_ROOT/rollback_helpers"
done < <(
    find . -maxdepth 1 -type f \
        \( \
            -name 'rollback_*' -o \
            -name 'migrate_*' \
        \) \
        -print0
)

echo
echo "===== DATED UI CHANGE BACKUPS ====="

while IFS= read -r -d '' item; do
    move_item "$item" "$ARCHIVE_ROOT/ui_change_backups"
done < <(
    find . -maxdepth 1 -mindepth 1 \
        \( -type f -o -type d \) \
        \( \
            -name 'network_*_20????????_??????' -o \
            -name 'operator_*_20????????_??????' -o \
            -name 'printer_*_20????????_??????' -o \
            -name 'shared_*_20????????_??????' -o \
            -name 'theme_*_20????????_??????' \
        \) \
        -print0
)

echo
echo "===== AUDITS, WATCH LOGS, AND DIFFS ====="

while IFS= read -r -d '' item; do
    move_item "$item" "$ARCHIVE_ROOT/audits_and_logs"
done < <(
    find . -maxdepth 1 -type f \
        \( \
            -name '*AUDIT*.txt' -o \
            -name '*audit*.txt' -o \
            -name 'reset_watch*.log' -o \
            -name 'reset_reason_watch*.log' -o \
            -name '*.diff' -o \
            -name 'shared_ui_inventory.txt' -o \
            -name 'main_remaining_audit.txt' \
        \) \
        -print0
)

echo
echo "===== SDKCONFIG SNAPSHOTS ====="

while IFS= read -r -d '' item; do
    move_item "$item" "$ARCHIVE_ROOT/config_snapshots"
done < <(
    find . -maxdepth 1 -type f \
        \( \
            -name 'sdkconfig.freeze_*' -o \
            -name 'sdkconfig.old' \
        \) \
        -print0
)

echo
echo "===== MISCELLANEOUS ROOT ITEMS ====="

for item in \
    "./1" \
    "./archive_project_clutter.sh"
do
    if [[ -e "$item" ]]; then
        move_item "$item" "$ARCHIVE_ROOT/miscellaneous"
    fi
done

echo
if [[ "$APPLY" == "1" ]]; then
    echo "===== PASS 2 COMPLETE ====="
    echo "Archive:  $ARCHIVE_ROOT"
    echo "Manifest: $MANIFEST"
else
    echo "===== PREVIEW ONLY ====="
    echo "Nothing was moved."
    echo
    echo "Review the list, then run:"
    echo
    echo "    APPLY=1 ./archive_root_clutter_pass2.sh"
fi
