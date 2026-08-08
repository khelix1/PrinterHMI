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

modified = git("diff", "--name-only").stdout.splitlines()
if modified != [target]:
    raise SystemExit(
        f"ERROR: unexpected ESP-IDF modifications: {modified}"
    )

numstat = git("diff", "--numstat", "--", target).stdout.strip()
if numstat != f"1\t1\t{target}":
    raise SystemExit(
        "ERROR: ESP-IDF SDIO patch must change exactly one line"
    )

source = (idf / target).read_text()
required = 'SDIO_SLAVE_CHECK(len > 0, "len <= 0", ESP_ERR_INVALID_ARG);'
legacy = 'SDIO_SLAVE_CHECK(len > 0 && len <= 4092,'
if required not in source or legacy in source:
    raise SystemExit(
        "ERROR: required ESP-IDF SDIO compatibility correction is not exact"
    )

git("diff", "--check")

print(
    "PASS: ESP-IDF v6.0.2 contains only the tracked "
    "ESP-Hosted SDIO patch"
)
