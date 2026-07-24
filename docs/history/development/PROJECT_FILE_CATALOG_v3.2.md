# PrinterHMI v3.2 Project File Catalog

## Purpose
Living catalog of source files currently used by the project.

This file should be updated whenever files are added, removed, renamed, or ownership changes.

---

## Core build files

### main/CMakeLists.txt
Owns:
- ESP-IDF component source list
- Include directories
- Required components

Notes:
- Every new `.c` module must be added to `SRCS`.

---

## Main application

### main/main.c
Current role:
- Legacy application root
- App startup
- WiFi/Moonraker/SD/OTA glue
- Page dispatcher bridge
- Many legacy page implementations still live here temporarily

Migration goal:
- Shrink over time.
- Move page-specific code into page modules.
- Move reusable UI helpers into `ui_widgets`.

---

## Theme and widgets

### main/ui_theme.c
Owns:
- Theme colors
- Visual constants
- Shared style values

### main/ui_theme.h
Owns:
- Public theme declarations

### main/ui_widgets.c
Owns:
- Generic reusable UI widgets
- Shared LVGL widget builders
- Generic cards/buttons once extracted

### main/ui_widgets.h
Owns:
- Public reusable widget declarations

---

## Shell

### main/ui_shell.c
Owns:
- Top bar
- Navigation rail
- Navigation buttons
- Active nav highlight
- Shell raise helpers

### main/ui_shell.h
Owns:
- Shell public API
- Page enum
- Shell create/update function declarations

Current status:
- Extracted and working.
- OTA verified.

---

## Dashboard

### main/ui_dashboard_v32.c
Owns:
- Dashboard page creation
- Dashboard v3.2 layout

### main/ui_dashboard_v32.h
Owns:
- Dashboard public API

### main/ui_status_banner_v32.c / .h
Owns:
- Dashboard status banner widget

### main/ui_active_print_v32.c / .h
Owns:
- Active print dashboard widget

### main/ui_machine_status_v32.c / .h
Owns:
- Machine status dashboard widget

### main/ui_command_bar_v32.c / .h
Owns:
- Dashboard command bar widget/actions

---

## Printer

### main/ui_printer_v32.c
Current role:
- Printer public API bridge
- Calls existing implementation still in `main.c`

Owns:
- `ui_printer_v32_show()`
- `ui_printer_v32_hide()`
- `ui_printer_v32_refresh()`

Migration goal:
- Move Printer-specific implementation here:
  - Printer page show/hide
  - Printer action buttons
  - Motion popup
  - Nozzle/bed/part fan callbacks
  - Printer thumbnail handling
  - Printer file popup handling

### main/ui_printer_v32.h
Owns:
- Printer public API declarations

Current status:
- Added to CMake.
- Dispatcher routes through this API.
- Clean build verified.
- OTA verified.

---

## Moonraker

### main/moonraker.c
Owns:
- Moonraker communication helpers already extracted or planned

### main/moonraker.h
Owns:
- Moonraker public declarations

---

## Settings

### main/ui_settings.c
Owns:
- Settings-related UI or helpers already extracted

### main/ui_settings.h
Owns:
- Settings public declarations

---

## Logs and checkpoints

### SHELL_REFACTOR_LOG.md
Owns:
- Refactor milestone log
- Build/OTA checkpoints
- Architecture decisions

### PROJECT_FILE_CATALOG.md
Owns:
- This file catalog
- Source ownership map

---

## Current architecture rule

Before moving code:
1. Identify the owning module.
2. Add/update public API if needed.
3. Build.
4. OTA if the change affects runtime behavior.
5. Log the checkpoint.

Do not move shared widget helpers into page modules.
Move only page-specific ownership first.
Shared card/button helpers belong in ui_widgets.

---

## Update: Printer Migration Status

### main/ui_printer_v32.c
Current role:
- Printer module
- Public Printer API
- Temporary bridge to legacy implementation in `main.c`

Current ownership:
- `ui_printer_v32_show()`
- `ui_printer_v32_hide()`
- `ui_printer_v32_refresh()`
- Dispatcher-facing Printer interface

Current implementation:
- `legacy_show_printer_tab()` still lives in `main.c`
- `legacy_hide_printer_tab()` still lives in `main.c`
- Legacy Printer controls block is marked in `main.c`

Remaining migration:
- Motion subsystem
- Temperature/fan popup subsystem
- Printer page implementation
- Printer file popup/browser
- Thumbnail integration
- Legacy bridge removal


---

## Update: Network Migration Status

### main/ui_network_v32.c
Current role:
- Network module
- Public Network API
- Temporary bridge to legacy implementation in `main.c`

Current ownership:
- `ui_network_v32_show()`
- `ui_network_v32_hide()`
- `ui_network_v32_refresh()`
- Dispatcher-facing Network interface

Current implementation:
- `legacy_show_network_tab()` still lives in `main.c`
- `legacy_hide_network_tab()` still lives in `main.c`

Remaining migration:
- Network page implementation
- WiFi scan popup/subsystem
- Moonraker discovery subsystem
- Host editor
- Port editor
- Password popup
- Legacy bridge removal


---

## Update: Drybox Migration Status

### main/ui_drybox_v32.c
Current role:
- Drybox module
- Public Drybox API
- Temporary bridge to legacy implementation in `main.c`

Current ownership:
- `ui_drybox_v32_show()`
- `ui_drybox_v32_hide()`
- `ui_drybox_v32_refresh()`
- Dispatcher-facing Drybox interface

Current implementation:
- `legacy_show_drybox_tab()` still lives in `main.c`
- `legacy_hide_drybox_tab()` still lives in `main.c`

Remaining migration:
- Drybox page implementation
- Drybox macro buttons
- Drybox status refresh
- Legacy bridge removal


---

## Update: Files Migration Status

### main/ui_files_v32.c
Current role:
- Files module
- Public Files API
- Temporary bridge to legacy implementation in `main.c`

Current ownership:
- `ui_files_v32_show()`
- `ui_files_v32_hide()`
- `ui_files_v32_refresh()`
- Dispatcher-facing Files interface

Current implementation:
- `legacy_show_files_tab()` still lives in `main.c`
- `legacy_hide_files_tab()` still lives in `main.c`
- `legacy_printer_load_files_now()` still lives in `main.c`
- File popup still uses legacy printer-file naming

Remaining migration:
- Files page/popup implementation
- File list loading
- File selection callbacks
- Rename legacy printer-file functions to Files ownership
- Legacy bridge removal


---

## Update: Network Legacy Block Marked

`main.c` now contains a marked legacy Network implementation block.

Status:
- Build verified
- OTA verified

Purpose:
- Marks the Network implementation boundary before code is moved into `ui_network_v32.c`.


---

## Update: Files Legacy Block Marked

`main.c` now contains a marked legacy Files implementation block.

Purpose:
- Marks the Files implementation boundary before code is moved into `ui_files_v32.c`.


---

## Update: Drybox Legacy Block Marked

`main.c` now contains a marked legacy Drybox implementation block.

Purpose:
- Marks the Drybox implementation boundary before code is moved into `ui_drybox_v32.c`.



---

## Update: Phase A Complete – Page Ownership Refactor

Page lifecycle ownership is now module-based.

### Current page ownership

- Dashboard: `ui_dashboard_v32`
- Printer: `ui_printer_v32`
- Files: `ui_files_v32`
- Network: `ui_network_v32`
- Drybox: `ui_drybox_v32`

### Important distinction

Ownership and physical file location are now separate.

The page modules own public lifecycle entry points, but some implementation code still temporarily lives in `main.c`.

### Temporary implementation locations

`main.c` still contains temporary implementation helpers for:

- Printer page create/destroy
- Files page create/destroy
- Network page create/destroy
- Drybox create/cleanup/refresh
- Some shared popup/helper logic

### Next catalog goal

During Phase B, move page-specific implementation into the matching module file:

- `ui_network_v32.c`
- `ui_files_v32.c`
- `ui_printer_v32.c`
- `ui_drybox_v32.c`

Do not move shared helpers into page modules unless ownership is clear.


---

## Update: Network Tools Boundary Created

New files:

- `main/ui_network_tools.c`
- `main/ui_network_tools.h`

Purpose:

- Future owner of Network-related popups/actions/tooling.
- Keeps `ui_network_v32.c` focused on the Network page layout and refresh lifecycle.

Future migration targets:

- WiFi scan popup
- WiFi password popup
- Host editor
- Port editor
- Moonraker test/scan popup
- Network action callbacks


---

## Update: v3.3 Responsibility Architecture Created

New architecture reference:

- `ARCHITECTURE_v3.3.md`

New module direction:

- `ui_network_v32.c` owns Network page layout/refresh.
- `ui_network_tools.c` owns Network popups/actions.
- Future `network_service.c` should own WiFi/Moonraker/NVS operations.

Catalog rule:
- Do not move large mixed-responsibility blocks into page modules.
- Extract by responsibility, not just by current line location.


---

## Update: Header/API Rule

Headers should list real exported module APIs only.

Do not add functions to a `.h` file just because they are future migration targets.

If a function:
- still lives in `main.c`
- is still `static`
- is not called from another compilation unit

then it should not be declared in a public header yet.


---

## Update: First Physical Feature Extraction

`main/ui_network_tools.c` now contains real implementation code.

Moved from `main.c`:
- WiFi scan list clear helper
- WiFi SSID row/button creation helper

Design note:
- These helpers receive LVGL object pointers and callbacks from the caller.
- This avoids exporting WiFi popup globals from `main.c`.

Status:
- Build verified
- OTA/runtime verified


---

## Update: Network WiFi Scan Popup Extraction

`main/ui_network_tools.c` now owns:
- WiFi scan list clearing
- WiFi SSID row creation
- WiFi scan popup creation

State remains temporarily in `main.c`.

Design:
- Popup helper is parameterized with LVGL object pointers and callback.


---

## Update: Network WiFi Scan Close Extraction

`main/ui_network_tools.c` now also owns:
- WiFi scan popup close/delete helper

`main.c` still owns:
- LVGL event adapter callback
- temporary WiFi popup state

## ui_network_tools.c / ui_network_tools.h update - 2026-07-02

Owns:
- WiFi scan popup UI
- WiFi password popup UI
- Popup creation/destruction helpers

Does not own yet:
- WiFi credential save/connect logic
- WiFi scan result processing
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
