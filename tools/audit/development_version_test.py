#!/usr/bin/env python3
"""Check version-source validation, development release rejection and CMake patterns.
No firmware build, network access or release publication is performed.
"""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
FILES = [
    'version.txt', 'CMakeLists.txt', 'main/idf_component.yml',
    'main/ui_splash.c', 'main/ui_dashboard.c', 'main/ui_settings.c',
    'main/moonraker_live_websocket.c', 'README.md', 'docs/README.md',
    'docs/ARCHITECTURE.md', 'docs/BUILDING.md', 'docs/HARDWARE.md',
    'docs/PROJECT_FILE_CATALOG.md', 'docs/history/README.md', 'CHANGELOG.md',
    'docs/releases/v7.0.0-dev.md', 'tools/audit/version_audit.sh',
    'tools/release_stable.sh',
]


def run(repo, *args):
    return subprocess.run(args, cwd=repo, text=True, capture_output=True)


cmake = (ROOT / 'CMakeLists.txt').read_text()
patterns = []
for name in ['STABLE', 'DEVELOPMENT', 'NIGHTLY']:
    match = re.search(r'set\(PRINTERHMI_' + name + r'_VERSION_PATTERN\s+"([^"]+)"', cmake)
    assert match, name
    patterns.append(match.group(1).replace('\\\\', '\\'))
for value, expected in [('7.0.0-dev', True), ('7.0.0', True),
                        ('nightly-2026-10-11-abcdef', True),
                        ('7.0.0-dev-extra', False), ('7.0', False),
                        ('7.0.0-rc1', False)]:
    assert any(re.fullmatch(pattern, value) for pattern in patterns) == expected, value

with tempfile.TemporaryDirectory(prefix='development-version-') as directory:
    repo = Path(directory)
    assert run(repo, 'git', 'init', '--quiet', '-b', 'main').returncode == 0
    for name in FILES:
        destination = repo / name
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(ROOT / name, destination)
    result = run(repo, 'bash', 'tools/audit/version_audit.sh')
    assert result.returncode == 0, result.stdout + result.stderr
    assert run(repo, 'git', 'add', '--all').returncode == 0
    assert run(repo, 'git', '-c', 'user.name=Version Fixture', '-c',
               'user.email=version@example.invalid', 'commit', '-qm', 'Version fixture').returncode == 0
    result = run(repo, 'bash', 'tools/release_stable.sh')
    assert result.returncode != 0 and 'development firmware cannot be published as stable' in result.stderr
    assert not run(repo, 'git', 'status', '--porcelain').stdout.strip()
    assert not run(repo, 'git', 'tag').stdout.strip()
    readme = repo / 'README.md'
    prior = readme.read_text()
    readme.write_text(prior.replace('- Firmware version: `7.0.0-dev`', '- Firmware version: `6.5.6`'))
    result = run(repo, 'bash', 'tools/audit/version_audit.sh')
    assert result.returncode != 0 and 'README.md is missing current version' in result.stderr
    readme.write_text(prior)
    for invalid in ['7.0', '7.0.0-dev-extra', '7.0.0-rc1']:
        (repo / 'version.txt').write_text(invalid + '\n')
        result = run(repo, 'bash', 'tools/audit/version_audit.sh')
        assert result.returncode != 0 and 'not an X.Y.Z or X.Y.Z-dev version' in result.stderr
    # Final-release audit support; this fixture never runs stable publishing.
    (repo / 'version.txt').write_text('7.0.0\n')
    for name in FILES:
        if name.endswith('.md'):
            path = repo / name
            path.write_text(path.read_text().replace('7.0.0-dev', '7.0.0'))
    shutil.copy2(repo / 'docs/releases/v7.0.0-dev.md', repo / 'docs/releases/v7.0.0.md')
    result = run(repo, 'bash', 'tools/audit/version_audit.sh')
    assert result.returncode == 0, result.stdout + result.stderr

print('PASS: development/final version audits, header drift, malformed identities, stable publication rejection and CMake patterns')
print('CMake patterns checked; actual CMake/ESP-IDF build remains a target validation step.')
