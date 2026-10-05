#!/usr/bin/env python3
"""Host checks for Console presentation filters (requires a C compiler)."""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix="console-filters-") as directory:
    executable=Path(directory)/"console_filters"
    subprocess.run(["cc","-std=c11","-O2","-Wall","-Wextra","-Werror",
                    "-I",str(root/"main"),str(root/"main/console_filter.c"),
                    str(root/"tools/audit/console_filters_test.c"),"-lm","-o",str(executable)],check=True)
    subprocess.run([str(executable)],check=True)
