#!/usr/bin/env python3
"""Keep this independent integration branch free of experimental viewer code."""
from pathlib import Path
import re
import tempfile

ROOT = Path(__file__).resolve().parents[2]
FORBIDDEN = ('gcode_toolpath', 'gcode_cache', 'gcode_surface', 'gcode_volume',
             'gcode_viewer_loader', 'ui_gcode_viewer', 'mesh_preview',
             'mesh_preview_loader', 'ui_mesh_preview')
HOOKS = re.compile(r'\b(?:ui_gcode_viewer_\w+|ui_mesh_preview_\w+|ui_(?:tools|files)_set_viewer_callback|(?:files|tools)_open_viewer_bridge)\b|3D (?:G-code Viewer|view)')

def problems(root):
    errors = []
    main = root / 'main'
    for name in FORBIDDEN:
        for suffix in ('.c', '.h'):
            if (main / (name + suffix)).exists():
                errors.append('experimental source: ' + name + suffix)
    for path in main.rglob('*'):
        if path.suffix in ('.c', '.h', '.txt') and HOOKS.search(path.read_text()):
            errors.append('experimental route: ' + str(path.relative_to(root)))
    for name in ('tools/gcode-viewer', 'tools/mesh-preview', 'tools/theme-maker'):
        if (root / name).exists():
            errors.append('experimental tooling: ' + name)
    return errors

with tempfile.TemporaryDirectory(prefix='non-viewer-boundary-') as folder:
    root = Path(folder)
    (root / 'main').mkdir()
    (root / 'main/main.c').write_text('void print_gcode(void) {}\n')
    assert not problems(root)
    path = root / 'main/gcode_volume.c'
    path.write_text('')
    assert any('experimental source' in e for e in problems(root))
    path.unlink()
    (root / 'main/main.c').write_text('ui_files_set_viewer_callback(open_viewer);\n')
    assert any('experimental route' in e for e in problems(root))
    (root / 'main/main.c').write_text('')
    (root / 'tools/gcode-viewer').mkdir(parents=True)
    assert any('experimental tooling' in e for e in problems(root))
errors = problems(ROOT)
if errors:
    raise SystemExit('\n'.join(errors))
print('PASS: independent branch has no experimental viewer sources, routing hooks or tooling; negative fixtures checked')
