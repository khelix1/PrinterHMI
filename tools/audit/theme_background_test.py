#!/usr/bin/env python3
"""Compile and render v3 artwork using real LVGL, cJSON and portable native export."""
from pathlib import Path
import argparse,json,subprocess,sys,tempfile,zipfile,io
root=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(root/'tools/theme-maker/linux'))
from PIL import Image
from artwork import store_source,bundle
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--lvgl-dir',type=Path,required=True);p.add_argument('--lvgl-lib',type=Path,required=True);p.add_argument('--cjson-dir',type=Path,required=True)
a=p.parse_args()
with tempfile.TemporaryDirectory(prefix='theme-artwork-') as temporary:
 t=Path(temporary)
 model=json.loads(subprocess.check_output(['node','-e',"console.log(JSON.stringify(require('./tools/theme-maker/theme-model.js').createBlank()))"],cwd=root,text=True))
 image=Image.new('RGB',(1024,600),'#FF0000');image.paste('#0000FF',(512,0,1024,600));stream=io.BytesIO();image.save(stream,'PNG')
 asset,_=store_source(stream.getvalue(),t/'sources')
 model.update(schema_version=3,background={'asset':asset,'fit':'cover','position_x':50,'position_y':50,'overlay_color':'#000000','overlay_opacity':0})
 with zipfile.ZipFile(io.BytesIO(bundle(json.dumps(model),t/'sources'))) as archive:
  names=archive.namelist();theme=next(n for n in names if n.endswith('.phmitheme'));doc=json.loads(archive.read(theme))
  path=t/'theme.phmitheme';path.write_bytes(archive.read(theme));(t/doc['background']['asset']).write_bytes(archive.read('PrinterHMI/themes/'+doc['background']['asset']))
  studio=json.loads(subprocess.check_output(['node','-e',"console.log(JSON.stringify(require('./tools/theme-maker/theme-model.js').create('studio')))"],cwd=root,text=True));studio.update(schema_version=3,background=doc['background']);(t/'studio.phmitheme').write_text(json.dumps(studio))
  doc['background']['asset']='../outside.phmibg';(t/'invalid.phmitheme').write_text(json.dumps(doc))
 (t/'esp_heap_caps.h').write_text('''#pragma once
#include <stdlib.h>
extern int fail_background_alloc;
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
#define heap_caps_malloc(n,c) ((fail_background_alloc && (n)>=1228800)?NULL:malloc(n))
#define heap_caps_calloc(n,s,c) calloc(n,s)
#define heap_caps_free(p) free(p)
''')
 (t/'esp_log.h').write_text('#pragma once\n#include <stdio.h>\n#define ESP_LOGI(t,...) do{(void)(t);if(0)printf(__VA_ARGS__);}while(0)\n#define ESP_LOGW ESP_LOGI\n#define ESP_LOGE ESP_LOGI\n')
 flags=['-DLV_CONF_SKIP','-DLV_KCONFIG_IGNORE','-DLV_USE_STDLIB_MALLOC=1']+[f'-DLV_FONT_MONTSERRAT_{n}=1' for n in range(12,34,2)]+['-DLV_FONT_MONTSERRAT_48=1']
 exe=t/'render'
 subprocess.run(['cc','-std=c11','-D_POSIX_C_SOURCE=200809L','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=undefined','-ffunction-sections','-fdata-sections',*flags,'-I',str(t),'-I',str(a.lvgl_dir.resolve()),'-I',str(a.lvgl_dir.resolve()/'src'),'-I',str(a.cjson_dir.resolve()),'-I',str(root/'main'),str(root/'tools/audit/theme_background_test.c'),*[str(root/'main'/s) for s in ['ui_theme.c','ui_dashboard_layout_profile.c','ui_page_layout_profile.c','theme_background_asset.c']],str(a.cjson_dir/'cJSON.c'),str(a.lvgl_lib.resolve()),'-Wl,--gc-sections','-lm','-o',str(exe)],check=True)
 subprocess.run([str(exe),str(path),str(t/'invalid.phmitheme'),str(t/'studio.phmitheme')],check=True,timeout=30)
