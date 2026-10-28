#!/usr/bin/env python3
"""Exercise actual profile-switch bridges for reset/restore ordering with SDK mocks."""
from pathlib import Path
import argparse
import os
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--main-source', type=Path, default=root / 'main/main.c')
args = parser.parse_args()
source = args.main_source.read_text()
masked = re.sub(r'/\*[\s\S]*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'', lambda m: ' ' * len(m[0]), source)
def function(name):
    match = re.search(r'static void ' + name + r'\s*\([^;]*?\)\s*\{', masked)
    assert match, name
    end, depth = match.end(), 1
    while depth:
        depth += (masked[end] == '{') - (masked[end] == '}')
        end += 1
    return source[match.start():end]

code = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static bool s_moonraker_ok, visible, cache_available=true, select_ok=true;
static unsigned resets, runtime_resets, restores, reconciles, transport_changes, tests, hides, creates;
static int active_profile;
static char moonraker_status[128];
#define UI_SHELL_PAGE_DASHBOARD 0
static void reset_preview_state_for_host_change(void){resets++;visible=false;}
static void reset_active_printer_runtime_state(void){assert(resets==1&&!visible);runtime_resets++;}
static void dashboard_restore_active_profile_preview(void){assert(resets==1||!runtime_resets);restores++;visible=cache_available;}
static void ui_network_set_port(int p){assert(p==7125);}
static int moonraker_config_port(void){return 7125;}
static void safe_copy(char *out,size_t size,const char *value){snprintf(out,size,"%s",value);}
static void test_moonraker_now(void){tests++;}
static void ui_printer_popups_close_all(void){}
static uint32_t moonraker_config_generation(void){return 9;}
static void moonraker_live_websocket_prepare_profile_change(uint32_t generation){assert(generation==9);transport_changes++;}
static const char *moonraker_config_active_profile_name(void){return "Printing printer";}
static void ui_shell_set_active_printer_name(const char *name){assert(!strcmp(name,"Printing printer"));}
static void printer_profile_health_reconcile(void){reconciles++;}
static void printer_profile_preview_worker_reset(void){}
static void printer_preview_store_reset_restore(void){}
static void ui_printer_chooser_refresh(void){}
static void ui_dashboard_refresh_camera(void){}
static int moonraker_config_active_profile_index(void){return active_profile;}
static bool moonraker_config_select_profile(int profile){if(!select_ok)return false;active_profile=profile;return true;}
static void ui_printer_chooser_hide(void){hides++;}
static void ui_dashboard_create(void){creates++;}
static void ui_shell_set_active_nav(int page){assert(page==UI_SHELL_PAGE_DASHBOARD);}
'''
code += '\n'.join(function(n) for n in ['moonraker_configuration_changed', 'printer_profiles_active_changed_bridge', 'printer_chooser_select_bridge'])
code += r'''
static void reset_counts(void){resets=runtime_resets=restores=reconciles=transport_changes=tests=hides=creates=0;}
int main(void){
 for(unsigned cached=0;cached<2;cached++){
  reset_counts();active_profile=0;visible=true;s_moonraker_ok=true;cache_available=cached;
  printer_chooser_select_bridge(1);
  assert(active_profile==1&&resets==1&&runtime_resets==1&&reconciles==1&&transport_changes==1);
  assert(visible==(bool)cached&&!s_moonraker_ok&&!tests&&hides==1&&creates==1);
  assert(strstr(moonraker_status,"Printing printer"));
 }
 reset_counts();cache_available=true;printer_chooser_select_bridge(1);
 assert(!resets&&!runtime_resets&&!transport_changes&&visible&&hides==1&&creates==1);
 reset_counts();select_ok=false;printer_chooser_select_bridge(2);
 assert(active_profile==1&&!resets&&!runtime_resets&&!hides&&!creates);
 reset_counts();moonraker_configuration_changed("Retest",true);assert(resets==1&&tests==1&&!visible);
 puts("PASS: actual switch bridges reset once before profile-cache restore, preserve health/retest policy, handle missing cache, reselect and failed selection");
}
'''
with tempfile.TemporaryDirectory(prefix='printer-switch-refresh-') as directory:
    path = Path(directory)
    (path / 'switch.c').write_text(code)
    exe = path / 'switch'
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-fno-pie', '-no-pie', str(path / 'switch.c'), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True, env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0'})
