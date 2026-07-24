# PrinterHMI v3.2 Roadmap

Purpose:
Track project direction by milestone.

---

# Current Stable Baseline

- v3.2 dashboard opens by default
- Legacy shell retained
- Live Machine Status works
- OTA works
- Active Print card works
- Preview widget exists
- Thumbnail Manager exists

---

# Milestone 1: Finish Dashboard

Done:
[x] Default v3.2 dashboard startup
[x] Status Banner
[x] Machine Status live data
[x] Command Bar
[x] Active Print card exists
[x] Active Print footer placeholder

Next:
[ ] Replace Active Print inline preview with ui_preview_v32
[ ] Connect preview placeholder safely
[ ] Connect thumbnail_manager_v32 status safely
[ ] Show cached thumbnail path
[ ] Display real thumbnail image

---

# Milestone 2: Thumbnail Pipeline

[ ] Identify old thumbnail code in main.c
[ ] Move cache path logic into thumbnail_manager_v32
[ ] Move SD cache load/store into thumbnail_manager_v32
[ ] Move download logic into thumbnail_manager_v32
[ ] Move decode/render responsibility out of main.c
[ ] Feed ui_preview_v32 with ready image

---

# Milestone 3: Printer Page Extraction

[ ] Review printer page block
[ ] Define ui_printer_v32 ownership
[ ] Move printer page layout
[ ] Move printer page update API
[ ] Keep command routing in main.c until stable

---

# Milestone 4: Files Page Extraction

[ ] Review files popup/page
[ ] Define ui_files_v32 ownership
[ ] Move file list UI
[ ] Move file selection UI callbacks
[ ] Keep Moonraker/file loading service separate

---

# Milestone 5: Network Extraction

[ ] Review network UI
[ ] Define ui_network_v32 ownership
[ ] Move network page layout
[ ] Move WiFi scan popup UI
[ ] Keep WiFi backend in main/service layer

---

# Milestone 6: Graphs Extraction

[ ] Review graph UI
[ ] Define ui_graphs_v32 ownership
[ ] Separate graph data storage from graph rendering
[ ] Move graph widgets
[ ] Keep SD history backend separate

---

# Milestone 7: Legacy Cleanup

[ ] Retire old dashboard body
[ ] Retire duplicate dashboard thumbnail path
[ ] Remove unused globals
[ ] Remove dead freeze-era experiments
[ ] Final pass over main.c


---

# Refactor Extraction Plan

## Phase 1: Remove Duplicate Dashboard

Goal:
Keep only ui_dashboard_v32 as the dashboard.

Actions:
[ ] Convert build_drybox_dashboard() into shell-only creation.
[ ] Remove legacy dashboard body from build_drybox_dashboard().
[ ] Keep top bar, nav rail, clock, WiFi bars, timers.
[ ] Start ui_dashboard_v32_create() as the only dashboard.
[ ] Verify OTA still works.
[ ] Freeze.

Target app_main flow:
    build_shell()
    ui_dashboard_v32_create()
    splash_create()
    start runtime

---

## Phase 2: Create ui_shell

Goal:
Move shell/topbar/nav ownership out of main.c.

Actions:
[x] Create ui_shell.c / ui_shell.h.
[ ] Move top bar creation.
[ ] Move nav rail creation.
[ ] Move clock/WiFi icon update.
[ ] Keep page routing in main.c until stable.
[/] Verify all nav tabs still open.
[ ] Freeze.

---

## Phase 3: Dashboard Refresh API

Goal:
Replace separate dashboard push helpers with one refresh entry point.

Actions:
[ ] Add ui_dashboard_v32_refresh(...).
[ ] Move banner formatting.
[ ] Move machine formatting.
[ ] Move active print footer formatting.
[ ] Remove ui_dashboard_v32_push_live_banner_data().
[ ] Remove ui_dashboard_v32_push_live_machine_data().
[ ] Verify live data.
[ ] Freeze.

---

## Phase 4: Preview Pipeline

Goal:
Separate files, thumbnail manager, and preview rendering.

Actions:
[ ] Add ui_preview_v32 to CMake.
[ ] Add thumbnail_manager_v32 to CMake.
[ ] Decouple ui_preview_v32 from thumbnail_manager_v32.
[ ] Move thumbnail cache/path/download into thumbnail_manager_v32.
[ ] Move canvas/decode/aspect-fit into ui_preview_v32.
[ ] Connect dashboard preview.
[ ] Connect printer preview.
[ ] Freeze.

---

## Phase 5: Page Modules

Future modules:
[ ] ui_printer_v32
[ ] ui_files_v32
[ ] ui_drybox_v32
[ ] ui_network_v32
[ ] ui_graphs_v32


---

# Progress Update
Date: 2026-07-01

## Phase 2: ui_shell

Status:
[x] Create ui_shell.c / ui_shell.h
[x] Move top bar creation
[x] Move clock timer
[x] Move WiFi status icons
[x] Move navigation rail
[x] Move navigation buttons
[x] Move active navigation highlighting
[x] Replace string routing with enum routing
[x] Create ui_shell_page_action() bridge
[x] OTA verified
[x] Runtime verified

Remaining:
[ ] Remove old shell code from main.c
[ ] Remove obsolete shell globals
[ ] Remove old navigation callback
[ ] Cleanup freeze

## Network Refactor Progress - 2026-07-02

Done:
- WiFi scan popup extracted.
- WiFi password popup extracted.

Next:
- Extract reusable WiFi credential save/connect helper.

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
