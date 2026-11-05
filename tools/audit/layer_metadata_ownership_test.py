#!/usr/bin/env python3
"""Run actual thumbnail-session metadata ownership and filename encoding checks."""
from pathlib import Path
import argparse,re,subprocess,tempfile
root=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--lvgl-dir',type=Path,required=True);args=p.parse_args()
cjson=root/'tools/theme-maker/linux/native/vendor/cjson'
with tempfile.TemporaryDirectory() as folder:
 t=Path(folder);(t/'esp_attr.h').write_text('#pragma once\n#define EXT_RAM_BSS_ATTR\n')
 source=(root/'main/thumbnail_manager.c').read_text()
 match=re.search(r'^void thumbnail_manager_url_encode\(.*?^\}',source,re.M|re.S)
 assert match
 (t/'encoder.inc').write_text(match.group(0))
 exe=t/'test'
 subprocess.run(['cc','-std=c11','-D_POSIX_C_SOURCE=200809L','-O1','-Wall','-Wextra','-Werror','-ffunction-sections','-fdata-sections','-fsanitize=undefined','-DLV_CONF_SKIP','-DLV_KCONFIG_IGNORE','-I',str(t),'-I',str(root/'main'),'-I',str(root/'tools/theme-maker/linux/native/shims'),'-I',str(args.lvgl_dir),'-I',str(cjson),str(cjson/'cJSON.c'),str(root/'tools/audit/layer_metadata_ownership_test.c'),'-Wl,--gc-sections','-lm','-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
