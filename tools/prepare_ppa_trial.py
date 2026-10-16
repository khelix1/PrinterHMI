#!/usr/bin/env python3
"""Seed/verify the default fill-only PPA config; retain sdkconfig.idf6 for software fallback."""
from pathlib import Path
import re
import sys

COMMON = {
    'CONFIG_IDF_TARGET': '"esp32p4"',
    'CONFIG_CACHE_L2_CACHE_LINE_SIZE': '64',
    'CONFIG_BSP_LCD_DPI_BUFFER_NUMS': '2',
    'CONFIG_BSP_DISPLAY_LVGL_AVOID_TEAR': 'y',
    'CONFIG_BSP_DISPLAY_LVGL_DIRECT_MODE': 'y',
    'CONFIG_LV_COLOR_DEPTH': '16',
    'CONFIG_LV_DRAW_BUF_STRIDE_ALIGN': '1',
    'CONFIG_LV_DRAW_SW_DRAW_UNIT_CNT': '1',
    'CONFIG_LVGL_PORT_ENABLE_PPA': 'n',
}
TRIAL = {'CONFIG_LV_USE_PPA': 'y', 'CONFIG_LV_USE_PPA_IMG': 'n',
         'CONFIG_LV_DRAW_BUF_ALIGN': '64', 'CONFIG_LV_PPA_BURST_LENGTH': '128'}

def options(text):
    result = {}
    for line in text.splitlines():
        match = re.fullmatch(r'(CONFIG_[A-Z0-9_]+)=(.*)', line)
        off = re.fullmatch(r'# (CONFIG_[A-Z0-9_]+) is not set', line)
        if not match and not off: continue
        key, value = match.groups() if match else (off[1], 'n')
        if key in result and result[key] != value: raise RuntimeError('Conflicting config option: ' + key)
        result[key] = value
    return result

def require(values, expected):
    for key, value in expected.items():
        if values.get(key) != value:
            raise RuntimeError(f'{key}: expected {value}, found {values.get(key)}; no configuration written')

def seed(source, output):
    if source.is_symlink() or output.is_symlink(): raise RuntimeError('Refusing symlinked config')
    if source.resolve() == output.resolve(): raise RuntimeError('Trial config must be separate from baseline')
    text = source.read_text()
    values = options(text)
    require(values, COMMON)
    require(values, {'CONFIG_LV_USE_PPA': 'n', 'CONFIG_LV_DRAW_BUF_ALIGN': '4'})
    lines = []
    for line in text.splitlines():
        if any(line.startswith(key + '=') or line == '# ' + key + ' is not set' for key in TRIAL): continue
        lines.append(line)
    lines += ['# PrinterHMI default PPA fill-only renderer; generated from sdkconfig.idf6']
    lines += [('# ' + key + ' is not set') if value == 'n' else key + '=' + value for key, value in TRIAL.items()]
    generated = '\n'.join(lines) + '\n'
    require(options(generated), {**COMMON, **TRIAL})
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(generated)
    print('PASS: isolated PPA fill-only config prepared; baseline unchanged')

def verify(path):
    require(options(path.read_text()), {**COMMON, **TRIAL})
    print('PASS: effective PPA configuration verified after reconfigure')

if __name__ == '__main__':
    try:
        if len(sys.argv) == 3 and sys.argv[1] == '--verify': verify(Path(sys.argv[2]))
        elif len(sys.argv) == 3: seed(Path(sys.argv[1]), Path(sys.argv[2]))
        else: raise RuntimeError('Usage: prepare_ppa_trial.py source output | --verify config')
    except (OSError, RuntimeError) as error:
        raise SystemExit('ERROR: ' + str(error))
