#!/usr/bin/env python3
"""Compile the actual custom-theme SD parser with real cJSON and LVGL headers."""
from pathlib import Path
import argparse, json, subprocess, tempfile
root=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--lvgl-dir',required=True,type=Path)
p.add_argument('--cjson-dir',required=True,type=Path)
a=p.parse_args()
with tempfile.TemporaryDirectory(prefix='theme-package-') as d:
 t=Path(d)
 exports=subprocess.check_output(['node','-e',"const M=require('./tools/theme-maker/theme-model.js');console.log(JSON.stringify(Object.fromEntries(['studio','operator','foundry','glass','blank'].map(b=>[b,JSON.parse(M.serialize(b==='blank'?M.createBlank():M.create(b)))]))))"],cwd=root,text=True)
 packages=json.loads(exports)
 for name,doc in packages.items():
  if name=='studio':
   doc['palette']={k:'#123456' for k in doc['palette']}
   doc['metrics'].update(radius_card=3,radius_button=4,radius_panel=5)
  (t/(name+'.json')).write_text(json.dumps(doc))
 def invalid(name,base,change):
  doc=json.loads(json.dumps(packages[base]));change(doc);(t/(name+'.json')).write_text(json.dumps(doc))
 invalid('missing_color','studio',lambda p:p['palette'].pop('danger_dark'))
 invalid('invalid_color','studio',lambda p:p['palette'].update(text='#ZZZZZZ'))
 invalid('studio_legacy','studio',lambda p:p.update(schema_version=1))
 invalid('studio_layout','studio',lambda p:p.update(layouts={}))
 invalid('studio_opacity','studio',lambda p:p['metrics'].update(surface_opacity=128))
 invalid('missing_metric','studio',lambda p:p['metrics'].pop('radius_card'))
 invalid('overflow_layout','operator',lambda p:p['layouts']['dashboard']['banner'].update(x=814,width=854))
 (t/'oversized.json').write_text(' '*49153)
 (t/'esp_log.h').write_text('#pragma once\n#include <stdio.h>\n#define ESP_LOGI(t,...) do{(void)(t);if(0)printf(__VA_ARGS__);}while(0)\n#define ESP_LOGW ESP_LOGI\n#define ESP_LOGE ESP_LOGI\n')
 (t/'esp_heap_caps.h').write_text('#pragma once\n#include <stdlib.h>\n#define MALLOC_CAP_SPIRAM 1\n#define MALLOC_CAP_8BIT 2\n#define heap_caps_malloc(n,c) malloc(n)\n#define heap_caps_calloc(n,s,c) calloc(n,s)\n#define heap_caps_free(p) free(p)\n')
 flags=['-DLV_CONF_SKIP','-DLV_KCONFIG_IGNORE']+[f'-DLV_FONT_MONTSERRAT_{n}=1' for n in range(12,34,2)]+['-DLV_FONT_MONTSERRAT_48=1']
 exe=t/'check'
 subprocess.run(['cc','-std=c11','-D_POSIX_C_SOURCE=200809L','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=undefined','-ffunction-sections','-fdata-sections',*flags,'-I',str(t),'-I',str(a.lvgl_dir.resolve()),'-I',str(a.cjson_dir.resolve()),'-I',str(root/'main'),str(root/'tools/audit/custom_theme_package_test.c'),*[str(root/'main'/s) for s in ['ui_theme.c','ui_dashboard_layout_profile.c','ui_page_layout_profile.c','theme_background_asset.c']],str(a.cjson_dir/'cJSON.c'),str(a.lvgl_dir/'src/misc/lv_color.c'),'-Wl,--gc-sections','-lm','-o',str(exe)],check=True)
 subprocess.run([str(exe),str(t)],check=True)
