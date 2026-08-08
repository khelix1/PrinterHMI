#!/usr/bin/env bash
set -euo pipefail

build_dir="${1:-build-idf6-hosted3}"
log_dir="$build_dir/log"

[[ -d "$log_dir" ]] || {
    echo "ERROR: build log directory not found: $log_dir" >&2
    exit 1
}

mapfile -t logs < <(
    find "$log_dir" -maxdepth 1 -type f \
        \( -name 'idf_py_stdout_output_*' -o -name 'idf_py_stderr_output_*' \) \
        -printf '%T@ %p\n' | sort -nr | head -n 2 | cut -d ' ' -f2-
)

(( ${#logs[@]} > 0 )) || {
    echo "ERROR: no ESP-IDF build logs found in $log_dir" >&2
    exit 1
}

# Inspect only the latest stdout/stderr pair from the just-finished build. Gate
# warnings produced by PrinterHMI's owned application sources; ESP-IDF Kconfig
# notes and third-party component warnings remain visible but out of scope.
warnings="$({
    grep -HnE '/main/[^:]+:[0-9]+:[0-9]+: warning:' "${logs[@]}" || true
} | sort -u)"

if [[ -n "$warnings" ]]; then
    echo "ERROR: compiler warnings were emitted from PrinterHMI application sources:" >&2
    printf '%s\n' "$warnings" >&2
    exit 1
fi

echo "PASS: no compiler warnings from PrinterHMI application sources"
