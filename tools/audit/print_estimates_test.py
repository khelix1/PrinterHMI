#!/usr/bin/env python3
"""Test print progress sources and independent estimate selection on the host."""
from pathlib import Path
import subprocess
import argparse
import os
import tempfile

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--preferences-source',type=Path,default=root/'main/print_estimate_preferences.c')
args = parser.parse_args()
fixture = r'''
#include <assert.h>
#include <math.h>
#include <stddef.h>
#include "print_estimates.h"
#include "nvs.h"
#include <string.h>
static bool available, fail_commit, demo_enabled;
static int open_error, read_error;
static unsigned opens, reads, closes;
bool demo_mode_enabled(void){return demo_enabled;}
static uint8_t stored, pending;
esp_err_t nvs_open(const char *ns, int mode, nvs_handle_t *out)
{ assert(!strcmp(ns, "hmi")); opens++; if(open_error)return open_error; *out=mode; return ESP_OK; }
esp_err_t nvs_get_u8(nvs_handle_t handle, const char *key, uint8_t *out)
{ assert(handle==NVS_READONLY&&!strcmp(key,"print_estimate")); reads++;if(read_error)return read_error;if(!available)return ESP_ERR_NVS_NOT_FOUND; *out=stored; return ESP_OK; }
esp_err_t nvs_set_u8(nvs_handle_t handle, const char *key, uint8_t value)
{ assert(handle==NVS_READWRITE&&!strcmp(key,"print_estimate")); pending=value; return ESP_OK; }
esp_err_t nvs_commit(nvs_handle_t handle)
{ assert(handle==NVS_READWRITE); if(fail_commit)return ESP_FAIL; stored=pending; available=true; return ESP_OK; }
void nvs_close(nvs_handle_t handle) { (void)handle; closes++; }

int main(int argc,char **argv)
{
    if(argc>1){
        if(!strcmp(argv[1],"demo")){
            demo_enabled=true;assert(print_estimate_options_get().progress==PRINT_PROGRESS_FILE_RELATIVE);
            assert(print_estimate_options_save((print_estimate_options_t){PRINT_PROGRESS_FILAMENT,2,7}));
            assert(print_estimate_options_get().progress==PRINT_PROGRESS_FILAMENT&&!opens&&!reads);
            demo_enabled=false;available=true;stored=PRINT_PROGRESS_SLICER;
            assert(print_estimate_options_get().progress==PRINT_PROGRESS_SLICER&&opens==1);
            demo_enabled=true;assert(print_estimate_options_get().progress==PRINT_PROGRESS_FILAMENT);
        }else{
            if(!strcmp(argv[1],"missing-namespace"))open_error=ESP_ERR_NVS_NOT_FOUND;
            assert(print_estimate_options_get().progress==PRINT_PROGRESS_FILE_RELATIVE);
            unsigned settled_opens=opens,settled_reads=reads;
            open_error=0;available=true;stored=PRINT_PROGRESS_FILAMENT;
            assert(print_estimate_options_get().progress==PRINT_PROGRESS_FILE_RELATIVE);
            assert(opens==settled_opens&&reads==settled_reads);
            assert(print_estimate_options_save((print_estimate_options_t){PRINT_PROGRESS_FILAMENT,2,7}));
            assert(print_estimate_options_get().progress==PRINT_PROGRESS_FILAMENT);
        }
        return 0;
    }
    open_error=ESP_FAIL;
    assert(print_estimate_options_get().progress==PRINT_PROGRESS_FILE_RELATIVE&&opens==1&&!reads&&!closes);
    open_error=0;read_error=ESP_FAIL;
    assert(print_estimate_options_get().progress==PRINT_PROGRESS_FILE_RELATIVE&&opens==2&&reads==1&&closes==1);
    read_error=0;
    available=true; stored=PRINT_PROGRESS_SLICER|(3<<2)|(4<<5);
    print_estimate_options_t saved=print_estimate_options_get();
    assert(saved.progress==PRINT_PROGRESS_SLICER&&saved.remaining_sources==3&&saved.eta_sources==4);
    fail_commit=true;
    assert(!print_estimate_options_save((print_estimate_options_t){PRINT_PROGRESS_FILAMENT,2,7}));
    assert(print_estimate_options_get().progress==PRINT_PROGRESS_SLICER);
    fail_commit=false;
    assert(print_estimate_options_save((print_estimate_options_t){PRINT_PROGRESS_FILAMENT,2,7}));
    assert(stored==(PRINT_PROGRESS_FILAMENT|(2<<2)|(7<<5)));
    assert(print_estimate_options_get().eta_sources==7);

    print_estimate_sample_t sample = {
        .file_progress = .4, .slicer_progress = .6,
        .file_position = 300, .filament_used = 100,
        .file_valid = true, .slicer_valid = true, .position_valid = true, .filament_valid = true,
    };
    print_estimate_metadata_t meta = {.start_byte=100, .end_byte=900, .filament_total=200, .estimated_time=1000};
    print_estimate_options_t opts = {PRINT_PROGRESS_FILE_RELATIVE, PRINT_ESTIMATE_FILE,
        PRINT_ESTIMATE_FILAMENT | PRINT_ESTIMATE_SLICER};
    print_estimate_result_t result = print_estimate_calculate(opts, &sample, &meta, 200);
    assert(result.progress == .25 && result.remaining == 600 && result.eta_remaining == 500);
    opts.progress = PRINT_PROGRESS_FILE_ABSOLUTE;
    result = print_estimate_calculate(opts, &sample, &meta, 200);
    assert(result.progress == .4 && result.remaining == 300);
    opts.progress = PRINT_PROGRESS_SLICER;
    assert(print_estimate_calculate(opts, &sample, &meta, 200).progress == .6);
    opts.progress = PRINT_PROGRESS_FILAMENT;
    assert(print_estimate_calculate(opts, &sample, &meta, 200).progress == .5);
    sample.filament_used = 300;
    result = print_estimate_calculate(opts, &sample, &meta, 200);
    assert(result.progress == 1 && result.remaining == -1 && result.eta_remaining == 800);
    opts.progress = PRINT_PROGRESS_FILE_RELATIVE;
    assert(print_estimate_calculate(opts, &sample, NULL, 200).progress == .4);
    sample.file_position = 99;
    assert(print_estimate_calculate(opts, &sample, &meta, 200).progress == 0);
    sample.file_position = 950;
    assert(print_estimate_calculate(opts, &sample, &meta, 200).progress == 1);
    sample.file_position = 300;
    meta.start_byte = -1;
    assert(print_estimate_calculate(opts, &sample, &meta, 200).progress == .4);
    opts.remaining_sources = opts.eta_sources = 0;
    result = print_estimate_calculate(opts, &sample, &meta, 200);
    assert(result.progress == .4 && result.remaining == -1 && result.eta_remaining == -1);
    opts.eta_sources = PRINT_ESTIMATE_SLICER;
    assert(print_estimate_calculate(opts, &sample, &meta, NAN).eta_remaining == -1);
    assert(print_estimate_calculate(opts, &sample, &meta, 2000).eta_remaining == -1);
    opts.progress = PRINT_PROGRESS_SLICER;
    sample.slicer_valid = false;
    assert(print_estimate_calculate(opts, &sample, &meta, 200).progress == -1);
    opts.progress = PRINT_PROGRESS_FILAMENT;
    assert(print_estimate_calculate(opts, &sample, NULL, 200).progress == -1);
}
'''
with tempfile.TemporaryDirectory() as tmp:
    source = Path(tmp) / 'test.c'
    exe = Path(tmp) / 'test'
    source.write_text(fixture)
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-fno-pie', '-no-pie', '-DPRINTERHMI_DEMO_RUNTIME=1', '-I', str(root / 'main'),
                    '-I', str(root / 'tools/theme-maker/linux/native/shims'),
                    str(source), str(root / 'main/print_estimates.c'),
                    str(args.preferences_source),
                    '-lm', '-o', str(exe)], check=True)
    for scenario in [[],['missing-namespace'],['missing-key'],['demo']]:
        subprocess.run([str(exe),*scenario], check=True, env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
print('PASS: transient preference-load retry, settled missing defaults, save rollback, demo isolation, four progress modes, source averages and unavailable inputs')
