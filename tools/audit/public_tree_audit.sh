#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(git rev-parse --show-toplevel 2>/dev/null)" || {
    echo "ERROR: run this inside the PrinterHMI Git repository" >&2
    exit 1
}

cd "$repo_dir"

if ! command -v rg >/dev/null 2>&1; then
    echo "ERROR: rg (ripgrep) is required for the public-tree safety audit" >&2
    exit 1
fi

echo "Auditing public main tree..."

if git ls-files | rg -n '(\.bak($|_)|\.backup($|_)|\.before_|\.orig$|\.rej$|~$)'; then
    echo "ERROR: tracked backup or editor-recovery files found" >&2
    exit 1
fi

if git grep -nE \
    '192\.168\.68\.[0-9]{1,3}|INSERT CONTACT METHOD' \
    -- . \
    ':(exclude)tools/end_of_night_checkpoint.sh' \
    ':(exclude)tools/audit/public_tree_audit.sh'; then
    echo "ERROR: personal LAN default or unresolved public placeholder found" >&2
    exit 1
fi

if git grep -nE \
    '#define[[:space:]]+(WIFI_(SSID|PASS)|MOONRAKER_API_KEY)[[:space:]]+"[^"]+"' \
    -- . \
    ':(exclude)tools/end_of_night_checkpoint.sh' \
    ':(exclude)tools/audit/public_tree_audit.sh'; then
    echo "ERROR: embedded network credential definition found" >&2
    exit 1
fi

echo "PASS: public-tree safety audit"
