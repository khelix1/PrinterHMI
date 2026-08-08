#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(git rev-parse --show-toplevel 2>/dev/null)" || {
    echo "ERROR: run this inside the PrinterHMI Git repository" >&2
    exit 1
}

cd "$repo_dir"

lock_file="dependencies.lock"
[[ -s "$lock_file" ]] || {
    echo "ERROR: missing dependency lock: $lock_file" >&2
    exit 1
}

# Local components may use repository-relative paths. Absolute paths would
# make another developer's clean clone non-reproducible.
if rg -n -U \
    '^[[:space:]]*path:[[:space:]]*(/|[A-Za-z]:[/\\]|file://)' \
    "$lock_file"; then
    echo "ERROR: dependencies.lock contains an absolute local path" >&2
    exit 1
fi

if rg -n '(file:///home/|/Users/|\\\\[A-Za-z0-9_-]+\\)' "$lock_file"; then
    echo "ERROR: dependencies.lock contains a machine-specific path" >&2
    exit 1
fi

echo "PASS: dependencies.lock contains only portable sources"
