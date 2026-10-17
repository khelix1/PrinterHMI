#!/usr/bin/env python3
"""Keep declared dependency credits and local notice references complete."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[2]
credits_path = root / "docs/SOFTWARE_CREDITS.md"
credits = credits_path.read_text()
lock = (root / "dependencies.lock").read_text().split("direct_dependencies:", 1)[0]
names = re.findall(r"^  ([^\s:]+):$", lock, re.M)
assert names, "No locked components found; inspect dependency-lock format"
missing = [name for name in names if f"`{name}`" not in credits]
assert not missing, f"Add software credits for locked components: {missing}"
references = re.findall(r"\]\(([^)]+)\)", credits)
for target in references:
    if "://" in target or target.startswith("#"):
        continue
    assert (credits_path.parent / target.split("#", 1)[0]).is_file(), target
for name, author in [("LVGL.txt", "LVGL"), ("LodePNG.txt", "Lode Vandevenne"),
                     ("Inter-OFL.txt", "Inter Project Authors"),
                     ("Montserrat-OFL.txt", "Montserrat Project Authors"),
                     ("Font-Awesome-Free.txt", "Fonticons, Inc."),
                     ("DejaVu.txt", "Bitstream"), ("cJSON.txt", "Dave Gamble"),
                     ("ESP-IDF-COPYRIGHT.rst", "Espressif Systems")]:
    assert author in (root / "docs/licenses" / name).read_text(), name
print(f"PASS: all {len(names)} locked components credited, local notice references resolve, original author notices retained")
