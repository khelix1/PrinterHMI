# PrinterHMI v3.2 Crash Log

Purpose:
Track every crash, every successful change, and every suspected cause.

Rules:
- One functional change per test.
- Freeze before every significant modification.
- Record exactly which files changed.
- Verify OTA image before blaming code.
- Never refactor while debugging crashes.

Current Stable Baseline:
v32_dashboard_default_live_machine_ota_stable

Verified:
- v3.2 dashboard is default startup dashboard
- Legacy shell retained
- Live Machine Status works
- OTA works
- Active Print card works
- Preview module exists
- Thumbnail Manager exists

Known Safe Changes:
- Hardcoded TEST footer: ui_dashboard_v32.c
- Replace TEST with 7/88: ui_dashboard_v32.c
- Restore idle footer: ui_dashboard_v32.c
- Default startup equals v3.2 dashboard: main.c

Known Unsafe / Suspicious Changes:
- Export dashboard Active Print API: ui_dashboard_v32.c/h
- Add dashboard refresh helper: ui_dashboard_v32.c
- Large refresh callback changes: main.c
- Multiple ownership changes at once

Crash Signature:
assert failed:
vApplicationGetTimerTaskMemory
port_common.c:97
(pxStackBufferTemp != NULL)

OTA Checklist:
sha256sum build/PrinterHMI_v3_2.bin
curl -s http://192.0.2.119:8000/PrinterHMI_v3_2.bin -o /tmp/ota.bin
sha256sum /tmp/ota.bin

Working Philosophy:
Small changes first.
One change, one build, one OTA, one proof.

Future Test Log:
Date | Change | Files | Build | OTA | Boot | Result | Notes
