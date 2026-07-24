# Shell Refactor Log

## Checkpoint: shell ownership clean

### Verified working
- `idf.py build` passes.
- OTA workflow still working.
- New `ui_shell.c` owns shell navigation highlight state.

### Changes completed
- Removed old `set_active_nav()` from `main.c`.
- Removed old shell globals from `main.c`:
  - `top_bar`
  - `nav_rail`
  - `nav_buttons`
  - `active_nav_index`
- Removed unused legacy `rail` object creation from `build_drybox_dashboard()`.
- Confirmed `main.c` only calls shell creation:
  - `ui_shell_create()`
  - `ui_shell_create_nav()`

### Current ownership
- `ui_shell.c`
  - top bar
  - nav rail
  - nav buttons
  - active nav highlight

- `main.c`
  - page dispatcher
  - page show/hide logic
  - legacy page implementations

### Next target
Extract the Printer page into its own module without behavior changes.

Candidate files:
- `ui_printer_v32.c`
- `ui_printer_v32.h`

Goal:
- Move printer page create/show/hide/refresh ownership out of `main.c`.
- Keep `ui_shell_page_action()` in `main.c` for now.

## Printer page extraction notes

### Current state
`show_printer_tab()` is still in `main.c`.

### Important discovery
The Printer page UI builder depends on helpers that already appear before it:
- `make_printer_button`
- `part_fan_card_event_cb`
- `nozzle_card_event_cb`
- `bed_card_event_cb`
- `motion_popup_event_cb`
- `make_printer_info_w`
- `make_printer_info_compact_w`

### Extraction warning
Do not use broad Python block replacement on `main.c`.
A previous automated block move duplicated large sections and corrupted ordering.
Use manual, tiny edits only.

### Next safe move
Either:
1. Move only helper prototypes near the top, or
2. Create `ui_printer_v32.h/.c` as empty files and include them, with no behavior changes.

## Checkpoint: empty Printer module builds

### Verified working
- Added `ui_printer_v32.c`
- Added `ui_printer_v32.h`
- Included `ui_printer_v32.h` from `main.c`
- `idf.py build` passes

### Meaning
The Printer module now exists in the build system with no behavior changes.

### Next move
Move one tiny Printer-related helper or wrapper at a time.

## Checkpoint: Printer module bridge builds

### Verified working
- `show_printer_tab()` and `hide_printer_tab()` exported from `main.c`
- `ui_printer_v32_show()` calls existing `show_printer_tab()`
- `ui_printer_v32_hide()` calls existing `hide_printer_tab()`
- `idf.py build` passes

### Meaning
Printer page now has a public module API, while implementation still temporarily lives in `main.c`.

## Checkpoint: dispatcher uses Printer module API

### Verified working
- Added `ui_printer_v32.c` to `main/CMakeLists.txt`
- `main.c` now calls `ui_printer_v32_show()` / `ui_printer_v32_hide()`
- `ui_printer_v32.c` bridges those calls to existing `show_printer_tab()` / `hide_printer_tab()`
- `idf.py build` passes

### Meaning
The Printer page now has a real module boundary.

============================================================
Checkpoint: Printer API Bridge Complete
============================================================

Status
------
✓ idf.py clean
✓ idf.py build
✓ OTA update successful
✓ Runtime verified

Architecture
------------
✓ ui_shell owns shell
✓ ui_dashboard_v32 owns dashboard
✓ ui_printer_v32 owns Printer public API
✓ Dispatcher now routes through ui_printer_v32

Migration Pattern Proven
------------------------
1. Create module
2. Add to CMake
3. Create public API
4. Bridge to existing implementation
5. Redirect callers
6. Migrate implementation incrementally

Next Phase
----------
Begin moving Printer implementation helpers into
ui_printer_v32.c one small piece at a time.


============================================================
Checkpoint: Project File Catalog Added
============================================================

Added:
- PROJECT_FILE_CATALOG.md

Purpose:
- Catalog all active project source files.
- Track ownership of modules.
- Prevent unclear code movement during refactor.

Rule:
- Update PROJECT_FILE_CATALOG.md whenever source files are added,
  removed, renamed, or ownership changes.


============================================================
Checkpoint: Printer Migration Docs Updated
============================================================

Status:
- Clean build verified
- OTA verified

Added:
- Printer migration status to PROJECT_FILE_CATALOG.md
- Phase 3 Printer Migration update to ARCHITECTURE_v3.2.md
- Legacy Printer controls block markers in main.c

Decision:
- Pause Printer implementation migration for now.
- Next tab/module target: Network.

Reason:
- Network is more isolated than Printer.
- Network has fewer thumbnail/control dependencies.
- Same proven module bridge pattern can be reused.


============================================================
Checkpoint: Network API Bridge Created
============================================================

Added:
- ui_network_v32.c
- ui_network_v32.h
- ui_network_v32.c added to CMake

Changed:
- Network page implementation renamed to legacy_show_network_tab()
  and legacy_hide_network_tab()
- Dispatcher should now route through ui_network_v32_show()
  and ui_network_v32_hide()

Next verification:
- idf.py build
- OTA if build passes


============================================================
Checkpoint: Drybox API Bridge Created
============================================================

Added:
- ui_drybox_v32.c
- ui_drybox_v32.h
- ui_drybox_v32.c added to CMake

Changed:
- Drybox page implementation renamed to legacy_show_drybox_tab()
  and legacy_hide_drybox_tab()
- Dispatcher should now route through ui_drybox_v32_show()
  and ui_drybox_v32_hide()

Next verification:
- idf.py build
- OTA if build passes


============================================================
Checkpoint: Migration Status Added to Architecture
============================================================

Updated:
- ARCHITECTURE_v3.2.md

Purpose:
- Track module API, bridge, and implementation status in the architecture document.
- Avoid adding another separate migration tracker file.

Current verified bridges:
- Printer
- Network
- Drybox

Next target:
- Files


============================================================
Checkpoint: Files API Bridge Created
============================================================

Added:
- ui_files_v32.c
- ui_files_v32.h
- ui_files_v32.c added to CMake

Changed:
- Files route should now go through ui_files_v32_show()
  and ui_files_v32_hide()
- File refresh should now go through ui_files_v32_refresh()
- Legacy implementation remains in main.c

Next verification:
- idf.py build
- OTA if build passes


============================================================
Checkpoint: Network Legacy Block Marked
============================================================

Changed:
- Added BEGIN/END LEGACY NETWORK BLOCK markers in main.c.

Verified:
- idf.py build
- OTA successful

Meaning:
- Network implementation has a safe extraction boundary.
- Ready for incremental subsystem migration later.


============================================================
Checkpoint: Files Legacy Block Marked
============================================================

Changed:
- Added BEGIN/END LEGACY FILES BLOCK markers in main.c.

Meaning:
- Files implementation has a safe extraction boundary.
- Ready for incremental subsystem migration later.


============================================================
Checkpoint: Drybox Legacy Block Marked
============================================================

Changed:
- Added BEGIN/END LEGACY DRYBOX BLOCK markers in main.c.

Meaning:
- Drybox implementation has a safe extraction boundary.
- Ready for incremental subsystem migration later.


============================================================
Checkpoint: Page API Bridges and Legacy Markers Frozen
============================================================

Verified:
- Build successful
- OTA successful

Completed API bridges:
- ui_printer_v32
- ui_network_v32
- ui_drybox_v32
- ui_files_v32

Existing extracted modules:
- ui_shell
- ui_dashboard_v32

Marked legacy blocks:
- LEGACY PRINTER CONTROLS BLOCK
- LEGACY PRINTER PAGE BLOCK
- LEGACY NETWORK BLOCK
- LEGACY FILES BLOCK
- LEGACY DRYBOX BLOCK

Meaning:
- Public ownership boundaries are established.
- Legacy implementation boundaries are marked.
- Ready to begin implementation migration one subsystem at a time.

Recommended next phase:
- Start implementation migration with Network or Drybox, not Printer.


============================================================
Checkpoint: Drybox Refresh Bridge Verified
============================================================

Changed:
- Added legacy_refresh_drybox_tab() in main.c.
- ui_drybox_v32_refresh() now bridges to legacy_refresh_drybox_tab().
- legacy_show_drybox_tab() calls ui_drybox_v32_refresh() after creating widgets.

Verified:
- idf.py build
- OTA successful

Meaning:
- Drybox module now owns public refresh API.
- Drybox refresh behavior has a defined migration path.
- First real implementation migration step completed.


============================================================
Checkpoint: Drybox Legacy Helpers Renamed
============================================================

Changed:
- make_drybox_info() renamed to legacy_make_drybox_info()
- make_drybox_button() renamed to legacy_make_drybox_button()

Verified:
- idf.py build
- OTA successful

Meaning:
- Drybox layout helpers are clearly marked as temporary legacy implementation.
- Ready for future migration into ui_drybox_v32.c or ui_widgets as ownership becomes clearer.


============================================================
Checkpoint: Duplicate Drybox Refresh Blocks Replaced
============================================================

Changed:
- Replaced duplicate inline Drybox refresh blocks with ui_drybox_v32_refresh().
- Kept canonical legacy_refresh_drybox_tab() implementation in main.c.
- ui_drybox_v32_refresh() remains the public refresh API.

Verified:
- idf.py build
- OTA successful

Meaning:
- Drybox refresh now has one canonical implementation path.
- Other code now routes through the Drybox module API.
- This reduces duplicated refresh logic before moving implementation.


============================================================
Checkpoint: Drybox Refresh Cleanup Frozen
============================================================

Frozen:
- Drybox refresh bridge
- Duplicate Drybox refresh blocks removed
- Drybox refresh routed through ui_drybox_v32_refresh()

Status:
- Ready for next Drybox implementation migration step.


============================================================
Checkpoint: Drybox Hide Ownership Step
============================================================

Changed:
- legacy_hide_drybox_tab() renamed to legacy_cleanup_drybox_tab().
- ui_drybox_v32_hide() now owns the hide entry point.
- Cleanup implementation still temporarily lives in main.c.

Verified:
- idf.py build
- OTA successful

Meaning:
- Drybox hide behavior is now routed through the Drybox module boundary.
- One more legacy page function removed from public architecture.


============================================================
Checkpoint: Drybox Show Ownership Step
============================================================

Changed:
- legacy_show_drybox_tab() renamed to legacy_create_drybox_tab().
- ui_drybox_v32_show() now owns the show entry point.
- Create/layout implementation still temporarily lives in main.c.

Verified:
- idf.py build
- OTA successful

Meaning:
- Drybox show/hide/refresh all route through ui_drybox_v32.
- Drybox legacy implementation is now clearly reduced to create/cleanup/refresh helpers.


============================================================
Checkpoint: Drybox Show/Hide/Refresh Ownership Frozen
============================================================

Drybox public API:
- ui_drybox_v32_show()
- ui_drybox_v32_hide()
- ui_drybox_v32_refresh()

Legacy helpers remaining in main.c:
- legacy_create_drybox_tab()
- legacy_cleanup_drybox_tab()
- legacy_refresh_drybox_tab()

Verified:
- idf.py build
- OTA successful

Meaning:
- Drybox module owns all page entry points.
- main.c only holds temporary legacy implementation helpers.



============================================================
Checkpoint: Phase A Complete – Page Ownership Refactor
============================================================

Completed:
- Shell navigation routes through page module APIs.
- Printer ownership renamed to ui_printer_v32_create()/destroy().
- Files ownership renamed to ui_files_v32_create()/destroy().
- Network ownership renamed to ui_network_v32_create()/destroy().
- Drybox owns show/hide/refresh through ui_drybox_v32.
- Page lifecycle ownership is now module-based.

Still temporary:
- Page implementations still live in main.c.
- Some implementation blocks are still marked legacy because they have not yet been physically extracted.

Verified:
- Build verified.
- OTA/runtime verified during ownership cleanup.

Meaning:
- Phase A is complete.
- The project is ready for Phase B: move implementations into owning modules one page at a time.


============================================================
Checkpoint: Network Tools Boundary Created
============================================================

Changed:
- Added ui_network_tools.c
- Added ui_network_tools.h
- Added ui_network_tools.c to main/CMakeLists.txt

Purpose:
- Establishes a future owner for Network action/popup/tooling code.
- Network page layout remains owned by ui_network_v32.
- Network popups/actions will migrate incrementally into ui_network_tools.

Verified:
- idf.py build

Meaning:
- Phase B Network extraction can proceed without turning ui_network_v32.c into another large mixed-responsibility file.


============================================================
Checkpoint: v3.3 Responsibility Architecture Created
============================================================

Created:
- ARCHITECTURE_v3.3.md

Purpose:
- Define the next refactor stage before moving large code blocks.
- Prevent page modules from becoming new monoliths.

Rule:
- Pages orchestrate.
- Feature modules own popups/actions.
- Service modules own non-UI operations.

Next:
- Begin with Network responsibility extraction.
- First target: ui_network_tools.


============================================================
Checkpoint: Network Tool Callback Ownership Step
============================================================

Changed:
- Renamed Network page action callbacks:
  - network_open_wifi_scan_cb() -> ui_network_tools_open_wifi_scan_cb()
  - network_open_host_edit_cb() -> ui_network_tools_open_host_edit_cb()
  - network_open_port_edit_cb() -> ui_network_tools_open_port_edit_cb()
- Callback ownership renamed, but not exported in ui_network_tools.h yet.

Verified:
- idf.py build

Meaning:
- Network action callbacks now point toward ui_network_tools ownership.
- Implementations still temporarily live in main.c.
- No behavior change intended.


============================================================
Checkpoint: Header/API Rule Added
============================================================

Changed:
- Added rule that public headers must describe real exported APIs only.
- Static functions still living in main.c should not be declared in module headers.

Meaning:
- Ownership can be renamed before implementation moves.
- Public API export happens later, only when needed.


============================================================
Checkpoint: Network WiFi Popup Ownership Rename
============================================================

Changed:
- Renamed WiFi scan popup functions under ui_network_tools ownership:
  - close_wifi_scan_popup_cb() -> ui_network_tools_close_wifi_scan_popup_cb()
  - show_wifi_scan_popup() -> ui_network_tools_show_wifi_scan_popup()

Verified:
- idf.py build

Meaning:
- WiFi scan popup ownership now points toward ui_network_tools.
- Implementations still remain static in main.c.
- No public header export yet.
- No behavior change intended.


============================================================
Checkpoint: Network WiFi List Helper Ownership Rename
============================================================

Changed:
- Renamed WiFi scan list helper functions under ui_network_tools ownership:
  - clear_wifi_popup_network_buttons() -> ui_network_tools_clear_wifi_popup_network_buttons()
  - add_wifi_ssid_button() -> ui_network_tools_add_wifi_ssid_button()

Verified:
- idf.py build

Meaning:
- WiFi scan popup/list ownership continues moving toward ui_network_tools.
- Implementations still remain static in main.c.
- No public header export yet.
- No behavior change intended.


============================================================
Checkpoint: Network WiFi Password Popup Ownership Rename
============================================================

Changed:
- Renamed remaining WiFi popup/password helpers under ui_network_tools ownership:
  - close_wifi_password_popup_cb() -> ui_network_tools_close_wifi_password_popup_cb()
  - save_wifi_password_only_cb() -> ui_network_tools_save_wifi_password_only_cb()
  - show_wifi_password_popup() -> ui_network_tools_show_wifi_password_popup()
  - wifi_ssid_selected_cb() -> ui_network_tools_wifi_ssid_selected_cb()

Verified:
- idf.py build
- OTA/runtime WiFi popup test if performed

Meaning:
- WiFi scan + password popup ownership now consistently points toward ui_network_tools.
- Implementations remain static in main.c.
- No public header export yet.
- No behavior change intended.

============================================================
Checkpoint: Network WiFi Feature Ownership Complete
============================================================

Completed ownership rename for the entire WiFi Scan feature.

Feature includes:
- WiFi scan callback
- WiFi scan runner
- WiFi scan popup
- WiFi list helpers
- SSID selection
- Password popup
- Password save
- Popup close helpers

Status:
- Build verified
- OTA verified

Meaning:
- The complete WiFi Scan feature now belongs to ui_network_tools by ownership.
- Physical implementation still resides in main.c.
- Next step is a single feature extraction instead of individual function moves.


============================================================
Checkpoint: Network WiFi Feature Ownership Complete
============================================================

Completed ownership rename for the complete WiFi Scan feature.

Feature includes:
- WiFi scan callback
- WiFi scan runner
- WiFi scan popup
- WiFi scan list helpers
- SSID selection callback
- Password popup
- Password save callback
- Popup close helpers

Verified:
- idf.py build
- OTA/runtime verification if performed

Meaning:
- The complete WiFi Scan feature now belongs to ui_network_tools by ownership.
- Physical implementation still resides in main.c.
- No public header export yet.
- Next step is one cohesive feature extraction into ui_network_tools.c.


============================================================
Checkpoint: First Physical Feature Extraction
============================================================

Completed:
- Moved WiFi scan list helper implementations from main.c to ui_network_tools.c.

Functions moved:
- ui_network_tools_clear_wifi_popup_network_buttons()
- ui_network_tools_add_wifi_ssid_button()

Design improvement:
- Helpers now operate on caller-supplied state instead of reaching into main.c globals.
- SSID selected callback is supplied as a parameter instead of being hard-coded.

Verified:
- idf.py build
- OTA/runtime WiFi scan list verified

Meaning:
- First successful Phase B physical feature extraction completed.
- main.c has begun shrinking.
- ui_network_tools is now a real implementation module, not only a placeholder.


============================================================
Checkpoint: Network WiFi Scan Popup Extraction
============================================================

Completed:
- Moved WiFi scan popup builder into ui_network_tools.c.

Function moved:
- ui_network_tools_show_wifi_scan_popup()

Design:
- Popup helper receives popup/list/label state by pointer.
- Close callback is supplied by caller.
- No main.c globals exported.

Verified:
- idf.py build
- OTA/runtime WiFi popup verified

Meaning:
- ui_network_tools now owns WiFi scan list helpers and WiFi scan popup creation.
- main.c continues to own temporary page/feature state.


============================================================
Checkpoint: Network WiFi Scan Close Extraction
============================================================

Completed:
- Moved WiFi scan popup close/delete behavior into ui_network_tools.c.

Function added:
- ui_network_tools_close_wifi_scan_popup()

Remaining in main.c:
- ui_network_tools_close_wifi_scan_popup_cb()
  - now only adapts LVGL event callback to extracted helper.

Verified:
- idf.py build
- OTA/runtime popup close/reopen/password flow verified

Meaning:
- WiFi scan popup construction and teardown behavior now live in ui_network_tools.
- main.c keeps temporary state and thin event adapter.
