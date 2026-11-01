#!/usr/bin/env python3
"""Fail when a production UI module has no reviewed native-host owner."""
from pathlib import Path
import re
root=Path(__file__).resolve().parents[3]
production=set(re.findall(r'"(ui_[^"/]+\.c)"',(root/'main/CMakeLists.txt').read_text()))
source=(root/'tools/theme-maker/linux/native/build.py').read_text()+'\n'+'\n'.join(p.read_text() for p in (root/'tools/theme-maker/linux/native').glob('*.c'))
# Boot presentation and display instrumentation are not interactive page flows.
excluded={'ui_splash.c','ui_logo_assets.c','ui_graphics_diagnostics.c'}
missing=sorted(name for name in production-excluded if name not in source)
assert not missing,'New firmware UI has no native owner: '+', '.join(missing)
settings=(root/'tools/theme-maker/linux/native/settings_bridge.c').read_text()
assert '@setting:' not in settings,'Settings still sends a workflow to browser reconstruction'
assert '#include "ui_setup_wizard.c"' in source
assert "'ui_case_lighting.c'" in source and "'ui_ota_release_browser.c'" in source
print(f'PASS: {len(production-excluded)} production UI modules mapped to native owners; three boot/instrumentation exclusions; no Settings browser workflow routes')
