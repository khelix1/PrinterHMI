#!/usr/bin/env python3
"""Apply the upstream LVGL 9.5 draw-buffer length alignment correction (#9868/9869)."""
from pathlib import Path
import hashlib
import sys

BEFORE = 'b969e62c4fe8c55797a694bf94d4eb1272533af14f8f1d3449d7223d80256898'
AFTER = '2ce7893f977c63a1b5ec077ebbe592efbc5f4899d328d32dc4b9d99356d49fab'
OLD = b'    return size;\n}\n\nstatic void draw_buf_get_full_area'
NEW = b'    size = LV_ROUND_UP(size, LV_DRAW_BUF_ALIGN);\n    return size;\n}\n\nstatic void draw_buf_get_full_area'

def apply(component):
    path = component / 'src/draw/lv_draw_buf.c'
    if path.is_symlink(): raise RuntimeError('Refusing symlinked LVGL source')
    data = path.read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    if digest == AFTER:
        print('PASS: LVGL 9.5 PPA buffer-length correction already applied')
        return
    if digest != BEFORE or data.count(OLD) != 1:
        raise RuntimeError('LVGL draw-buffer source differs from verified 9.5.0; no source changed')
    updated = data.replace(OLD, NEW)
    if hashlib.sha256(updated).hexdigest() != AFTER: raise RuntimeError('LVGL patch verification failed')
    path.write_bytes(updated)
    print('PASS: applied LVGL 9.5 PPA buffer-length correction')

if __name__ == '__main__':
    try:
        if len(sys.argv) > 2: raise RuntimeError('Usage: apply_lvgl95_ppa_alignment.py [lvgl-component-dir]')
        component = Path(sys.argv[1]) if len(sys.argv) == 2 else Path(__file__).resolve().parents[1] / 'managed_components/lvgl__lvgl'
        apply(component)
    except (OSError, RuntimeError) as error:
        raise SystemExit('ERROR: ' + str(error))
