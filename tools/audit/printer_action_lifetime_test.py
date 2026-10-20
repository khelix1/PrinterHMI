#!/usr/bin/env python3
"""Exercise the real Printer teardown and motion readiness block with LVGL."""
from pathlib import Path
import argparse,re,subprocess,tempfile
root=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--lvgl-dir',type=Path,required=True);p.add_argument('--lvgl-lib',type=Path,required=True);a=p.parse_args()
def block(source,marker):
 start=source.index(marker);opening=source.index('{',start);depth=1;end=opening+1
 while depth:
  if source[end]=='{':depth+=1
  elif source[end]=='}':depth-=1
  end+=1
 return source[start:end]
destroy=block((root/'main/main.c').read_text(),'void ui_printer_destroy(void)\n{')
refresh=block((root/'main/printer_ui_controller.c').read_text(),'if(ctx->printer_panel && ctx->motion_button)')
refs=set(re.findall(r'\b(printer_\w+)\s*=\s*NULL',destroy))
code='''#include "lvgl.h"
#include "ui_printer_actions.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static ui_printer_actions_t printer_actions;
static void ui_printer_preview_destroy_refs(void){}
static void ui_printer_toolhead_close(void){}
static void ui_shell_raise_topbar(void){}
static void ui_shell_raise_nav(void){}
'''+''.join('static lv_obj_t *'+n+';\n' for n in sorted(refs))+destroy+'''
static void deleted(lv_event_t *e){(void)e;assert(!printer_actions.motion&&!printer_actions.home&&!printer_actions.pause&&!printer_actions.resume&&!printer_actions.object&&!printer_actions.cancel);}
static void refresh_motion(lv_obj_t *panel,lv_obj_t *button,const char *reason){
 struct {lv_obj_t *printer_panel,*motion_button;} value={panel,button},*ctx=&value;
'''+refresh+'''
}
int main(void){lv_init();lv_display_t*d=lv_display_create(1024,600);
for(unsigned i=0;i<1000;i++){
 printer_panel=lv_obj_create(lv_screen_active());
 printer_actions.motion=lv_button_create(printer_panel);
 printer_actions.home=lv_button_create(printer_panel);
 printer_actions.pause=lv_button_create(printer_panel);
 printer_actions.resume=lv_button_create(printer_panel);
 printer_actions.object=lv_button_create(printer_panel);
 printer_actions.cancel=lv_button_create(printer_panel);
 lv_obj_add_event_cb(printer_panel,deleted,LV_EVENT_DELETE,NULL);
 refresh_motion(printer_panel,printer_actions.motion,"Offline");assert(lv_obj_has_state(printer_actions.motion,LV_STATE_DISABLED));
 refresh_motion(printer_panel,printer_actions.motion,"");assert(!lv_obj_has_state(printer_actions.motion,LV_STATE_DISABLED));
 lv_obj_t*retired=printer_actions.motion;
 ui_printer_destroy();assert(!printer_panel&&!printer_actions.motion);
 /* Intentionally retain a stale pointer: no page means no LVGL call. */
 refresh_motion(NULL,retired,"Offline");refresh_motion(NULL,retired,"");
 ui_printer_destroy();
}
lv_display_delete(d);lv_deinit();puts("PASS: 1000 Printer/chooser teardown cycles, action refs retired before deletion, offline/reconnect states and absent-page refresh");}
'''
with tempfile.TemporaryDirectory(prefix='printer-action-lifetime-') as t:
 c=Path(t)/'test.c';c.write_text(code);exe=Path(t)/'test'
 subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror','-DLV_CONF_SKIP','-DLV_KCONFIG_IGNORE','-DLV_USE_STDLIB_MALLOC=1','-I',str(a.lvgl_dir),'-I',str(root/'main'),str(c),str(a.lvgl_lib),'-lm','-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
