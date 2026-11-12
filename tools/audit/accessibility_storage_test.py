#!/usr/bin/env python3
"""Exercise production grouped accessibility persistence with staged NVS faults."""
from pathlib import Path
import argparse,os,subprocess,tempfile
root=Path(__file__).resolve().parents[2]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--source',type=Path,default=root/'main/theme_manager.c')
source=parser.parse_args().source.read_text()
def function(name):
 at=source.index(name+'(');return source[source.rfind('\n',0,at)+1:source.index('\n}\n',at)+3]
# Retain the legacy helper when testing the preceding implementation.
production=(function('save_u8')+'\n' if 'save_u8(THEME_NVS_LARGE_TEXT_KEY' in function('theme_manager_set_accessibility') else '')+function('theme_manager_set_accessibility')
fixture=r'''
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef int esp_err_t,nvs_handle_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define NVS_READWRITE 1
#define THEME_NVS_NAMESPACE "ui_theme"
#define THEME_NVS_LARGE_TEXT_KEY "large_text"
#define THEME_NVS_CONTRAST_KEY "contrast"
#define THEME_NVS_SOLID_KEY "solid_glass"
#define THEME_NVS_MOTION_KEY "reduce_motion"
#define ESP_LOGE(t,...) do{(void)(t);if(0)printf(__VA_ARGS__);}while(0)
static const char TAG[]="fixture";
typedef struct {bool large_text,high_contrast,reduced_transparency,reduced_motion;} ui_accessibility_t;
static uint8_t saved[4],pending[4];
static ui_accessibility_t current;
static unsigned opens,closes,sets,commits,applied,fail_set;
static bool fail_open,fail_commit;
static const char *esp_err_to_name(int e){(void)e;return "fixture";}
static int nvs_open(const char *ns,int mode,nvs_handle_t *h){assert(!strcmp(ns,"ui_theme")&&mode==NVS_READWRITE);opens++;if(fail_open)return ESP_FAIL;*h=1;memcpy(pending,saved,sizeof(saved));return ESP_OK;}
static int nvs_set_u8(nvs_handle_t h,const char *k,uint8_t value){assert(h==1);sets++;if(sets==fail_set)return ESP_FAIL;const char *keys[]={"large_text","contrast","solid_glass","reduce_motion"};for(unsigned i=0;i<4;i++)if(!strcmp(k,keys[i])){pending[i]=value;return ESP_OK;}assert(0);return ESP_FAIL;}
static int nvs_commit(nvs_handle_t h){assert(h==1);commits++;if(fail_commit)return ESP_FAIL;memcpy(saved,pending,sizeof(saved));return ESP_OK;}
static void nvs_close(nvs_handle_t h){assert(h==1);closes++;}
static void ui_theme_set_accessibility(ui_accessibility_t options){assert(saved[0]==options.large_text&&saved[1]==options.high_contrast&&saved[2]==options.reduced_transparency&&saved[3]==options.reduced_motion);current=options;applied++;}
/* PRODUCTION */
static void reset(void){memset(saved,0,sizeof(saved));memset(pending,0,sizeof(pending));memset(&current,0,sizeof(current));opens=closes=sets=commits=applied=fail_set=0;fail_open=fail_commit=false;}
int main(void){
 ui_accessibility_t requested={true,true,true,true};
 const unsigned failures[]={3,0,1,2,4,5};
 for(unsigned test=0;test<6;test++){
  unsigned failure=failures[test];
  reset();fail_open=failure==0;fail_set=failure>=1&&failure<=4?failure:0;fail_commit=failure==5;
  assert(!theme_manager_set_accessibility(requested));
  assert(!applied&&!current.large_text&&!current.high_contrast&&!current.reduced_transparency&&!current.reduced_motion);
  for(unsigned i=0;i<4;i++)assert(!saved[i]);
  assert(opens==1&&closes==(fail_open?0:1));assert(commits==(fail_commit?1:0));
  fail_open=fail_commit=false;fail_set=0;opens=closes=sets=commits=0;
  assert(theme_manager_set_accessibility(requested)&&opens==1&&closes==1&&sets==4&&commits==1&&applied==1);
 }
 reset();requested=(ui_accessibility_t){false,true,false,true};assert(theme_manager_set_accessibility(requested));
 assert(!saved[0]&&saved[1]&&!saved[2]&&saved[3]&&commits==1&&applied==1);
 puts("PASS: production accessibility group, open/each-key/commit faults, no earlier commits, unchanged live options, one-commit retry and mixed selections");
}
'''.replace('/* PRODUCTION */',production)
with tempfile.TemporaryDirectory(prefix='accessibility-storage-') as directory:
 p=Path(directory);(p/'test.c').write_text(fixture)
 subprocess.run(['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-no-pie',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
