#!/usr/bin/env python3
"""Build real LVGL host checks for preview fill. Supply --lvgl-dir if unmanaged."""
from pathlib import Path
import argparse
import concurrent.futures
import subprocess
import tempfile
root=Path(__file__).resolve().parents[2]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument("--lvgl-dir",type=Path)
parser.add_argument("--lvgl-lib",type=Path,help="Optional host LVGL archive built with these font/stdlib flags")
args=parser.parse_args()
candidates=[args.lvgl_dir] if args.lvgl_dir else list((root/"managed_components").glob("*lvgl*"))
lvgl=next((p for p in candidates if p and (p/"src/core/lv_obj.c").is_file()),None)
if lvgl is None:parser.error("LVGL sources not found; supply --lvgl-dir")
lvgl=lvgl.resolve()
flags=["-DLV_CONF_SKIP","-DLV_KCONFIG_IGNORE","-DLV_USE_STDLIB_MALLOC=1"]+[f"-DLV_FONT_MONTSERRAT_{n}=1" for n in range(14,34,2)]+["-DLV_FONT_MONTSERRAT_48=1"]
with tempfile.TemporaryDirectory(prefix="preview-fill-") as directory:
    tmp=Path(directory)
    (tmp/"esp_heap_caps.h").write_text("#pragma once\n#include <stdlib.h>\n#define MALLOC_CAP_SPIRAM 1\n#define MALLOC_CAP_8BIT 2\n#define heap_caps_malloc(size,caps) malloc(size)\n#define heap_caps_free(ptr) free(ptr)\n")
    library=args.lvgl_lib
    if library is None:
        def compile_source(p):
            obj=tmp/(p.relative_to(lvgl).as_posix().replace("/","_")+".o")
            subprocess.run(["cc","-std=c11","-O1","-ffunction-sections","-fdata-sections",*flags,"-I",str(lvgl),"-c",str(p),"-o",str(obj)],check=True,capture_output=True)
            return str(obj)
        with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
            objects=list(pool.map(compile_source,(lvgl/"src").rglob("*.c")))
        library=tmp/"liblvgl.a"
        subprocess.run(["ar","rcs",str(library),*objects],check=True)
    executable=tmp/"preview_fill"
    subprocess.run(["cc","-std=c11","-O2","-Wall","-Wextra","-Werror","-Wrestrict","-ffunction-sections","-fdata-sections",*flags,
                    "-I",str(tmp),"-I",str(lvgl),"-I",str(lvgl/"src"),"-I",str(root/"main"),
                    str(root/"tools/audit/preview_fill_test.c"),str(root/"main/thumbnail_render.c"),str(root/"main/ui_thumbnail.c"),
                    str(library),"-Wl,--gc-sections","-lm","-o",str(executable)],check=True)
    subprocess.run([str(executable)],check=True)
