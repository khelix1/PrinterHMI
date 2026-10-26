#!/usr/bin/env python3
"""Check firmware source membership and explicitly parked host experiments."""
from pathlib import Path
import re
import tempfile

ROOT = Path(__file__).resolve().parents[2]
PARKED = {}  # The separate 3D viewer experiments are not part of this branch.

def problems(root, parked):
    main = root / 'main'
    cmake = (main / 'CMakeLists.txt').read_text()
    listed = re.findall(r'"([^"\n]+\.c)"', cmake)
    errors = []
    for name in sorted(set(listed)):
        if listed.count(name) != 1:
            errors.append('duplicate firmware source: ' + name)
        if not (main / name).is_file():
            errors.append('missing firmware source: ' + name)
    for source in sorted(main.rglob('*.c')):
        name = source.relative_to(main).as_posix()
        if name not in listed and name not in parked:
            errors.append('unclassified source outside firmware: ' + name)
    for name, fixture in parked.items():
        if name in listed or not (main / name).is_file() or not (root / fixture).is_file():
            errors.append('invalid parked experiment/fixture: ' + name)
    # Platform headers are supplied by ESP-IDF/LVGL. ui_* headers belong here.
    for source in sorted(main.rglob('*')):
        if source.suffix not in {'.c', '.h'}:
            continue
        for header in re.findall(r'^\s*#\s*include\s+"(ui_[^"]+)"', source.read_text(), re.M):
            if not (source.parent / header).is_file() and not (main / header).is_file():
                errors.append(f'missing project header: {source.relative_to(root)} -> {header}')
    return errors

def fixture_checks():
    with tempfile.TemporaryDirectory(prefix='source-membership-') as folder:
        root = Path(folder)
        main = root / 'main'
        main.mkdir()
        cmake = main / 'CMakeLists.txt'
        cmake.write_text('idf_component_register(SRCS "one.c")\n')
        (main / 'one.c').write_text('#include "esp_system.h"\n')
        assert not problems(root, {})
        (main / 'extra.c').write_text('')
        assert any('unclassified' in e for e in problems(root, {}))
        cmake.write_text('idf_component_register(SRCS "one.c" "extra.c" "one.c" "gone.c")\n')
        errors = problems(root, {})
        assert any('duplicate' in e for e in errors) and any('missing firmware' in e for e in errors)
        cmake.write_text('idf_component_register(SRCS "one.c" "extra.c")\n')
        (main / 'one.c').write_text('#include "ui_missing.h"\n')
        assert any('missing project header' in e for e in problems(root, {}))
        (main / 'ui_missing.h').write_text('#pragma once\n')
        assert not problems(root, {})

if __name__ == '__main__':
    fixture_checks()
    errors = problems(ROOT, PARKED)
    if errors:
        raise SystemExit('\n'.join(errors))
    count = len(re.findall(r'"([^"\n]+\.c)"', (ROOT / 'main/CMakeLists.txt').read_text()))
    print(f'PASS: {count} firmware sources, {len(PARKED)} classified host experiments, project headers and negative inventory fixtures')
