#!/usr/bin/env python3
"""Verify repository-relative Markdown links in active public documentation."""
from __future__ import annotations

import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(subprocess.check_output(
    ["git", "rev-parse", "--show-toplevel"], text=True).strip())
LINK = re.compile(r"(?<!!)\[[^]]*\]\(([^)]+)\)")
IGNORED_PREFIXES = ("http://", "https://", "mailto:", "tel:", "data:")


def tracked_markdown() -> list[pathlib.Path]:
    paths = subprocess.check_output(
        ["git", "ls-files", "*.md"], cwd=ROOT, text=True).splitlines()
    return [ROOT / path for path in paths
            if not path.startswith("docs/history/")]


missing: list[str] = []
for source in tracked_markdown():
    text = source.read_text(encoding="utf-8")
    for raw in LINK.findall(text):
        target = raw.strip().split()[0]
        # GitHub renders release bodies outside docs/releases/, so relative
        # repository links become malformed /blob/ URLs on the Release page.
        if source.relative_to(ROOT).parts[:2] == ("docs", "releases") and not target.startswith(("http://", "https://", "#")):
            missing.append(f"{source.relative_to(ROOT)} -> release-page link must be an absolute URL: {raw}")
            continue
        if not target or target.startswith("#") or target.startswith(IGNORED_PREFIXES):
            continue
        target = target.split("#", 1)[0].split("?", 1)[0]
        if not target:
            continue
        if not (source.parent / target).resolve().is_file():
            missing.append(f"{source.relative_to(ROOT)} -> {raw}")

if missing:
    print("ERROR: broken repository-relative Markdown links:", file=sys.stderr)
    print("\n".join(missing), file=sys.stderr)
    sys.exit(1)
print("PASS: active public Markdown links resolve")
