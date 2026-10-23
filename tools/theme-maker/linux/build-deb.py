#!/usr/bin/env python3
"""Build a Linux Mint 22 / Ubuntu 24.04 desktop package without modifying the host."""
from pathlib import Path
import argparse
import shutil
import subprocess
import tempfile

VERSION = '0.1.0~preview27'
APP = 'printerhmi-theme-maker'
BASE = Path(__file__).resolve().parent.parent
ROOT = BASE.parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output-dir', type=Path, default=ROOT / 'dist')
parser.add_argument('--native-helper', type=Path, default=ROOT / 'build-theme-maker-native/printerhmi-ui-host')
args = parser.parse_args()
if not args.native_helper.is_file(): parser.error('Build linux/native/build.py first; the native helper is required.')
if subprocess.check_output([str(args.native_helper.resolve()), '--version'], stdin=subprocess.DEVNULL, text=True, timeout=5).strip() != 'printerhmi-ui-host 16': parser.error('Rebuild the native helper for this version.')
architecture = subprocess.check_output(['dpkg', '--print-architecture'], text=True).strip()
args.output_dir.mkdir(parents=True, exist_ok=True)
output = args.output_dir.resolve() / (APP + '_' + VERSION + '_' + architecture + '.deb')
with tempfile.TemporaryDirectory(prefix=APP + '-deb-') as temporary:
    stage = Path(temporary)
    editor = stage / 'usr/share' / APP
    (editor / 'linux').mkdir(parents=True)
    for name in ['index.html', 'maker.css', 'app.js', 'project-model.js', 'tablet-model.js', 'studio-scene.js', 'theme-model.js', 'theme-spec.js', 'README.md', 'TABLET_PREVIEW.md']:
        shutil.copyfile(BASE / name, editor / name)
    shutil.copytree(BASE / 'assets', editor / 'assets')
    for name in ['theme-maker.py', 'desktop_io.py', 'artwork.py', 'projects.py', 'native_preview.py']:
        shutil.copyfile(BASE / 'linux' / name, editor / 'linux' / name)
    helper = stage / 'usr/lib' / APP / 'printerhmi-ui-host'
    helper.parent.mkdir(parents=True)
    shutil.copyfile(args.native_helper, helper)
    helper.chmod(0o755)
    launcher = stage / 'usr/bin' / APP
    launcher.parent.mkdir(parents=True)
    launcher.write_text('#!/bin/sh\nexec /usr/bin/python3 /usr/share/printerhmi-theme-maker/linux/theme-maker.py "$@"\n')
    launcher.chmod(0o755)
    desktop = stage / 'usr/share/applications'
    desktop.mkdir(parents=True)
    shutil.copyfile(BASE / 'linux' / (APP + '.desktop'), desktop / (APP + '.desktop'))
    icons = stage / 'usr/share/icons/hicolor/scalable/apps'
    icons.mkdir(parents=True)
    shutil.copyfile(BASE / 'linux' / (APP + '.svg'), icons / (APP + '.svg'))
    docs = stage / 'usr/share/doc' / APP
    docs.mkdir(parents=True)
    shutil.copyfile(ROOT / 'LICENSE', docs / 'copyright')
    shutil.copyfile(BASE / 'linux' / 'README.md', docs / 'README.md')
    shutil.copyfile(BASE / 'linux/native/vendor/cjson/LICENSE', docs / 'cjson-copyright')
    (stage / 'DEBIAN').mkdir()
    size = sum(p.stat().st_size for p in (stage / 'usr').rglob('*') if p.is_file())
    (stage / 'DEBIAN/control').write_text(f'''Package: {APP}
Version: {VERSION}
Architecture: {architecture}
Section: graphics
Priority: optional
Maintainer: PrinterHMI project <noreply@github.com>
Depends: libc6 (>= 2.34), python3 (>= 3.10), python3-gi, python3-pil, gir1.2-gtk-3.0, gir1.2-webkit2-4.1 (>= 2.40)
Installed-Size: {(size + 1023) // 1024}
Homepage: https://github.com/khelix1/PrinterHMI
Description: Offline desktop theme designer for PrinterHMI (development preview)
 GTK desktop application with a bundled WebKit editor, local drafts,
 palette and corner controls, page previews and native theme exports.
''')
    subprocess.run(['dpkg-deb', '--root-owner-group', '--build', str(stage), str(output)], check=True)
print(output)
