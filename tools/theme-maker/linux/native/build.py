#!/usr/bin/env python3
"""Build the Linux LVGL helper directly from this checkout's firmware UI sources."""
from pathlib import Path
import argparse,concurrent.futures,re,subprocess
here=Path(__file__).resolve().parent
root=here.parents[3]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--lvgl-dir',type=Path);p.add_argument('--lvgl-lib',type=Path);p.add_argument('--output-dir',type=Path,default=root/'build-theme-maker-native');args=p.parse_args()
candidate=args.lvgl_dir or next((v for v in (root/'managed_components').glob('*lvgl*') if (v/'src/core/lv_obj.c').is_file()),None)
if candidate is None:p.error('LVGL sources not found; use --lvgl-dir /path/to/lvgl')
lvgl=candidate.resolve();out=args.output_dir.resolve();out.mkdir(parents=True,exist_ok=True)
if not (lvgl/'src/core/lv_obj.c').is_file():p.error('LVGL source tree is missing')
source=(root/'main/custom_theme.c').read_text();routes=re.findall(r'\{"([a-z_0-9]+)",\s*(0x[0-9A-Fa-f]+),\s*(0x[0-9A-Fa-f]+),\s*(0x[0-9A-Fa-f]+),\s*COLOR_[A-Z_0-9]+\}',source)
if not 25<=len(routes)<=64:raise RuntimeError('Unrecognized firmware color routes')
(out/'routes.h').write_text('struct route {const char *name; unsigned a,b,c;};\nstatic const struct route routes[]={\n'+''.join('{"%s",%s,%s,%s},\n'%r for r in routes)+'};\n#define ROUTE_COUNT (sizeof(routes)/sizeof(routes[0]))\n')
flags=['-DLV_CONF_SKIP','-DLV_KCONFIG_IGNORE','-DLV_USE_STDLIB_MALLOC=1']+[f'-DLV_FONT_MONTSERRAT_{n}=1' for n in range(12,34,2)]+['-DLV_FONT_MONTSERRAT_48=1']
lib=args.lvgl_lib.resolve() if args.lvgl_lib else out/'liblvgl.a'
if not args.lvgl_lib:
 def compile_one(source):
  obj=out/(source.relative_to(lvgl).as_posix().replace('/','_')+'.o');subprocess.run(['cc','-std=c11','-O2','-ffunction-sections','-fdata-sections',*flags,'-I',str(lvgl),'-c',str(source),'-o',str(obj)],check=True,capture_output=True);return str(obj)
 with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:objects=list(pool.map(compile_one,(lvgl/'src').rglob('*.c')))
 subprocess.run(['ar','rcs',str(lib),*objects],check=True)
sources=['ui_popup.c','ui_button.c','ui_theme.c','ui_theme_a.c','ui_theme_b.c','ui_theme_c.c','ui_theme_studio.c','ui_studio_icons.c','ui_font_fallback.c','ui_text.c','ui_widgets.c','console_filter.c','macro_parameter_utils.c','ui_page_title.c','ui_page_layout_profile.c','ui_printer_popups.c','ui_printer_toolhead.c']+['assets/fonts/studio/inter_'+str(n)+'.c' for n in [18,20,24,28,32,48,64,96]]
subprocess.run(['cc','-std=c11','-D_POSIX_C_SOURCE=200809L','-O2','-Wall','-Wextra','-Werror','-ffunction-sections','-fdata-sections',*flags,'-I',str(here/'shims'),'-I',str(out),'-I',str(lvgl),'-I',str(lvgl/'src'),'-I',str(root/'main'),str(here/'host.c'),str(here/'tuning_bridge.c'),*[str(root/'main'/s) for s in sources],str(lib),'-Wl,--gc-sections','-lm','-o',str(out/'printerhmi-ui-host')],check=True)
print(out/'printerhmi-ui-host')
