# PrinterHMI v3.3 Architecture

## Core Principle

Pages are orchestration modules, not implementation modules.

A page owns:
- create
- destroy
- refresh
- layout
- page-local widgets
- page-local callbacks

A page does not own:
- WiFi scan internals
- Moonraker scan internals
- NVS persistence
- OTA
- file-browser internals
- printer command logic
- reusable popups

---

## Layers

Application:
- app_main
- startup
- timers
- tasks
- polling
- global runtime

Shell:
- top bar
- clock
- WiFi indicator
- nav rail
- page selection
- page visibility

Pages:
- ui_dashboard_v32
- ui_printer_v32
- ui_files_v32
- ui_network_v32
- ui_drybox_v32
- ui_settings

Feature/UI Modules:
- ui_network_tools
- ui_file_popup
- ui_printer_actions
- ui_printer_popup
- ui_drybox_actions
- ui_graphs

Service Modules:
- moonraker
- network_service
- ota
- nvs/settings
- history_service
- thumbnail/cache

---

## Phase B: Responsibility Extraction

No behavior changes.

Goal:
- Move code by responsibility.
- Build after every extraction.
- OTA/runtime verify before deleting old code.
- Keep docs current.

Recommended order:
1. Expand `ui_network_tools`.
2. Create `network_service`.
3. Shrink `ui_network_v32`.
4. Extract printer actions/popups.
5. Extract file popup/browser.
6. Extract drybox actions.
7. Extract graphs/history.

---

## Phase C: UI Modernization

After ownership is clean:
- theme polish
- cards
- dashboard redesign
- graph redesign
- animations
- visual consistency

---

## Phase D: Appliance Platform

Future:
- multi-printer support
- multi-dryer support
- profiles
- auto-discovery
- capability abstraction
- plugin-like page/features

---

## Refactor Rules

1. Make the smallest possible change.
2. Build after every extraction.
3. OTA/runtime verify before deleting old code.
4. Move one responsibility at a time.
5. Do not create another giant module.
6. Keep docs and logs synchronized.


---

## Header/API Rule

Headers describe real module APIs, not future intentions.

Do not place a function in a public header until:
- the implementation lives in the owning module, or
- another module actually needs to call it externally.

If a function still lives in `main.c` and is `static`, it must not be declared in a public header.

Ownership rename does not automatically mean public API export.

Correct extraction sequence:

1. Create module.
2. Rename ownership.
3. Build.
4. OTA/runtime verify.
5. Move implementation.
6. Build.
7. OTA/runtime verify.
8. Export API only if needed.
9. Delete bridge only after replacement is verified.
10. Update docs/logs.


---

## Extraction Rule: Prefer Parameterized Helpers

When moving helper functions into feature modules, prefer passing required state as parameters instead of exposing globals.

Good:
- helper(parent, state_pointer, callback)

Avoid:
- helper reaches into `main.c` globals via `extern`

Reason:
- Keeps feature modules reusable.
- Reduces cross-module coupling.
- Makes each extraction easier to verify.


---

## Extraction Rule: Feature Behavior vs Page State

Feature modules should own reusable behavior.

Page/main state may remain with the caller during incremental extraction.

Preferred pattern:
- feature_helper(&popup, &label, &list, status_text, callback)

Avoid:
- feature_helper() reaching into unrelated module globals

This keeps extraction incremental without creating broad `extern` state.

## Network Tools Ownership Update - 2026-07-02

ui_network_tools now owns:
- WiFi scan popup creation
- WiFi scan popup cleanup
- WiFi password popup creation
- WiFi password popup cleanup

main.c still owns:
- LVGL event callbacks
- WiFi credential save/connect behavior
- Network page orchestration

2026-07-02 Network tools update:
- Extracted WiFi password text copy helper into ui_network_tools.
- Reused ui_network_tools scan/popup cleanup helper after CONNECT.
- main.c still owns NVS credential save and WiFi reconnect behavior.
- Build verified.

2026-07-02 Network tools update:
- Extracted WiFi password text copy helper into ui_network_tools.
- Reused ui_network_tools scan/password popup cleanup after CONNECT.
- main.c still owns NVS credential save and WiFi reconnect behavior.
- Build and device behavior verified.

2026-07-02 Network refactor checkpoint:
- Completed ui_network_tools popup/UI extraction phase.
- ui_network_tools now owns WiFi scan popup UI, WiFi password popup UI, popup lifecycle cleanup, password text copy helper, and SSID selection/highlight helper.
- main.c still owns WiFi scan workflow, NVS credential save, WiFi reconnect behavior, and Network page orchestration.
- Next planned boundary: create network_wifi_scan.c/.h for WiFi scan workflow ownership.

2026-07-02 Network WiFi scan boundary:
- Added network_wifi_scan.c/.h.
- Added network_wifi_scan.c to CMakeLists.txt.
- Current module is a compile-verified boundary only.
- OTA verified after boundary addition.
- Future: move WiFi scan workflow out of main.c.

2026-07-02 Network WiFi scan extraction:
- Extracted WiFi scan result status formatter into network_wifi_scan.
- main.c now calls network_wifi_scan_format_found_status().
- Build and OTA/device behavior verified.

2026-07-02 Network WiFi scan extraction:
- Moved WiFi scan result rendering into network_wifi_scan_render_results().
- network_wifi_scan now formats scan status and coordinates result rendering.
- ui_network_tools still owns popup/list/button UI construction.
- main.c still owns starting the blocking WiFi scan and retrieving AP records.
- Build and OTA/device behavior verified.

2026-07-02 Printer Motion extraction complete:
- Added ui_printer_motion module.
- Moved Motion popup UI into ui_printer_motion.
- Moved motion button factory, step highlight logic, jog command formatting, extrude/retract command formatting, and Motion event callbacks into ui_printer_motion.
- main.c now only provides a small Moonraker send-gcode bridge for Motion.
- Motion popup open/close, jog, home, step highlight, extrude, and retract verified.
- main.c reduced to 6981 lines.

2026-07-02 Full refactor session summary:

Network tools:
- Extracted WiFi scan popup creation/cleanup into ui_network_tools.
- Extracted WiFi password popup creation/cleanup into ui_network_tools.
- Extracted WiFi password text copy helper into ui_network_tools.
- Extracted SSID selection/highlight helper into ui_network_tools.
- Extracted WiFi scan button population helper into ui_network_tools.

Network WiFi scan:
- Added network_wifi_scan.c/.h.
- Added network_wifi_scan.c to CMakeLists.txt.
- Extracted WiFi scan status formatting into network_wifi_scan.
- Extracted WiFi scan results rendering into network_wifi_scan.
- Left blocking scan start/AP collection in main.c for now after one failed move attempt was safely reverted.

Printer Motion:
- Added ui_printer_motion.c/.h.
- Added ui_printer_motion.c to CMakeLists.txt.
- Extracted Motion button factory into ui_printer_motion.
- Extracted Motion step highlight logic into ui_printer_motion.
- Extracted jog G-code formatting into ui_printer_motion.
- Extracted extrude/retract G-code formatting into ui_printer_motion.
- Extracted full Motion popup UI into ui_printer_motion.
- Moved Motion event callbacks into ui_printer_motion.
- main.c now only provides a small Moonraker send-gcode bridge for Motion.

Verification:
- Builds passed after each completed extraction.
- OTA/device behavior verified repeatedly.
- Motion popup open/close, X/Y/Z jog, home, step highlight, extrude, and retract verified.
- WiFi scan/password popup behavior verified.

Line count:
- Previous high-water freeze: ~7331 lines.
- Current main.c: 6981 lines.
- Crossed below 7000 lines.

## Splash Ownership

Module:
- ui_splash_v32

Responsibilities:
- Create splash UI.
- Destroy splash UI.
- Render splash progress.
- Own splash progress percentages.
- Own splash status text.
- Provide named startup-stage presentation APIs.

Public API:
- ui_splash_v32_create()
- ui_splash_v32_display_ready()
- ui_splash_v32_wifi_starting()
- ui_splash_v32_wifi_waiting(bool connected)
- ui_splash_v32_moonraker_ready()
- ui_splash_v32_dashboard_ready()
- ui_splash_v32_destroy()

main.c responsibility:
- Startup sequencing only.
- main.c decides when each stage happens, but does not own splash presentation.

