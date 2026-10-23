#!/usr/bin/env python3
"""Build the Linux LVGL helper directly from this checkout's firmware UI sources."""
from pathlib import Path
import argparse,concurrent.futures,re,subprocess,tempfile
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
# The Printer assembler still lives in main.c. Extract its complete functions
# at build time so layout and callback changes remain shared with firmware.
def firmware_function(name):
 text=(root/'main/main.c').read_text()
 match=re.search(r'^(?:static )?(?:void|const char \*) '+re.escape(name)+r'\([^;]*?\)\n\{.*?^\}',text,re.M|re.S)
 if not match: raise RuntimeError('Firmware function not found: '+name)
 return match.group(0)+'\n'
(out/'printer_builder.inc').write_text(''.join(firmware_function(name) for name in ['filament_sensor_banner_event_cb','part_fan_card_event_cb','nozzle_card_event_cb','bed_card_event_cb','printer_motion_send_gcode_bridge','show_motion_popup','printer_ui_controller_show_cancel_bridge','printer_ui_controller_show_object_bridge','ui_printer_create','ui_printer_destroy']))
(out/'drybox_status.inc').write_text(firmware_function('dashboard_dry_status_event_cb'))
filament=(root/'main/moonraker.c').read_text()
match=re.search(r'^moonraker_filament_status_t moonraker_filament_state_status\([^;]*?\)\n\{.*?^\}',filament,re.M|re.S)
if not match:raise RuntimeError('Filament status helper not found')
(out/'filament_status.inc').write_text(match.group(0)+'\n')
# Keep the real lightbox rendering/close implementation. Replace only the
# asynchronous HTTP fetch entry with a local image fixture adapter.
lightbox=(root/'main/ui_preview_lightbox.c').read_text().split('static bool fullscreen_job_is_current')[0]
for include in ['bsp/esp-bsp.h','freertos/FreeRTOS.h','freertos/queue.h','freertos/task.h']:
 lightbox=lightbox.replace('#include "'+include+'"','')
lightbox=lightbox.replace('static QueueHandle_t s_preview_queue;','').replace('static bool s_preview_worker_started;','')
lightbox=re.sub(r'typedef struct \{.*?\} fullscreen_job_t;', '',lightbox,flags=re.S)
lightbox+='\nvoid ui_preview_lightbox_show_file_object(lv_obj_t *obj,const char *file){(void)file;ui_preview_lightbox_show_object(obj); }\n'
(out/'lightbox_local.inc').write_text(lightbox)
flags=['-DLV_CONF_SKIP','-DLV_KCONFIG_IGNORE','-DLV_USE_STDLIB_MALLOC=1']+[f'-DLV_FONT_MONTSERRAT_{n}=1' for n in range(12,34,2)]+['-DLV_FONT_MONTSERRAT_48=1']
lib=args.lvgl_lib.resolve() if args.lvgl_lib else out/'liblvgl.a'
if not args.lvgl_lib:
 def compile_one(source):
  obj=out/(source.relative_to(lvgl).as_posix().replace('/','_')+'.o');subprocess.run(['cc','-std=c11','-O2','-ffunction-sections','-fdata-sections',*flags,'-I',str(lvgl),'-c',str(source),'-o',str(obj)],check=True,capture_output=True);return str(obj)
 with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:objects=list(pool.map(compile_one,(lvgl/'src').rglob('*.c')))
 subprocess.run(['ar','rcs',str(lib),*objects],check=True)
sources=['ui_popup.c','ui_button.c','ui_theme.c','ui_theme_a.c','ui_theme_b.c','ui_theme_c.c','ui_theme_studio.c','ui_studio_icons.c','ui_font_fallback.c','ui_text.c','ui_widgets.c','console_filter.c','macro_parameter_utils.c','ui_page_title.c','ui_page_layout_profile.c','ui_printer_popups.c','ui_printer_toolhead.c','ui_shell.c','ui_global_estop.c','ui_console.c','ui_printer.c','ui_printer_layout.c','ui_printer_info_cards.c','ui_printer_actions.c','ui_printer_motion.c','printer_controller.c','printer_ui_controller.c','ui_printer_banner.c','ui_status_banner.c','ui_thumbnail.c','thumbnail_render.c','ui_filament_recovery.c','ui_dashboard_page.c','ui_dashboard_layout_profile.c','ui_dashboard_status.c','ui_active_print.c','ui_machine_status.c','ui_command_bar.c','dashboard_live_controller.c','ui_files.c','ui_page_state.c','ui_drybox.c','ui_drybox_page.c','ui_toast.c']+['assets/fonts/studio/inter_'+str(n)+'.c' for n in [18,20,24,28,32,48,64,96]]
compiler=['cc','-include',str(here/'shims/host_compat.h'),'-std=c11','-D_POSIX_C_SOURCE=200809L','-O2','-Wall','-Wextra','-Werror','-ffunction-sections','-fdata-sections',*flags,'-I',str(here/'shims'),'-I',str(out),'-I',str(lvgl),'-I',str(lvgl/'src'),'-I',str(root/'main')]
source_files=[here/name for name in ['host.c','tuning_bridge.c','page_bridge.c','console_fixture.c','printer_bridge.c','dashboard_bridge.c','files_bridge.c','camera_bridge.c','profiles_bridge.c','camera_fixture.c','drybox_bridge.c','notification_fixture.c']]+[root/'main'/name for name in sources]
def compile_host(source):
 obj=out/('host_'+source.relative_to(root).as_posix().replace('/','_')+'.o')
 result=subprocess.run([*compiler,'-c',str(source),'-o',str(obj)],capture_output=True,text=True)
 if result.returncode:raise RuntimeError(result.stderr)
 return str(obj)
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:objects=list(pool.map(compile_host,source_files))
# Publish an executable atomically, including when an existing copy is read-only.
with tempfile.TemporaryDirectory(prefix='link-',dir=out) as temporary:
 helper=Path(temporary)/'printerhmi-ui-host'
 subprocess.run(['cc',*objects,str(lib),'-Wl,--gc-sections','-lm','-o',str(helper)],check=True)
 helper.chmod(0o755)
 helper.replace(out/'printerhmi-ui-host')
print(out/'printerhmi-ui-host')
