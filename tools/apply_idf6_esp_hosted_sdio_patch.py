#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
idf = (
    Path(sys.argv[1]).resolve()
    if len(sys.argv) > 1
    else Path.home() / "esp/esp-idf-v6.0.2"
)
patch = root / "tools/patches/esp-idf-6.0.2-esp-hosted-sdio.patch"
target = "components/esp_driver_sdio/src/sdio_slave.c"


def git(*args, capture=True):
    return subprocess.run(
        ["git", "-C", str(idf), *args],
        text=True,
        capture_output=capture,
        check=True,
    )


if not (idf / ".git").exists():
    raise SystemExit(f"ERROR: ESP-IDF path is not a Git checkout: {idf}")

head = git("rev-parse", "HEAD").stdout.strip()
tag_commit = git("rev-list", "-n", "1", "v6.0.2").stdout.strip()

if head != tag_commit:
    raise SystemExit(
        f"ERROR: ESP-IDF HEAD {head[:12]} is not the v6.0.2 commit "
        f"{tag_commit[:12]}"
    )

expected = patch.read_text()
actual = git("diff", "--", target).stdout

if not actual:
    check = subprocess.run(
        ["git", "-C", str(idf), "apply", "--check", str(patch)],
        text=True,
    )
    if check.returncode != 0:
        raise SystemExit("ERROR: required ESP-IDF SDIO patch does not apply")

    subprocess.run(
        ["git", "-C", str(idf), "apply", str(patch)],
        check=True,
    )
    actual = git("diff", "--", target).stdout

if actual != expected:
    raise SystemExit(
        "ERROR: ESP-IDF has changes other than the exact tracked SDIO patch"
    )

modified = git("diff", "--name-only").stdout.splitlines()

if modified != [target]:
    raise SystemExit(
        f"ERROR: unexpected ESP-IDF modifications: {modified}"
    )

git("diff", "--check")

print(
    "PASS: ESP-IDF v6.0.2 contains only the tracked "
    "ESP-Hosted SDIO patch"
)
