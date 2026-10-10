#!/usr/bin/env python3
"""Build real LVGL host checks for complete button labels and affected row layouts in every theme. Supply --lvgl-dir if unmanaged."""
from pathlib import Path
import argparse
import concurrent.futures
import subprocess
import tempfile
root=Path(__file__).resolve().parents[2]
import re,json
def calls(s,name):
 for m in re.finditer(r'\b'+re.escape(name)+r'\s*\(',s):
  start=m.end(); depth=1; quote=False; esc=False; args=[];part=start
  for i in range(start,len(s)):
   c=s[i]
   if quote:
    if esc:esc=False
    elif c=='\\':esc=True
    elif c=='"':quote=False
    continue
   if c=='"':quote=True;continue
   if c=='(':depth+=1
   if c==')':
    depth-=1
    if not depth:args.append(s[part:i].strip());yield m.start(),i+1,args;break
   if c==',' and depth==1:args.append(s[part:i].strip());part=i+1

def texts(x):
 # Enumerate literal alternatives; keep adjacent symbols with strings.
 x=x.replace('ui_text(', '(')
 branches=re.split(r'\?|:',x)
 found=[]
 for b in branches:
  tokens=re.findall(r'"(?:\\.|[^"\\])*"|LV_SYMBOL_\w+',b)
  if tokens:found.append(' '.join(tokens))
 return found

def size(x):return bool(re.fullmatch(r'[0-9\s,+*/()-]+|ui_theme_density_metric\([0-9\s,]+\)',x))
cases=[]; inventory=[]
for p in sorted((root/'main').glob('*.c')):
 s=re.sub(r'/\*[\s\S]*?\*/|//[^\n]*',lambda m: ''.join('\n' if c=='\n' else ' ' for c in m.group()),p.read_text())
 for name in ('ui_button_create','ui_button_create_icon','ui_popup_add_action_at','ui_create_button','ui_create_button_primary','ui_create_button_secondary','ui_create_button_success','ui_create_button_warning','ui_create_button_danger','lv_button_create','ui_button_create_empty','ui_popup_add_footer_action','ui_popup_add_action_aligned','ui_popup_add_selectable_row','ui_create_operator_nav_button'):
  for start,end,a in calls(s,name):
   if s[end:].lstrip().startswith('{'):continue
   line=s.count('\n',0,start)+1; entry={'file':p.name,'line':line,'api':name,'args':a};inventory.append(entry)
   if name in ('lv_button_create','ui_button_create_empty'):continue
   icon=None;geom=None;kind='UI_BUTTON_OUTLINED'
   if name in ('ui_popup_add_selectable_row','ui_create_operator_nav_button'):continue
   if name=='ui_popup_add_footer_action' and len(a)>=4:
    text=a[2];geom=[a[3],'48']
   elif name=='ui_popup_add_action_aligned' and len(a)>=5:
    text=a[2];geom=a[3:5]
   elif name=='ui_popup_add_action_at' and len(a)>=7:
    text=a[2];geom=a[5:7]
   else:
    text=a[3] if name=='ui_button_create_icon' and len(a)>3 else a[2] if name=='ui_button_create' and len(a)>2 else a[1] if len(a)>1 else ''
    if name=='ui_button_create_icon':icon=a[2]
    prefix=s[max(0,start-150):start]
    match=re.search(r'([A-Za-z_][\w]*(?:->\w+)?(?:\[\w+\])?)\s*=\s*$',prefix)
    if match:
     variable=match[1]
     for _,_,z in calls(s[end:end+1800],'lv_obj_set_size'):
      if len(z)==3 and z[0]==variable:geom=z[1:];break
   # Macro actions depend on native sizing and Studio's final font fitting.
   # Exercise the real owner in responsive_layout_test instead of isolated geometry.
   if p.name=='ui_macros.c' and name=='ui_button_create' and text in ('"SEARCH"','"CLEAR SEARCH"'):continue
   if geom and all(size(z) for z in geom) and not (geom[0]=='1'):
    for t in texts(text):
     if icon and not re.fullmatch(r'LV_SYMBOL_\w+|"(?:\\.|[^"\\])*"',icon):continue
     cases.append({'file':p.name,'line':line,'text':t,'w':geom[0],'h':geom[1],'icon':icon,'api':name})

# Parameterized toolhead controls have fixed geometry supplied at their call sites.
source=(root/"main/ui_printer_toolhead.c").read_text()
for start,end,a in calls(source,"make_button"):
 if len(a)>6 and size(a[5]) and size(a[6]):
  for t in texts(a[2]):
   cases.append(dict(file="ui_printer_toolhead.c",line=source.count("\n",0,start)+1,text=t,w=a[5],h=a[6],icon=None,api="make_button"))
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument("--lvgl-dir",type=Path)
parser.add_argument("--lvgl-lib",type=Path,help="Optional host LVGL archive built with these font/stdlib flags")
args=parser.parse_args()
candidates=[args.lvgl_dir] if args.lvgl_dir else list((root/"managed_components").glob("*lvgl*"))
lvgl=next((p for p in candidates if p and (p/"src/core/lv_obj.c").is_file()),None)
if lvgl is None:parser.error("LVGL sources not found; supply --lvgl-dir")
lvgl=lvgl.resolve()
flags=["-DLV_CONF_SKIP","-DLV_KCONFIG_IGNORE","-DLV_USE_STDLIB_MALLOC=1"]+[f"-DLV_FONT_MONTSERRAT_{n}=1" for n in range(12,34,2)]+["-DLV_FONT_MONTSERRAT_48=1"]
with tempfile.TemporaryDirectory(prefix="lvgl95-ui-") as directory:
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
    (tmp/"esp_heap_caps.h").write_text("#pragma once\n#include <stdlib.h>\n#define MALLOC_CAP_SPIRAM 1\n#define MALLOC_CAP_8BIT 2\n#define MALLOC_CAP_INTERNAL 4\n#define heap_caps_calloc(n,size,caps) calloc(n,size)\n#define heap_caps_free(p) free(p)\n")
    (tmp/"esp_err.h").write_text("#pragma once\ntypedef int esp_err_t;\n#define ESP_OK 0\n#define ESP_FAIL -1\n")
    (tmp/"esp_http_client.h").write_text("#pragma once\ntypedef void *esp_http_client_handle_t;\ntypedef struct { int event_id; } esp_http_client_event_t;\n")
    (tmp/"esp_log.h").write_text("#pragma once\n#include <stdio.h>\n#define ESP_LOGI(t,...) do { (void)(t); if(0) printf(__VA_ARGS__); } while(0)\n#define ESP_LOGW ESP_LOGI\n#define ESP_LOGE ESP_LOGI\n")
    (tmp/"esp_system.h").write_text("#pragma once\nvoid esp_restart(void);\n")
    (tmp/"nvs_flash.h").write_text("#pragma once\n#include \"esp_err.h\"\nesp_err_t nvs_flash_erase(void);\n")
    (tmp/"freertos").mkdir()
    (tmp/"freertos/FreeRTOS.h").write_text("#pragma once\n#include <stdint.h>\n#define pdMS_TO_TICKS(ms) (ms)\n")
    (tmp/"freertos/task.h").write_text("#pragma once\nvoid vTaskDelay(uint32_t ticks);\n")
    code=""
    for c in cases:
        if c['api']=='ui_popup_add_footer_action':
            code+=f'footer({c["text"]},{c["w"]});\n';continue
        make=f'ui_button_create(lv_screen_active(),UI_BUTTON_OUTLINED,{c["text"]})'
        if c['icon']:make=f'ui_button_create_icon(lv_screen_active(),UI_BUTTON_OUTLINED,{c["icon"]},{c["text"]},UI_TEXT,UI_BUTTON_ICON_HORIZONTAL)'
        where=json.dumps(c['file']+':'+str(c['line']))
        code+=f'b={make};lv_obj_set_size(b,{c["w"]},{c["h"]});fit(b,{where});lv_obj_delete(b);\n'
    (tmp/"button_cases.inc").write_text(code)
    source=(root/"main/ui_settings.c").read_text()
    (tmp/"settings_cases.inc").write_text(''.join(f'row({",".join(a[1:4])});\n' for _,_,a in calls(source,'ui_settings_section_add_action_row')))
    source=(root/"main/ui_calibration.c").read_text()
    start=source.index('static void layout_bed_geometry_actions(')
    (tmp/"bed_actions.inc").write_text(source[start:source.index('static void refresh_capabilities',start)])
    sources=["ui_printer_info_cards.c","ui_settings_components.c","ui_printer_actions.c","ui_command_bar.c","ui_calibration_layout.c","ui_popup.c","ui_button.c","ui_theme.c","ui_theme_a.c","ui_theme_b.c","ui_theme_c.c","ui_theme_studio.c","ui_studio_icons.c","assets/fonts/studio/inter_18.c","assets/fonts/studio/inter_20.c","assets/fonts/studio/inter_24.c","assets/fonts/studio/inter_28.c","assets/fonts/studio/inter_32.c","assets/fonts/studio/inter_48.c","assets/fonts/studio/inter_64.c","assets/fonts/studio/inter_96.c","ui_font_fallback.c","ui_text.c","ui_widgets.c","console_filter.c","macro_parameter_utils.c","ui_page_title.c","ui_page_layout_profile.c","printer_controller.c"]
    executable=tmp/"button_text_fit"
    subprocess.run(["cc","-std=c11","-D_POSIX_C_SOURCE=200809L","-O2","-Wall","-Wextra","-Werror","-Wrestrict","-ffunction-sections","-fdata-sections",*flags,
        "-I",str(tmp),"-I",str(lvgl),"-I",str(lvgl/"src"),"-I",str(root/"main"),
        str(root/"tools/audit/button_text_fit_test.c"),
        str(lvgl/"src/font/lv_font_montserrat_12.c"),*[str(root/"main"/s) for s in sources],
        str(library),"-Wl,--gc-sections","-lm","-o",str(executable)],check=True)
    subprocess.run([str(executable)],check=True)
