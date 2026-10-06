#!/usr/bin/env python3
"""Host checks for production cache retention/writes and download-buffer lifetime."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix="thumbnail-storage-") as directory:
    tmp = Path(directory)
    flags = ["-std=c11", "-O2", "-Wall", "-Wextra", "-Werror", "-Wrestrict"]
    store = tmp / "storage"
    subprocess.run(["cc", *flags, '-DTHUMBNAIL_CACHE_ROOT="' + str(tmp / "sd") + '"',
                    "-I", str(root / "main"), str(root / "tools/audit/thumbnail_storage_test.c"),
                    "-o", str(store)], check=True)
    subprocess.run([str(store)], check=True)
    source = (root / "main/moonraker.c").read_text()
    start = source.index("static bool moonraker_fetch_thumbnail_encoded_internal(")
    end = source.index("bool moonraker_fetch_thumbnail_encoded(", start)
    harness = (root / "tools/audit/thumbnail_download_buffer_test.c").read_text()
    test = tmp / "download.c"
    test.write_text(harness.replace("/* PRODUCTION_DOWNLOADER */", source[start:end]))
    download = tmp / "download"
    subprocess.run(["cc", *flags, str(test), "-o", str(download)], check=True)
    subprocess.run([str(download)], check=True)
