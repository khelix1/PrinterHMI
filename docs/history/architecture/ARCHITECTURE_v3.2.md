# PrinterHMI v3.2 Architecture

Purpose:
Define ownership of every subsystem before moving code.

This document answers:
Where does this block really belong?

---

# Rules

- Prove functionality first.
- Refactor second.
- Make one small change at a time.
- Freeze before risky work.
- Do not move code until ownership is clear.
- main.c is the coordinator, not the dumping ground.

---

# main.c

Purpose:
Application coordinator.

Owns:
- app_main()
- Startup sequence
- Runtime task
- Timers
- Navigation
- Shell
- Top bar
- Left nav rail
- WiFi initialization
- Moonraker polling scheduler
- SD mount
- OTA startup and task orchestration
- Splash screen
- Page switching

Does NOT own:
- Dashboard widget layout
- Printer page layout
- Network page layout
- Files page layout
- Thumbnail rendering
- Graph rendering
- Machine Status rendering
- Active Print rendering

---

# ui_dashboard_v32

Purpose:
Operator dashboard.

Owns:
- Dashboard layout
- Dashboard composition
- Dashboard refresh
- Status Banner
- Active Print card
- Machine Status card
- Command Bar

Future:
- Dashboard-owned refresh coordinator
- Preview integration
- Active Print live footer integration

Does NOT own:
- WiFi
- Moonraker HTTP
- SD mounting
- OTA
- Thumbnail cache
- Thumbnail download

---

# ui_status_banner_v32

Purpose:
Dashboard status banner.

Owns:
- Printer state display
- Active file display
- ETA display
- Progress display

Does NOT own:
- Moonraker polling
- Printer state parsing

---

# ui_machine_status_v32

Purpose:
Machine status card.

Owns:
- Nozzle display
- Bed display
- Chamber / air temp display
- Humidity display
- Speed display
- Flow display
- Fan display

Does NOT own:
- Sensor polling
- Moonraker parsing

---

# ui_active_print_v32

Purpose:
Active Print card.

Owns:
- Active Print layout
- Footer display
- Preview container

Does NOT own:
- Thumbnail download
- Thumbnail cache
- Thumbnail decode
- Moonraker parsing

---

# ui_preview_v32

Purpose:
Reusable preview widget.

Owns:
- Preview box
- Placeholder text
- Image object
- Showing image source

Does NOT own:
- File selection
- Cache path
- Download
- Decode

---

# thumbnail_manager_v32

Purpose:
Backend thumbnail pipeline.

Owns:
- Selected file
- Thumbnail state
- Cache path
- Download
- Decode
- Ready image
- Error state

Does NOT own:
- LVGL objects
- Preview widget layout
- Dashboard layout

---

# moonraker

Purpose:
Moonraker communication and parsing.

Owns:
- HTTP event handler
- JSON string parsing
- JSON number parsing
- Thumbnail path parsing
- Moonraker state model

Does NOT own:
- UI rendering
- Page layout

---

# ui_theme

Purpose:
Shared colors, fonts, and style helpers.

Owns:
- Theme colors
- Panel style
- Card style
- Button style
- Label style

---

# ui_widgets

Purpose:
Reusable LVGL widget constructors.

Owns:
- Standard panel creation
- Standard card creation
- Standard label creation
- Standard button creation

---

# Current Extraction Status

Done:
- Theme module
- Widget module
- Status Banner module
- Machine Status module
- Command Bar module
- Dashboard v32 module exists
- Preview module exists
- Thumbnail Manager module exists
- New dashboard boots by default
- Live Machine Status works
- OTA works

In Progress:
- Active Print footer
- Preview integration
- Thumbnail pipeline migration

Legacy Still In main.c:
- Old dashboard body
- Printer page
- Files popup/page
- Network page
- Drybox page
- Graphs
- Thumbnail download/render/cache
- Printer controls
- Motion controls
- OTA UI
- Settings UI/shell linkage

---

# Extraction Order

Phase 1:
- Finish Dashboard
- Finish Active Print
- Finish Preview

Phase 2:
- Move thumbnail pipeline into thumbnail_manager_v32

Phase 3:
- Extract Printer page into ui_printer_v32

Phase 4:
- Extract Files page/popup into ui_files_v32

Phase 5:
- Extract Network page into ui_network_v32

Phase 6:
- Extract Graphs into ui_graphs_v32 plus graph history/storage helpers

Phase 7:
- Clean up old dashboard body from main.c

---

# Questions To Review Block By Block

What stays in main.c?
- app_main
- startup
- shell
- nav
- runtime
- WiFi init
- Moonraker poll scheduler
- SD mount
- OTA orchestration

What moves later?
- Printer page
- Files page
- Network page
- Drybox page
- Graphs
- Thumbnail pipeline
- Printer controls
- Motion popups
- OTA UI

What can eventually retire?
- Old dashboard body
- Old dashboard thumbnail render path
- Duplicate preview logic
- Unused legacy labels/globals

---

# Development Rule

Before writing code, ask:

Who owns this?

If ownership is unclear:
Update this document first.

Do not move shared widget helpers into page modules.
Move only page-specific ownership first.
Shared card/button helpers belong in ui_widgets.

---

# Update: Shell Ownership Corrected

main.c no longer owns:
- Shell
- Top bar
- Left nav rail
- Navigation highlight state

ui_shell owns:
- Shell creation
- Top bar
- Left nav rail
- Nav buttons
- Active nav highlight
- Shell raise helpers

main.c still owns:
- app_main()
- Startup sequence
- Runtime task/timers
- WiFi init
- Moonraker polling scheduler
- SD mount
- OTA orchestration
- Temporary page dispatcher bridge

---

# Update: Printer API Bridge

ui_printer_v32 now owns:
- Public Printer API:
  - ui_printer_v32_show()
  - ui_printer_v32_hide()
  - ui_printer_v32_refresh()

Current implementation:
- legacy_show_printer_tab() still lives in main.c
- legacy_hide_printer_tab() still lives in main.c
- ui_printer_v32 bridges to those legacy functions

Status:
- Clean build verified
- OTA verified
- Ready for incremental Printer implementation migration

---

# Update: Phase 3 – Printer Migration

Completed:
- Printer module created
- Added to CMake
- Public API established
- Dispatcher routes through Printer API
- Legacy implementation renamed:
  - legacy_show_printer_tab()
  - legacy_hide_printer_tab()
- Legacy Printer controls block marked
- Clean build verified
- OTA verified

Remaining:
- Move Motion subsystem
- Move Temperature/fan popup subsystem
- Move Printer page implementation
- Move Printer file popup/browser
- Resolve thumbnail ownership/integration
- Remove legacy bridge

Rule:
- Do not move generic widget helpers into ui_printer_v32.
- Shared cards/buttons belong in ui_widgets.
- Move Printer-specific behavior only.


---

# Update: Network API Bridge

ui_network_v32 now owns:
- Public Network API:
  - ui_network_v32_show()
  - ui_network_v32_hide()
  - ui_network_v32_refresh()

Current implementation:
- legacy_show_network_tab() still lives in main.c
- legacy_hide_network_tab() still lives in main.c
- ui_network_v32 bridges to those legacy functions

Status:
- Ready for clean build and OTA verification

Remaining:
- Move Network page implementation
- Move WiFi scan subsystem
- Move Moonraker discovery subsystem
- Move host/port/password UI


---

# Update: Drybox API Bridge

ui_drybox_v32 now owns:
- Public Drybox API:
  - ui_drybox_v32_show()
  - ui_drybox_v32_hide()
  - ui_drybox_v32_refresh()

Current implementation:
- legacy_show_drybox_tab() still lives in main.c
- legacy_hide_drybox_tab() still lives in main.c
- ui_drybox_v32 bridges to those legacy functions

Status:
- Ready for clean build and OTA verification

Remaining:
- Move Drybox page implementation
- Move Drybox macro button implementation
- Move Drybox refresh logic
- Remove legacy bridge


---

# Migration Status Snapshot

Legend:
- ✓ complete / verified
- ~ partial / in progress
- □ not started
- ✗ not migrated yet

## Shell
API: ✓
Bridge: ✓
Implementation: ✓

## Dashboard
API: ✓
Bridge: ✓
Implementation: ~

Remaining:
- Active Print footer integration
- Preview integration
- Old dashboard cleanup

## Printer
API: ✓
Bridge: ✓
Implementation: ✗

Remaining:
- Motion subsystem
- Temperature/fan popup subsystem
- Printer page implementation
- Printer file popup/browser
- Thumbnail integration
- Legacy bridge removal

## Network
API: ✓
Bridge: ✓
Implementation: ✗

Remaining:
- Network page implementation
- WiFi scan popup/subsystem
- Moonraker discovery subsystem
- Host editor
- Port editor
- Password popup
- Legacy bridge removal

## Drybox
API: ✓
Bridge: ✓
Implementation: ✗

Remaining:
- Drybox page implementation
- Drybox macro buttons
- Drybox refresh logic
- Legacy bridge removal

## Files
API: □
Bridge: □
Implementation: ✗

Next:
- Create ui_files_v32.c/.h
- Add to CMake
- Bridge Files API

## Graphs
API: □
Bridge: □
Implementation: ✗

Next:
- Create ui_graphs_v32.c/.h later
- Treat graph history/storage as separate backend subsystem

## Settings
API: □
Bridge: □
Implementation: ✗

Decision pending:
- Could become ui_settings_v32 or stay in existing ui_settings depending on current file role.


---

# Update: Files API Bridge

ui_files_v32 now owns:
- Public Files API:
  - ui_files_v32_show()
  - ui_files_v32_hide()
  - ui_files_v32_refresh()

Current implementation:
- legacy_show_files_tab() still lives in main.c
- legacy_hide_files_tab() still lives in main.c
- legacy_printer_load_files_now() still lives in main.c
- Legacy file popup still uses printer-file naming

Status:
- Ready for clean build and OTA verification

Remaining:
- Move Files page/popup implementation
- Move file list loading
- Move file selection callbacks
- Rename legacy printer-file identifiers under Files ownership
- Remove legacy bridge


---

# Update: Network Legacy Block Marked

Network implementation in `main.c` is now marked with:

- BEGIN LEGACY NETWORK BLOCK
- END LEGACY NETWORK BLOCK

Purpose:
- Defines the future extraction boundary for Network implementation.
- No behavior change.
- Build verified.
- OTA verified.

Future extraction units:
- WiFi scan popup
- WiFi scan action
- Moonraker discovery
- Host editor
- Port editor
- Network page show/hide implementation


---

# Update: Files Legacy Block Marked

Files implementation in `main.c` is now marked with:

- BEGIN LEGACY FILES BLOCK
- END LEGACY FILES BLOCK

Purpose:
- Defines the future extraction boundary for Files implementation.
- No behavior change.
- Build/OTA verified if completed after marker.

Future extraction units:
- File popup
- File list loading
- File selection callback
- Confirm print/detail popup
- Legacy printer-file naming cleanup


---

# Update: Drybox Legacy Block Marked

Drybox implementation in `main.c` is now marked with:

- BEGIN LEGACY DRYBOX BLOCK
- END LEGACY DRYBOX BLOCK

Purpose:
- Defines the future extraction boundary for Drybox implementation.
- No behavior change.
- Build/OTA verified if completed after marker.

Future extraction units:
- Drybox page show/hide
- Drybox info cards
- Drybox macro buttons
- Drybox refresh logic



---

# Update: Phase A Complete – Page Ownership Refactor

Phase A is complete.

The shell owns navigation and routes page selection through module APIs. Page lifecycle ownership now belongs to page modules:

- `ui_dashboard_v32`
- `ui_printer_v32`
- `ui_files_v32`
- `ui_network_v32`
- `ui_drybox_v32`

Current rule:

- Public page entry points live in page modules.
- Temporary implementation helpers may still live in `main.c`.
- `main.c` should not regain ownership of page lifecycle.
- New work should call module APIs, not implementation helpers.

Current temporary implementation helpers:

- `ui_printer_v32_create()` / `ui_printer_v32_destroy()` still live in `main.c`.
- `ui_files_v32_create()` / `ui_files_v32_destroy()` still live in `main.c`.
- `ui_network_v32_create()` / `ui_network_v32_destroy()` still live in `main.c`.
- Drybox create/cleanup/refresh helpers still live in `main.c`.

The old `legacy_show_*_tab()` / `legacy_hide_*_tab()` page ownership model is retired.

## Phase B Roadmap – Implementation Extraction

Recommended order:

1. Settings
2. Network
3. Files
4. Printer
5. Drybox
6. Dashboard/shared-widget polish

Extraction rule:

Move only one owning page at a time. Preserve behavior first. Build and OTA-verify before deleting bridge code.


---

# Update: v3.3 Responsibility Architecture Defined

A new architecture document has been created:

- `ARCHITECTURE_v3.3.md`

Purpose:
- Defines the next architecture stage after Phase A ownership cleanup.
- Separates page ownership from feature ownership and service ownership.
- Establishes that pages are orchestration modules, not implementation modules.

Phase B is now Responsibility Extraction:
- Move one responsibility at a time.
- Preserve behavior.
- Build and OTA/runtime verify before deleting old code.
