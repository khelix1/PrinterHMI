#!/usr/bin/env python3
"""Check PPA config isolation, pinned LVGL fix and real buffer allocation alignment."""
import argparse
import hashlib
import importlib.util
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--lvgl-dir', type=Path, required=True)
p.add_argument('--lvgl-lib', type=Path, required=True)
a = p.parse_args()
def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
    return module
config = load('ppa_config', root/'tools/prepare_ppa_trial.py')
patch = load('ppa_patch', root/'tools/apply_lvgl95_ppa_alignment.py')
def refuses(function):
    try: function()
    except RuntimeError: return
    raise AssertionError('Expected rejection')
with tempfile.TemporaryDirectory(prefix='ppa-trial-test-') as directory:
    tmp = Path(directory)
    baseline = (root/'sdkconfig.idf6').read_bytes()
    source = tmp/'sdkconfig.idf6'; source.write_bytes(baseline + b'\nCONFIG_AUDIT_KEEP_ME=17\n')
    original = source.read_bytes()
    output = tmp/'trial'
    config.seed(source, output); config.verify(output)
    assert source.read_bytes() == original and 'CONFIG_AUDIT_KEEP_ME=17' in output.read_text()
    # Only the intended options differ; duplicate identical deprecated options are legal.
    old = config.options(source.read_text()); new = config.options(output.read_text())
    assert {k for k in old.keys() | new.keys() if old.get(k) != new.get(k)} == set(config.TRIAL)
    previous = output.read_bytes()
    source.write_text(original.decode().replace('CONFIG_CACHE_L2_CACHE_LINE_SIZE=64','CONFIG_CACHE_L2_CACHE_LINE_SIZE=128'))
    refuses(lambda: config.seed(source, output)); assert output.read_bytes() == previous
    source.write_bytes(original); refuses(lambda: config.seed(source, source))
    refuses(lambda: config.options('CONFIG_LV_USE_PPA=y\n# CONFIG_LV_USE_PPA is not set\n'))
    output.write_text(previous.decode().replace('# CONFIG_LV_USE_PPA_IMG is not set','CONFIG_LV_USE_PPA_IMG=y'))
    refuses(lambda: config.verify(output)); output.write_bytes(previous)

    lvgl = tmp/'lvgl'; draw = lvgl/'src/draw/lv_draw_buf.c'; draw.parent.mkdir(parents=True)
    pinned = (a.lvgl_dir/'src/draw/lv_draw_buf.c').read_bytes()
    assert hashlib.sha256(pinned).hexdigest() == patch.BEFORE
    draw.write_bytes(pinned)
    patch.apply(lvgl); corrected = draw.read_bytes(); patch.apply(lvgl)
    assert draw.read_bytes() == corrected and hashlib.sha256(corrected).hexdigest() == patch.AFTER
    draw.write_bytes(pinned+b'\n/* other local change */\n'); unexpected = draw.read_bytes()
    refuses(lambda: patch.apply(lvgl)); assert draw.read_bytes() == unexpected
    draw.write_bytes(corrected)
    fixture = tmp/'buffers.c'
    fixture.write_text('''#include "lvgl.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
int main(void) {
    lv_init();
    const lv_color_format_t formats[] = {LV_COLOR_FORMAT_RGB565, LV_COLOR_FORMAT_RGB888,
        LV_COLOR_FORMAT_ARGB8888, LV_COLOR_FORMAT_A8, LV_COLOR_FORMAT_RGB565A8,
        LV_COLOR_FORMAT_I1, LV_COLOR_FORMAT_I8};
    for(unsigned f=0; f<sizeof(formats)/sizeof(formats[0]); ++f)
        for(unsigned w=1; w<42; w+=4) for(unsigned h=1; h<24; h+=3) {
            lv_draw_buf_t *b=lv_draw_buf_create(w,h,formats[f],0);
            assert(b && b->data && (uintptr_t)b->data % 64 == 0 && b->data_size % 64 == 0);
            assert(b->data_size >= b->header.stride * h);
            lv_draw_buf_clear(b, NULL); lv_draw_buf_destroy(b);
        }
    lv_deinit(); puts("PASS: real LVGL odd-size RGB/alpha/indexed buffer addresses and lengths aligned to 64");
}
''')
    flags = ['-DLV_CONF_SKIP','-DLV_KCONFIG_IGNORE','-DLV_USE_STDLIB_MALLOC=1','-DLV_DRAW_BUF_ALIGN=64']
    flags += [f'-DLV_FONT_MONTSERRAT_{n}=1' for n in range(12,34,2)] + ['-DLV_FONT_MONTSERRAT_48=1']
    exe = tmp/'buffers'
    subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror',*flags,
        '-I',str(a.lvgl_dir),'-I',str(a.lvgl_dir/'src/draw'),str(draw),str(fixture),str(a.lvgl_lib),'-lm','-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)

    # Exercise the real wrapper with fake IDF commands; no SDK or physical target is claimed.
    repo = tmp/'repo'; (repo/'tools').mkdir(parents=True)
    subprocess.run(['git','init','-q',str(repo)],check=True)
    (repo/'sdkconfig.idf6').write_bytes(baseline)
    for name in ['build_idf6_hosted3.sh','prepare_ppa_trial.py','apply_lvgl95_ppa_alignment.py','apply_lvgl95_ppa_cache.py']:
        shutil.copy2(root/'tools'/name,repo/'tools'/name)
    for name in ['apply_idf6_esp_hosted_sdio_patch.py','apply_esp_hosted_3_0_5_rssi_patch.py']:
        (repo/'tools'/name).write_text('print("PASS: stub hosted prerequisite")\n')
    component = repo/'managed_components/lvgl__lvgl/src/draw'; component.mkdir(parents=True)
    (component/'lv_draw_buf.c').write_bytes(pinned)
    for name in ['lv_draw_ppa.c','lv_draw_ppa_buf.c']:
        destination=component/'espressif/ppa'/name;destination.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(a.lvgl_dir/'src/draw/espressif/ppa'/name,destination)
    sdk = tmp/'idf'; sdk.mkdir(); binary = tmp/'bin'; binary.mkdir()
    (sdk/'export.sh').write_text('export PATH="'+str(binary)+':$PATH"\n')
    fake = binary/'idf.py'
    fake.write_text('''#!/usr/bin/env python3
import json,os,sys
from pathlib import Path
if sys.argv[1:] == ['--version']: print('ESP-IDF v6.0.2-dirty'); sys.exit(0)
args=sys.argv[1:]; cfg=Path(next(v.split('=',1)[1] for v in args if v.startswith('SDKCONFIG=')))
root=Path.cwd(); trial=os.environ.get('PRINTERHMI_PPA_TRIAL','1')=='1'
assert cfg == root/'build-idf6-hosted3/sdkconfig.ppa-trial' if trial else cfg == root/'sdkconfig.idf6'
if trial:
    assert 'CONFIG_LV_USE_PPA=y' in cfg.read_text()
    if 'build' in args:
        assert 'size = LV_ROUND_UP(size, LV_DRAW_BUF_ALIGN);' in (root/'managed_components/lvgl__lvgl/src/draw/lv_draw_buf.c').read_text()
        ppa=root/'managed_components/lvgl__lvgl/src/draw/espressif/ppa'
        assert 'other_unit_busy(layer, draw_unit)' in (ppa/'lv_draw_ppa.c').read_text()
        assert 'ESP_CACHE_MSYNC_FLAG_DIR_M2C' in (ppa/'lv_draw_ppa_buf.c').read_text()
with (root/'calls.jsonl').open('a') as log: log.write(json.dumps([trial,args[-1],str(cfg),args])+'\\n')
''')
    fake.chmod(0o755)
    for trial in [None,'0','1',None]:
        env = {**os.environ,'PRINTERHMI_IDF6_PATH':str(sdk)}
        env.pop('PRINTERHMI_PPA_TRIAL',None)
        if trial is not None: env['PRINTERHMI_PPA_TRIAL']=trial
        subprocess.run(['bash',str(repo/'tools/build_idf6_hosted3.sh')],cwd=repo,env=env,check=True,capture_output=True,text=True)
        assert (repo/'sdkconfig.idf6').read_bytes() == baseline
    import json
    calls = [json.loads(line) for line in (repo/'calls.jsonl').read_text().splitlines()]
    assert [(c[0],c[1]) for c in calls] == [(True,'reconfigure'),(True,'build'),(False,'reconfigure'),(False,'build'),(True,'reconfigure'),(True,'build'),(True,'reconfigure'),(True,'build')]
    subprocess.run(['bash',str(repo/'tools/build_idf6_hosted3.sh'),'flash','monitor'],cwd=repo,env=env,check=True,capture_output=True,text=True)
    calls = [json.loads(line) for line in (repo/'calls.jsonl').read_text().splitlines()]
    assert calls[-1][0] and calls[-1][1]=='monitor' and calls[-1][3][-3:]==['build','flash','monitor']
    assert calls[-1][2]==calls[-2][2] and (repo/'sdkconfig.idf6').read_bytes()==baseline
    previous=(repo/'calls.jsonl').read_bytes()
    rejected=subprocess.run(['bash',str(repo/'tools/build_idf6_hosted3.sh'),'fullclean'],cwd=repo,env=env,capture_output=True,text=True)
    assert rejected.returncode!=0 and (repo/'calls.jsonl').read_bytes()==previous
    env['PRINTERHMI_PPA_TRIAL']='invalid'
    rejected=subprocess.run(['bash',str(repo/'tools/build_idf6_hosted3.sh')],cwd=repo,env=env,capture_output=True,text=True)
    assert rejected.returncode!=0 and (repo/'calls.jsonl').read_bytes()==previous
    print('PASS: source fixes; isolated config; default PPA/software fallback/default restore; matching build/flash/monitor config and invalid-action/mode refusal')
