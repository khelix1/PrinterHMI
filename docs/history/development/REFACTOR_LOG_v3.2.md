# PrinterHMI v3.2 Refactor Log

---

Memory Audit Rule: During every major extraction or cleanup, inspect for large temporary buffers. Default new temporary work buffers to PSRAM first, with internal RAM only when required for performance or hardware constraints. This avoids hidden LVGL stack overflows and preserves scarce internal RAM for FreeRTOS, DMA, and system services.


# Session 001
Architecture Review Complete

Date:
2026-07-01

Status:
✔ Complete

---

## Major Milestone

Completed a full architectural review of the entire v3.2 codebase.

Every source file was reviewed.

Every major section of main.c was reviewed.

Ownership for every subsystem was identified before beginning extraction.

This establishes the long-term architecture for the project.

---

## Documentation Completed

✔ ARCHITECTURE_v3.2.md

✔ ROADMAP_v3.2.md

✔ CODE_REVIEW_v3.2.md

✔ CRASH_LOG_v3_2.md

---

## Files Reviewed

✔ main/CMakeLists.txt

✔ ui_theme

✔ ui_widgets

✔ ui_dashboard_v32

✔ ui_status_banner_v32

✔ ui_machine_status_v32

✔ ui_active_print_v32

✔ ui_command_bar_v32

✔ ui_preview_v32

✔ thumbnail_manager_v32

✔ moonraker

✔ ui_settings

✔ ui_shell interface

✔ Entire main.c

---

## Major Architectural Decisions

Application hierarchy is now defined as:

app_main()

↓

Application Runtime

↓

ui_shell

↓

Dashboard
Printer
Files
Network
Drybox
Settings
Graphs

↓

Preview

↓

Thumbnail Manager

---

Dashboard Decision

Keep:

    ui_dashboard_v32

Retire:

    Legacy dashboard contained in build_drybox_dashboard()

Only one dashboard will exist going forward.

---

Preview Decision

Preview rendering belongs in:

    ui_preview_v32

Thumbnail ownership belongs in:

    thumbnail_manager_v32

Files and Printer will both reuse the same preview component.

---

Shell Decision

Application shell responsibilities identified:

• Top bar

• Navigation rail

• Clock

• WiFi indicator

• Status icons

• Page host

Future owner:

    ui_shell

---

Runtime Decision

main.c remains responsible only for:

• startup

• runtime task

• scheduler

• service initialization

• page coordination

Business logic will continue moving into modules.

---

Phase 1 Started

Created:

    ui_shell.c

Added:

    ui_shell.c to CMake

Current status:

✔ Builds successfully

✔ OTA verified

✔ Runtime unchanged

✔ No regressions observed

ui_shell currently exists as a scaffold.

No functionality has been moved yet.

---

Next Step

Begin extracting shell responsibilities from main.c:

1. Top bar

2. Navigation rail

3. Clock

4. WiFi icons

5. Shell timers

After shell extraction:

Remove remaining legacy dashboard implementation.

---

Project Status

Architecture Confidence:
★★★★★

Runtime Stability:
★★★★★

OTA Stability:
★★★★★

Module Readiness:
★★★★★

The project has transitioned from exploratory development into structured modular refactoring.

---

# Session 002
Phase 1: ui_shell Top Bar Extraction

Status:
✔ Working

Completed:
✔ Top bar creation moved from main.c into ui_shell.c
✔ Clock moved into ui_shell.c
✔ WiFi bar objects moved into ui_shell.c
✔ ui_shell_create() now creates top bar
✔ ui_shell_raise_topbar() added
✔ main.c now keeps nav rail only during this phase
✔ Build passed
✔ OTA/test passed

Notes:
- Navigation still lives in main.c.
- Legacy dashboard removal is still pending.
- WiFi is connected, but WiFi bar display may need a small follow-up check after shell extraction.
- Runtime remained stable.

Next:
Move navigation rail into ui_shell, or first fix WiFi bar update if needed.


---

# Session 003
Top Bar Extraction Follow-up

Status:
✔ Working

Completed:
✔ Removed WiFi polling from shell clock timer
✔ WiFi bars now update from runtime path only
✔ Early ESP-Hosted link errors/noisy red WiFi state avoided
✔ Top bar remains owned by ui_shell
✔ Runtime stable

Next:
Move navigation rail into ui_shell.

# Session 004
Navigation Extraction

Status:
✔ Working

Completed:
✔ Navigation rail moved into ui_shell
✔ Navigation buttons moved into ui_shell
✔ Active navigation highlighting moved into ui_shell
✔ Top bar + navigation now owned by ui_shell
✔ Page routing intentionally remains in main.c
✔ OTA verified
✔ Runtime stable

Architecture:

main.c
    Application

        ↓

ui_shell
    Top Bar
    Clock
    WiFi Bars
    Navigation
    Active Nav

        ↓

Dashboard
Printer
Files
Network
Settings

---

# Session 006
Shell Cleanup Attempt Rolled Back

Status:
✔ Restored working state

What happened:
- Attempted to remove old shell/nav leftover code from main.c.
- Cleanup caused duplicate/orphaned function blocks around old nav code.
- Restored to the last working nav-extraction freeze.

Current stable state:
✔ ui_shell owns top bar
✔ ui_shell owns clock
✔ ui_shell owns WiFi bars
✔ ui_shell owns navigation rail
✔ ui_shell owns nav buttons
✔ main.c owns page routing through ui_shell_nav_action()
✔ Build restored

Decision:
- Leave old unused shell/nav code in main.c for now.
- Do not risk stability for warning cleanup tonight.
- Continue only from a known-good freeze.

Next:
- Verify build/OTA.
- Freeze restored state.
- Later cleanup should be manual and block-by-block, not broad scripted replacement.

Session 007
Shell Navigation Architecture Complete

✔ ui_shell owns shell widgets
✔ enum-based navigation
✔ page routing bridge converted
✔ OTA verified
✔ Runtime verified

Remaining:
- Module extraction
- Legacy cleanup

---

# Session 007
Shell Navigation Architecture Complete

Date:
2026-07-01

Status:
✔ Complete

---

## Major Milestone

Completed extraction of the application shell.

ui_shell now owns:

✔ Top Bar

✔ Clock

✔ WiFi Signal

✔ Navigation Rail

✔ Navigation Buttons

✔ Active Navigation Highlight

✔ Enum-based Navigation

✔ ui_shell_page_action()

OTA verified.

Runtime verified.

---

## Current Architecture

app_main()

↓

Runtime

↓

ui_shell

• Top Bar

• Clock

• WiFi

• Navigation

↓

ui_shell_page_action()

↓

Dashboard

Printer

Files

Network

Drybox

Settings

---

## Remaining

Small cleanup only.

• Remove obsolete shell globals

• Remove old navigation callback

• Remove dead shell helper functions

No architectural work remains.

---

## Next

Begin extraction of full page modules.

Priority:

1. Printer

2. Files

3. Network

4. Drybox

5. Graphs


---

# Checkpoint: Phase A Complete – Page Ownership Refactor

Status:
- Build verified.
- OTA/runtime verified after ownership steps.

Completed:
- Shell owns navigation.
- Dashboard routes through module API.
- Printer lifecycle ownership now belongs to `ui_printer_v32`.
- Files lifecycle ownership now belongs to `ui_files_v32`.
- Network lifecycle ownership now belongs to `ui_network_v32`.
- Drybox lifecycle ownership belongs to `ui_drybox_v32`.
- Page entry points no longer use `legacy_show_*_tab()` / `legacy_hide_*_tab()` ownership names.

Current state:
- Page APIs are established.
- Implementations still temporarily live in `main.c`.
- Next phase is implementation extraction, not ownership cleanup.

Next:
1. Extract Settings.
2. Extract Network.
3. Extract Files.
4. Extract Printer.
5. Extract Drybox.
6. Polish Dashboard and shared widgets.


---

# Checkpoint: v3.3 Responsibility Architecture Created

Created:
- `ARCHITECTURE_v3.3.md`

Meaning:
- Phase A page ownership is complete.
- Phase B will extract responsibilities instead of blindly moving page blocks.
- `ui_network_v32.c` remains the Network page.
- `ui_network_tools.c` becomes the Network popup/action owner.
- Future service code should move into service modules instead of page modules.

Next:
1. Expand `ui_network_tools`.
2. Move one small Network popup/helper.
3. Build.
4. OTA/runtime verify.
5. Update logs.


---

# Checkpoint: Network Tool Callback Ownership Step

Changed:
- Network card callbacks renamed under `ui_network_tools_*`.
- Callback names now point toward `ui_network_tools` ownership, but remain private/static until implementation moves.

Verified:
- `idf.py build`

Current state:
- Callback implementations still temporarily live in `main.c`.
- `ui_network_v32` now wires Network cards to `ui_network_tools_*` callbacks.


---

# Checkpoint: Header/API Rule Added

Lesson learned:
- Public headers should not declare functions that still live as `static` functions in `main.c`.

Rule:
- Headers describe real module APIs, not future intentions.
- Ownership rename does not automatically mean exported API.
- Export only after implementation moves or another module needs access.

Reason:
- Prevents static/non-static declaration conflicts.
- Keeps module boundaries honest during Phase B extraction.


---

# Checkpoint: Network WiFi Feature Ownership Complete

Completed:
- Full WiFi Scan feature ownership renamed under `ui_network_tools_*`.

Includes:
- scan open callback
- scan runner
- scan popup
- scan list helpers
- SSID selection
- password popup
- password save
- popup close helpers

Verified:
- `idf.py build`
- OTA/runtime verification if performed

Current state:
- Implementation remains static in `main.c`.
- Ownership is complete by name.
- Next step is physical extraction into `ui_network_tools.c`.


---

# Checkpoint: First Physical Feature Extraction

Completed:
- First physical Phase B extraction into `ui_network_tools.c`.

Moved:
- `ui_network_tools_clear_wifi_popup_network_buttons()`
- `ui_network_tools_add_wifi_ssid_button()`

Result:
- `main.c` is smaller.
- `ui_network_tools.c` now owns real WiFi scan list helper implementation.

Verified:
- `idf.py build`
- OTA/runtime WiFi scan list verified

Design rule reinforced:
- Prefer passing state into helpers instead of exposing globals with `extern`.


---

# Checkpoint: Network WiFi Scan Popup Extraction

Moved:
- `ui_network_tools_show_wifi_scan_popup()` into `ui_network_tools.c`.

Verified:
- `idf.py build`
- OTA/runtime WiFi popup verified

Design:
- Feature module owns popup behavior.
- Caller supplies popup state and close callback.
- No `extern` globals introduced.


---

# Checkpoint: Network WiFi Scan Close Extraction

Moved:
- WiFi scan popup close/delete behavior into `ui_network_tools.c`.

Added:
- `ui_network_tools_close_wifi_scan_popup()`

Verified:
- `idf.py build`
- OTA/runtime popup close/reopen/password flow verified

Design:
- Event callback remains in `main.c` as a thin adapter.
- Extracted helper receives all state by pointer.
- No globals exported.

## 2026-07-02 - Network Tools Popup Extraction

Completed:
- Extracted WiFi scan popup creation into ui_network_tools.
- Extracted WiFi scan popup cleanup into ui_network_tools.
- Extracted WiFi password popup creation into ui_network_tools.
- Extracted WiFi password popup cleanup into ui_network_tools.

Result:
- Popup lifecycle is now owned by ui_network_tools.
- main.c keeps thin wrappers/event callbacks.
- Build verified.

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

## 2026-07-04 — Rollback Checkpoint

Rolled back to known-good refactor checkpoint:

- main.c.freeze_refactor_6297_verified
- ui_network_tools.c.freeze_network_popups_verified
- ui_network_tools.h.freeze_network_popups_verified

Reason:
- Moonraker scan extraction introduced instability.
- OTA to known-good checkpoint worked.
- Known-good checkpoint boots successfully.

Conclusion:
- Popup/module refactors through 6297 lines are valid.
- Moonraker scan extraction should be retried later with stricter UI-thread ownership.
- Do not create LVGL widgets or mutate LVGL labels from moon_scan task.
- Next retry should use an explicit queue/event model instead of direct UI calls.


## 2026-07-04 — Stable Refactor Progress Continued

### Current Stable State
- main.c reduced to 6036 lines.
- OTA verified after latest cleanup.
- Known-good rollback point confirmed after failed Moonraker scan extraction attempt.

### Completed Since Last Log
- Removed duplicate cancel confirmation popup from main.c.
- Cancel confirmation popup is now fully owned by ui_printer_popups.
- Added printer_popup_send_gcode_bridge() so ui_printer_popups can use a void callback while moonraker_send_gcode() remains bool-returning.
- Removed stale cancel popup declarations and legacy state from main.c.
- Removed unused/dead helpers:
  - topbar_wifi_icon
  - old_clock_timer_cb_unused
  - fan_value_color
  - nav_btn_event_cb
  - old_update_topbar_status_icons_unused
  - make_printer_info_compact_w
  - printer_action_event_cb

### Verification
- Build passed.
- OTA worked.
- Runtime remained stable.

### main.c Progress
- Original baseline: 6981 lines
- Current: 6036 lines
- Total reduction: 945 lines

### Notes
Moonraker scan extraction was rolled back and should not be retried as a simple move. It needs a proper queued/event-driven UI update model because the scan task must not directly create or mutate LVGL objects.

### Next Investigation
Look at thumbnail/file preview ownership:
- ui_preview_v32
- thumbnail manager
- printer file detail popup
- selected file preview flow

Goal is to determine whether preview ownership can move cleanly before attempting the larger printer file browser extraction.


## 2026-07-04 — Files Popup Shell Extracted

### Completed
- Moved Files popup shell into ui_files_v32.
- ui_files_v32 now owns:
  - Files popup object
  - Files list object
  - Refresh button
  - File row/button creation
  - File UI status text
- main.c still owns:
  - Moonraker file HTTP fetch
  - JSON parsing
  - selected_print_file
  - thumbnail/metadata logic
  - file detail popup
  - print start

### Recovery Notes
Initial extraction caused:
- Files tab opening with no files
- Refresh returning to Dashboard
- One stack protection crash from recursive refresh path

Resolved by:
- Adding refresh/select bridge callbacks
- Wiring callbacks before Files UI entry
- Removing recursive ui_files_v32_refresh() from legacy_printer_load_files_now()
- Loading files on Files tab open

### Verification
- Build passed.
- OTA worked.
- Files tab opens and loads files.
- Refresh reloads files and stays on Files.
- File selection works.


## 2026-07-05 — Files Detail Popup Extracted

### Completed
- Moved Files detail popup UI into ui_files_v32.
- ui_files_v32 now owns:
  - Files popup shell
  - Files list
  - Refresh button
  - File row buttons
  - File detail popup layout
  - Detail popup Cancel/Start buttons
  - Detail popup thumbnail container

### main.c Still Owns
- selected_print_file
- metadata generation
- thumbnail download/render state
- start print HTTP request
- Moonraker file list fetch/parsing
- detail popup behavior bridges

### Verification
- Build passed.
- OTA worked.
- Files page loads files.
- Refresh works.
- File selection opens detail popup.
- Detail popup was frozen as verified.

### Current Size
- main.c: 5862 lines
- ui_files_v32.c: 284 lines


## 2026-07-05 — Printer Files Metadata Formatter Extracted

### Completed
- Created printer_files.c / printer_files.h.
- Moved metadata parsing and display text formatting out of main.c.
- Moved seconds-to-HH:MM formatting into printer_files.
- main.c now only fetches raw metadata through moonraker.c, then asks printer_files to format display text.

### Verified
- Build passed.
- OTA worked.
- Files page still works.
- Detail popup still shows metadata.
- Thumbnail path detection still works.
- Cancel/Start behavior still works.

### Current Files Subsystem Ownership
- ui_files_v32.c:
  - Files page UI
  - File rows
  - Detail popup UI
- moonraker.c:
  - File list HTTP
  - File metadata HTTP
  - Start print HTTP
- printer_files.c:
  - Metadata parsing
  - Metadata display text formatting
  - ETA formatting
- main.c:
  - selected_print_file
  - thumbnail workflow
  - application coordination

### Size
- main.c: 5750 lines
- printer_files.c: 81 lines
- moonraker.c: 398 lines
- ui_files_v32.c: 284 lines

### Progress
- main.c reduced by approximately 1231 lines from the original ~6981-line baseline.


## 2026-07-05 — Dead Files Callback Removed

Removed obsolete legacy callback:

- printer_file_selected_cb()

Verified active file selection path remains:

ui_files_v32
→ files_select_bridge()
→ show_printer_file_detail_popup()

Validation:
- build passed
- OTA verified
- hardware verified

No behavior change intended.
No rollback required.

## 2026-07-05 — Dead Files Callback Removed

Removed obsolete legacy callback:

- printer_file_selected_cb()

Verified active file selection path remains:

ui_files_v32
→ files_select_bridge()
→ show_printer_file_detail_popup()

main.c reduced to 5733 lines.

Validation:
- build passed
- OTA verified
- hardware verified

No behavior change intended.
No rollback required.

## 2026-07-05 — Dead Files Callback Removed

Removed obsolete legacy callback:

- printer_file_selected_cb()

Active file selection path remains:

ui_files_v32
→ files_select_bridge()
→ show_printer_file_detail_popup()

main.c reduced to 5733 lines.

Validation:
- build passed
- OTA verified
- Files page verified on hardware

No behavior change intended.
No rollback required.

## 2026-07-05 — File List Parser Extracted

Moved inline Moonraker file-list path parsing out of main.c and into printer_files.c.

New function:
- printer_files_for_each_path()

main.c now coordinates:
- fetch file list from Moonraker
- pass response body to printer_files parser
- add each parsed path to ui_files_v32

Ownership improved:
- printer_files owns file-list path parsing
- ui_files_v32 owns file list UI
- moonraker owns HTTP file-list API
- main.c owns application flow

Validation:
- build passed
- OTA verified
- Files refresh/select verified on hardware

No rollback required.

## 2026-07-05 — Thumbnail URL Encoder Extracted

Moved thumbnail URL encoding out of main.c and into thumbnail_manager_v32.

New owner:
- thumbnail_manager_v32_url_encode()

Updated:
- main.c thumbnail download path now calls thumbnail_manager_v32_url_encode()
- thumbnail_manager_v32.c added to CMake SRCS

main.c reduced to 5702 lines.

Validation:
- build passed
- OTA verified
- thumbnail behavior verified on hardware

No rollback required.

## 2026-07-05 — Thumbnail SD Cache Load Extracted

Moved raw thumbnail cache-file loading out of main.c and into thumbnail_manager_v32.

New function:
- thumbnail_manager_v32_load_cache_file()

main.c still owns:
- selected file coordination
- LVGL image descriptor setup
- thumbnail ready/failed flags
- popup/dashboard/printer render flow

thumbnail_manager_v32 now owns:
- thumbnail URL encoding
- thumbnail cache path generation
- raw SD cache-file loading

main.c reduced to 5663 lines.

Validation:
- build passed
- OTA verified
- Files popup thumbnail verified on hardware

No rollback required.

## 2026-07-05 — Thumbnail SD Cache Store Extracted

Moved raw thumbnail cache-file writing out of main.c and into thumbnail_manager_v32.

New function:
- thumbnail_manager_v32_store_cache_file()

thumbnail_manager_v32 now owns:
- thumbnail URL encoding
- thumbnail cache path generation
- raw SD cache-file loading
- raw SD cache-file storing

main.c still owns:
- selected file coordination
- current PNG buffer ownership
- LVGL image descriptor setup
- thumbnail ready/failed flags
- popup/dashboard/printer render flow

main.c reduced to 5650 lines.

Validation:
- build passed
- OTA verified
- popup thumbnail verified on hardware

No rollback required.

## 2026-07-05 — Moonraker Thumbnail Fetch Extracted

Moved thumbnail HTTP download transport out of main.c and into moonraker.c.

New function:
- moonraker_fetch_thumbnail_encoded()

Ownership improved:
- moonraker owns Moonraker HTTP thumbnail fetch
- thumbnail_manager_v32 owns URL encoding and SD cache helpers
- main.c owns thumbnail application coordination and LVGL image descriptor setup

main.c reduced to 5617 lines.

Validation:
- build passed
- OTA verified
- popup thumbnail verified on hardware

No rollback required.

## 2026-07-05 — Thumbnail PNG Install Helper Added

Consolidated duplicated thumbnail PNG buffer and LVGL image descriptor setup into:

- printer_thumb_install_png_buffer()

This is a local main.c cleanup that creates a clean seam for moving thumbnail image-state ownership later.

main.c reduced to 5614 lines.

Validation:
- build passed
- OTA verified
- popup thumbnail verified on hardware

No rollback required.

## 2026-07-05 — Thumbnail PNG Presence Helper Added

Added local helper:

- printer_thumb_has_png()

Replaced repeated direct PNG buffer checks with the helper.

Purpose:
- creates a clean seam for future thumbnail state ownership transfer
- reduces direct coupling to printer_thumb_png / printer_thumb_png_len
- prepares for eventual thumbnail_manager_v32 PNG-state API

Validation:
- build passed
- OTA verified
- popup thumbnail verified on hardware

No rollback required.

## 2026-07-05 — Thumbnail Image Descriptor Accessor Added

Added local helper:

- printer_thumb_image_dsc()

Replaced direct image descriptor read sites with the helper.

Purpose:
- reduces direct coupling to printer_thumb_img_dsc
- creates a seam for moving thumbnail image descriptor ownership into thumbnail_manager_v32 later

Validation:
- build passed
- OTA verified
- popup thumbnail verified on hardware

No rollback required.

## 2026-07-05 — Thumbnail PNG Accessors Added

Added local accessors:

- printer_thumb_png_data()
- printer_thumb_png_size()

Replaced direct read sites for PNG data and length.

Purpose:
- reduces direct coupling to printer_thumb_png / printer_thumb_png_len
- prepares for future transfer of PNG buffer ownership into thumbnail_manager_v32

Validation:
- build passed
- OTA verified
- popup thumbnail verified on hardware

No rollback required.

## 2026-07-05 — Thumbnail PNG Free Helper Moved

Moved PNG buffer free/reset helper into thumbnail_manager_v32.

New function:
- thumbnail_manager_v32_free_png_buffer()

main.c still owns:
- current PNG pointer
- current PNG length
- LVGL image descriptor reset
- render coordination

thumbnail_manager_v32 now owns:
- URL encoding
- cache path generation
- SD cache load/store
- PNG buffer free helper

Validation:
- build passed
- OTA verified
- popup thumbnail verified on hardware

No rollback required.

## 2026-07-05 — Thumbnail PNG Set Helper Moved

Moved PNG buffer assignment helper into thumbnail_manager_v32.

New function:
- thumbnail_manager_v32_set_png_buffer()

main.c no longer assigns printer_thumb_png / printer_thumb_png_len directly in the install path.

Validation:
- build passed
- OTA verified
- popup thumbnail verified on hardware

No rollback required.

## 2026-07-05 — Thumbnail Ready/Failed Accessors Added

Added local accessors:

- printer_thumb_is_ready()
- printer_thumb_has_failed()

Replaced direct ready/failed read sites.

Purpose:
- reduces direct coupling to printer_thumb_ready / printer_thumb_failed
- prepares for eventual thumbnail_manager_v32 state ownership

Validation:
- build passed
- OTA verified
- popup thumbnail verified on hardware

No rollback required.

## 2026-07-06 — Thumbnail Manager Ownership Complete (Phase 2)

### Objective
Continue migrating thumbnail ownership from `main.c` into `thumbnail_manager_v32` using the established incremental workflow.

### Completed

#### PNG Buffer Ownership
Moved PNG buffer ownership completely into `thumbnail_manager_v32`.

Manager now owns:
- PNG buffer
- PNG length
- Allocation/free
- Buffer installation
- Buffer queries

Added:
- `thumbnail_manager_v32_png_data()`
- `thumbnail_manager_v32_png_size()`
- `thumbnail_manager_v32_has_png()`
- `thumbnail_manager_v32_clear_png()`
- `thumbnail_manager_v32_take_png()`

---

#### Image Descriptor Ownership
Moved the thumbnail `lv_image_dsc_t` into `thumbnail_manager_v32`.

Added:
- `thumbnail_manager_v32_image_dsc()`
- `thumbnail_manager_v32_prepare_raw_image()`

The thumbnail manager now owns the complete image descriptor.

---

#### Wrapper Cleanup
Removed obsolete forwarding wrappers from `main.c`:

- `printer_thumb_png_data()`
- `printer_thumb_png_size()`
- `printer_thumb_image_dsc()`
- `printer_thumb_has_png()`

Thumbnail users now call the manager directly.

---

### Verification

Verified after each extraction:

- ✓ Build successful
- ✓ OTA successful
- ✓ Selected file thumbnail
- ✓ Active print thumbnail
- ✓ Dashboard thumbnail
- ✓ Printer page thumbnail
- ✓ Popup thumbnail
- ✓ Cache load/store
- ✓ Missing-thumbnail handling

No regressions observed.

---

### Current Ownership

**thumbnail_manager_v32**

Owns:
- Thumbnail state
- PNG buffer
- PNG length
- `lv_image_dsc_t`
- Cache helpers
- URL encoding
- Thumbnail memory management

**main.c**

Still owns:
- Download workflow
- Download task
- Ready/failed state
- UI updates
- Poll timer
- Decoder orchestration

---

### Milestone

`main.c` no longer owns any thumbnail memory or image objects.

The thumbnail manager now owns all thumbnail data.

Future work shifts from moving state to moving workflow.

---

### Workflow

The proven development process remains:

1. Small extraction
2. Build
3. OTA
4. Runtime verification
5. Freeze
6. Continue

This process has continued to produce regression-free refactors.

---

### Next Target

Move thumbnail workflow (download, cache, and decode orchestration) into `thumbnail_manager_v32` so `main.c` becomes a consumer of the thumbnail manager rather than implementing thumbnail behavior itself.


## 2026-07-06 — Thumbnail Download Workflow and Task Runner Extraction

### Objective
Continue moving thumbnail behavior out of `main.c` and into `thumbnail_manager_v32` after the manager had already taken ownership of PNG memory, image descriptor state, and selected-cache helpers.

### Completed

#### Download Workflow Moved
Moved the thumbnail download workflow into `thumbnail_manager_v32`.

Added:

- `thumbnail_manager_v32_download_ram()`

This manager function now owns:

- thumbnail path validation
- SD cache lookup
- thumbnail URL encoding
- Moonraker thumbnail fetch
- PNG install
- LVGL image descriptor preparation
- SD cache store
- download result reporting

`main.c` now calls the manager instead of implementing the workflow directly.

#### Task Argument Runner Moved
Added a reusable task-argument runner in `thumbnail_manager_v32`.

Added:

- `thumbnail_manager_v32_run_download_task_arg()`
- `thumbnail_manager_v32_download_cb_t`

The manager now owns copying/parsing the task argument path before invoking the download callback.

### Verification

Verified with:

- successful build
- successful OTA
- runtime thumbnail behavior working

### Current State

`main.c` still owns:

- `printer_thumb_download_task()`
- `printer_thumb_download_ram()` bridge
- ready/failed flags
- task-running flag
- force-refresh flag
- UI polling/rendering

`thumbnail_manager_v32` now owns:

- PNG memory
- PNG length
- LVGL image descriptor
- raw image preparation
- cache path generation
- cache load/store
- selected-file cache helpers
- URL encoding
- Moonraker thumbnail download workflow
- task argument path handling

### Notes

This was the first real thumbnail workflow extraction, not just a state/helper move.

The extraction was OTA verified before continuing.

### Next Target

Remove the now-thin `printer_thumb_download_ram()` bridge and let `printer_thumb_download_task()` call a manager-owned task download entry point directly.


## 2026-07-06 — Thumbnail Download Workflow and Task Runner Extraction

### Objective
Continue moving thumbnail behavior out of `main.c` and into `thumbnail_manager_v32` after the manager had already taken ownership of PNG memory, image descriptor state, and selected-cache helpers.

### Completed

#### Download Workflow Moved
Moved the thumbnail download workflow into `thumbnail_manager_v32`.

Added:

- `thumbnail_manager_v32_download_ram()`

This manager function now owns:

- thumbnail path validation
- SD cache lookup
- thumbnail URL encoding
- Moonraker thumbnail fetch
- PNG install
- LVGL image descriptor preparation
- SD cache store
- download result reporting

`main.c` now calls the manager instead of implementing the workflow directly.

#### Task Argument Runner Moved
Added a reusable task-argument runner in `thumbnail_manager_v32`.

Added:

- `thumbnail_manager_v32_run_download_task_arg()`
- `thumbnail_manager_v32_download_cb_t`

The manager now owns copying/parsing the task argument path before invoking the download callback.

### Verification

Verified with:

- successful build
- successful OTA
- runtime thumbnail behavior working

### Current State

`main.c` still owns:

- `printer_thumb_download_task()`
- `printer_thumb_download_ram()` bridge
- ready/failed flags
- task-running flag
- force-refresh flag
- UI polling/rendering

`thumbnail_manager_v32` now owns:

- PNG memory
- PNG length
- LVGL image descriptor
- raw image preparation
- cache path generation
- cache load/store
- selected-file cache helpers
- URL encoding
- Moonraker thumbnail download workflow
- task argument path handling

### Notes

This was the first real thumbnail workflow extraction, not just a state/helper move.

The extraction was OTA verified before continuing.

### Next Target

Remove the now-thin `printer_thumb_download_ram()` bridge and let `printer_thumb_download_task()` call a manager-owned task download entry point directly.


## 2026-07-06 — Thumbnail Download Bridge Removed

### Completed
Removed the `printer_thumb_download_ram()` bridge from `main.c`.

`printer_thumb_download_task()` now calls:

- `thumbnail_manager_v32_run_download_task()`

The manager now owns the full download task path handling and thumbnail download workflow.

### Verification
- Build successful
- OTA successful
- Runtime thumbnail behavior verified

### Result
`main.c` no longer contains `printer_thumb_download_ram()`.

Thumbnail download behavior is now manager-owned. The remaining thumbnail code in `main.c` is mostly task lifecycle, ready/failed flags, polling, and UI rendering.


## 2026-07-06 — ui_thumbnail_v32 Scaffold Restored

### Completed
Restored the active `ui_thumbnail_v32` module from the prior safe scaffold freeze.

Files restored:

- `main/ui_thumbnail_v32.c`
- `main/ui_thumbnail_v32.h`

Added `ui_thumbnail_v32.c` back to `main/CMakeLists.txt`.

### Verification
- Build successful
- No runtime wiring added yet
- No OTA required for this scaffold-only step

### Notes
The restored module currently provides only basic thumbnail UI container/placeholder behavior:

- `ui_thumbnail_v32_create()`
- `ui_thumbnail_v32_set_placeholder()`
- `ui_thumbnail_v32_clear()`

No renderer was restored yet. The old experimental renderer will not be blindly restored.

### Next Target
Wire one low-risk thumbnail UI path through `ui_thumbnail_v32`, likely popup placeholder/label ownership first, before adding image rendering.


## 2026-07-06 — Files Popup Thumbnail UI Moved to ui_thumbnail_v32

### Completed
Restored and wired the active `ui_thumbnail_v32` module for the Files popup thumbnail area.

Added:

- `ui_thumbnail_v32_wrap()`
- `ui_thumbnail_v32_show_image()`

The popup thumbnail container now uses `ui_thumbnail_v32` for:

- placeholder text
- loaded thumbnail image display
- existing-container wrapping

Removed stale popup globals from `main.c`:

- `printer_thumb_label`
- `printer_thumb_img`

### Verification
- Build successful
- OTA successful
- Files popup opens cleanly
- Placeholder remains centered
- Thumbnail image displays correctly
- Popup close/reopen works
- No scrolling/nested-card issue observed after switching to `ui_thumbnail_v32_wrap()`

### Notes
The first attempt used `ui_thumbnail_v32_create()` and created a nested card inside the popup thumbnail box. This worked but caused layout/scroll behavior. The corrected approach uses `ui_thumbnail_v32_wrap()` so the module attaches to the existing popup thumbnail container.

### Current Direction
`thumbnail_manager_v32` owns thumbnail backend behavior.
`ui_thumbnail_v32` now owns Files popup thumbnail presentation.
`main.c` continues shrinking toward orchestration only.


-------------------------------------------------------------------------------
2026-07-07 - Dashboard Thumbnail Ownership Refactor
-------------------------------------------------------------------------------

GOAL

Continue migrating Dashboard thumbnail ownership out of main.c and into the
new v3.2 UI modules while preserving existing behavior.

RESULT

Completed another major ownership migration with successful build and OTA
verification after every step.

OWNERSHIP MIGRATION

Dashboard thumbnail placeholder

- Moved placeholder ownership into ui_dashboard_v32.
- main.c no longer creates or owns the placeholder label.

Added:

- ui_dashboard_v32_thumb_set_placeholder()
- ui_dashboard_v32_thumb_clear_placeholder()

Verified:

- Build OK
- OTA OK
- Placeholder displayed correctly

-------------------------------------------------------------------------------

Dashboard thumbnail canvas state

Moved ownership of:

- dashboard canvas object
- dashboard canvas buffer
- dashboard canvas filename tracking

from:

    main.c

to:

    ui_dashboard_v32

Rendering behavior remained unchanged.

Verified:

- Build OK
- OTA OK
- Dashboard thumbnail continued rendering

-------------------------------------------------------------------------------

Dashboard thumbnail rendering

Refactored rendering helpers to use dashboard-owned canvas APIs instead of
directly manipulating dashboard LVGL objects from main.c.

Verified:

- PNG decoding still functional
- Thumbnail restore still functional
- Dashboard updates correctly

Example log:

DASH_CANVAS aspect-fit 400x300 -> 286x214 inside 286x215

-------------------------------------------------------------------------------

Dashboard layout investigation

Observed visual issue:

Dashboard thumbnail appeared to be sitting on another card.

Investigation determined:

- Legacy dashboard body was already disabled.
- Overlap originated from the new dashboard layout.

Temporary positioning confirmed the issue.

-------------------------------------------------------------------------------

Architectural decision

The standalone Dashboard thumbnail card will be removed.

New intended hierarchy:

Dashboard
├── Status Banner
├── Active Print Card
│   └── Thumbnail
├── Machine Status
└── Command Bar

The thumbnail becomes part of the Active Print widget instead of an independent
Dashboard widget.

-------------------------------------------------------------------------------

Active Print ownership

Added:

    ui_active_print_v32_thumb_box()

Dashboard now requests its thumbnail container from the Active Print widget
instead of constructing one itself.

Current ownership flow:

main.c
    ↓
ui_dashboard_v32
    ↓
ui_active_print_v32

This matches the long-term v3.2 ownership model.

-------------------------------------------------------------------------------

Current status

Current logs confirm:

- PNG loading works
- PNG decoding works
- Aspect-fit rendering works
- Dashboard canvas updates correctly

Example:

DASH_CANVAS aspect-fit 400x300 -> 286x214 inside 286x215

The remaining work is now layout integration inside ui_active_print_v32 rather
than rendering or ownership.

No regressions observed.

-------------------------------------------------------------------------------

Next session

- Complete thumbnail integration into ui_active_print_v32.
- Remove remaining legacy thumbnail assumptions.
- Resize/reposition thumbnail within the Active Print card.
- Continue simplifying main.c by moving thumbnail behavior into UI modules.


-------------------------------------------------------------------------------
2026-07-07 - Active Print Wrapper Extraction
-------------------------------------------------------------------------------

GOAL

Continue extracting Dashboard behavior into v3.2 UI modules without changing
runtime behavior.

RESULT

Added a Dashboard-level wrapper for Active Print footer updates.

New API:

    ui_dashboard_v32_set_active_print(layer, elapsed, remaining)

This wraps:

    ui_active_print_v32_set(...)

WHY

The Dashboard should own how it talks to its child widgets.

This keeps main.c and higher-level code from needing to know about the internal
Active Print LVGL object.

CURRENT FLOW

main.c
    ↓
ui_dashboard_v32
    ↓
ui_active_print_v32

VERIFICATION

- Build OK
- OTA OK
- No runtime regression observed

NOTES

This was intentionally small and safe after the failed attempt to move thumbnail
canvas lifetime directly into ui_active_print_v32.

The lesson from that failure:

    Move widget behavior first.
    Move LVGL object lifetime later.

NEXT

Continue making ui_active_print_v32 a complete widget by adding behavior APIs,
such as:

- set filename
- set progress
- set ETA / remaining
- set elapsed
- set thumbnail state

Avoid moving canvas/buffer lifetime again until the widget API boundary is fully
established and verified.


## 2026-07-09 / 2026-07-10 — Thumbnail, Active Print, and Splash Ownership

### Active Print Thumbnail Integration
- Moved dashboard thumbnail preview into the Active Print card.
- Removed reliance on the old standalone dashboard thumbnail card.
- Active Print card now owns the thumbnail container, placeholder, canvas refs, canvas buffer, and preview file bookkeeping.
- Dashboard thumbnail APIs now forward to the Active Print module.
- Expanded the thumbnail preview horizontally inside the Active Print card.
- Verified cached SD thumbnails render correctly after migration.

### Thumbnail Manager Ownership
- Continued moving thumbnail state out of main.c.
- thumbnail_manager_v32 now owns:
  - PNG buffer
  - PNG length
  - LVGL image descriptor
  - cache path generation
  - SD cache load/store helpers
  - selected-file cache helpers
- LVGL image decoder now uses thumbnail_manager_v32_image_dsc().
- Verified build/OTA after thumbnail state migration.
- Resolved temporary reboot/duplicate-function issue after thumbnail_render_v32 extraction.

### Dashboard API Cleanup
- Added dashboard wrapper APIs for live banner data.
- Added dashboard wrapper APIs for machine status data.
- Added dashboard/active-print forwarding APIs for thumbnail placeholder/canvas ownership.
- main.c now increasingly pushes state instead of owning dashboard presentation details.

### Splash Ownership
- Moved splash screen object ownership out of main.c into ui_splash_v32.
- ui_splash_v32 now owns:
  - splash creation
  - splash destruction
  - splash progress rendering
  - splash progress percentages
  - splash status strings
- Added named splash stage APIs:
  - ui_splash_v32_display_ready()
  - ui_splash_v32_wifi_starting()
  - ui_splash_v32_wifi_waiting(bool connected)
  - ui_splash_v32_moonraker_ready()
  - ui_splash_v32_dashboard_ready()
- main.c no longer contains hard-coded splash progress text or percentages.
- main.c only decides when each splash stage occurs.

### Verification
- Build verified.
- OTA verified.
- Splash progression verified on hardware.
- Dashboard startup still works.
- WiFi/Moonraker startup behavior unchanged.
- Thumbnail rendering unchanged.


## 2026-07-10 — Splash Encapsulation Complete

### Splash API Cleanup
- Removed ui_splash_v32_set_progress() from the public header.
- Made ui_splash_v32_set_progress() private/static inside ui_splash_v32.c.
- Public splash API now exposes only semantic lifecycle/stage calls.
- No external module can set arbitrary splash percentages or status text.

### Verification
- Build verified.
- OTA/hardware verified.
- Splash behavior unchanged.


## 2026-07-10 — Display Start Helper Rolled Back

- Tried extracting BSP display startup into app_display_start().
- Build/OTA completed, but repeated HP_SYS_HP_WDT_RESET occurred during WiFi/hosted startup.
- Rolled back to inline BSP display startup block inside app_main().
- app_startup_show_initial_ui() remains intact and verified.
- Decision: keep BSP display startup inline for now because the P4/C6 startup path appears timing-sensitive.


## 2026-07-10 — Dashboard Status Panel Ownership

Completed and OTA verified:

- Created `ui_dashboard_status_v32.c/.h`.
- Moved Dashboard right status panel creation out of `main.c`.
- Moved status/progress/time update behavior into `ui_dashboard_status_v32`.
- Removed leftover duplicate nozzle/bed widget block from `main.c`.
- Verified after each stage with `idf.py build` + OTA.
- Current good state: Dashboard visually OK after OTA.

Next:
- Remove remaining bridge globals in `main.c`:
  - `dash_state_label`
  - `dash_progress_label`
  - `dash_progress_bar`
  - `dash_elapsed_label`
  - `dash_remaining_label`
  - `dash_eta_label`
- Then move nozzle/bed update behavior into `ui_dashboard_status_v32`.

### Dashboard Status Step 4 Rollback

Attempted:
- Moved nozzle/bed temperature updates into `ui_dashboard_status_v32`.

Result:
- Build/OTA succeeded, but device rebooted after WiFi/IP startup.
- Rolled back Step 4 only.
- Restored Step 3 known-good state.

Current verified state:
- `ui_dashboard_status_v32` owns right-panel creation.
- `ui_dashboard_status_v32` owns state/progress/time updates.
- Nozzle/bed updates still remain in `main.c` through bridge handles.

### Temperature Setter Retry Failed

Attempted safer Step 4A:
- Added `ui_dashboard_status_v32_set_temperatures()` only.
- Did not route `main.c` through it.

Result:
- Build/OTA succeeded, but boot crash returned.
- Rolled back 4A.

Decision:
- Freeze dashboard status module at Step 3 verified state.
- Leave nozzle/bed bridge handles in `main.c` for now.
- Revisit later with deeper crash log / map-size inspection.


-------------------------------------------------------------------------------

## 2026-07-10 – Active Print filename ownership migration (COMPLETE)

### Objective

Move ownership of the Active Print filename display out of `main.c`
and into the Dashboard / Active Print modules using the incremental
refactor workflow.

### Completed

#### Step 1
Added filename ownership to `ui_active_print_v32`.

- Added private filename label.
- Added `ui_active_print_v32_set_filename()`.
- No routing changes.

Result:
- Build ✓
- OTA ✓
- Visual verification ✓

---

#### Step 2
Added Dashboard wrapper.

Added:

    ui_dashboard_v32_set_active_print_file()

Wrapper delegates directly to:

    ui_active_print_v32_set_filename()

Result:
- Build ✓

---

#### Step 3
Redirected normal runtime filename updates.

Replaced direct dashboard label updates with:

    ui_dashboard_v32_set_active_print_file(printer_file);

Result:
- Build ✓
- OTA ✓

---

#### Step 4
Redirected print-complete reset.

Replaced:

    lv_label_set_text(dash_job_label, "No active file");

with:

    ui_dashboard_v32_set_active_print_file("No active file");

Result:
- Build ✓
- OTA ✓

---

#### Step 5
Removed legacy dashboard filename ownership.

Removed:

- dash_job_label global
- dash_job_label creation
- remaining runtime writes

Result:
- Build ✓
- OTA ✓

### Final ownership

    main.c
        ↓
    ui_dashboard_v32_set_active_print_file()
        ↓
    ui_active_print_v32_set_filename()
        ↓
    Active Print filename label

### Outcome

Filename presentation is now fully owned by the Active Print module.

`main.c` no longer contains dashboard filename presentation logic.

Thumbnail rendering, footer rendering, and printer behavior were
unchanged throughout the migration.

Every step followed the project workflow:

    smallest change
        ↓
    build
        ↓
    OTA
        ↓
    verify
        ↓
    continue

### Next planned refactor

Do not split the Active Print footer.

Instead:

1. Move `progress_value_color()` into `ui_dashboard_status_v32`.

2. Extract the dashboard live-status formatting block
   (progress, elapsed, remaining, ETA formatting)
   into a dedicated presentation module.

This will remove another isolated responsibility from `main.c`
while preserving the current ownership boundaries.




## 2026-07-10 — Dashboard Status Cleanup + Printer Info Card Ownership

### Goal

Continue reducing `main.c` by moving presentation ownership into existing UI modules while preserving behavior through the established **smallest change → build → OTA → verify** workflow.

---

## Completed

### 1. Active Print filename ownership (completed previously, verified)

The Active Print filename presentation migration remains complete and verified.

Dashboard filename updates now flow through:

* `ui_dashboard_v32_set_active_print_file()`

instead of directly manipulating UI objects from `main.c`.

Status: **Build + OTA verified**

---

### 2. Dashboard progress color ownership

Moved dashboard progress color presentation policy from `main.c` into:

* `ui_dashboard_status_v32.c`

Added:

```c
lv_color_t ui_dashboard_status_v32_progress_color(double progress);
```

`main.c` now delegates dashboard progress color selection to the dashboard status module.

Status: **Build + OTA verified**

---

### 3. Dashboard status refresh ownership

Extracted dashboard progress/time formatting into:

```c
ui_dashboard_status_v32_refresh(
    double progress,
    double print_duration_seconds);
```

The dashboard status module now owns:

* progress text formatting
* progress percentage conversion
* elapsed formatting
* remaining formatting
* ETA formatting
* progress color selection
* dashboard widget updates

`main.c` now passes raw printer state instead of formatting dashboard presentation.

Status: **Build + OTA verified**

---

### 4. Removed duplicate dashboard formatting

After introducing `ui_dashboard_status_v32_refresh()`, the original elapsed/remaining/ETA formatting block inside `update_live_cards()` became redundant.

Removed the duplicate formatter while retaining:

```c
ui_dashboard_status_v32_set_print_state(
    dash_print_state_text());
```

Result:

* one authoritative dashboard status update path
* no duplicate formatting logic

Status: **Build + OTA verified**

---

### 5. Printer card icon ownership

Moved printer card icon presentation helpers from `main.c` into:

* `ui_printer_info_cards.c`

Private helpers:

* `card_icon_for_title()`
* `card_color_for_title()`

Public API:

```c
ui_printer_info_cards_add_vivid_icon(...)
```

`main.c` now delegates icon creation to the printer info card module.

Build initially failed because the extracted code required:

```c
#include <string.h>
```

Added the missing include.

Status:

* Build fixed
* OTA verified

---

### 6. Printer information card builder ownership

Discovered that card construction consisted of two builders:

```c
make_printer_info_w()
make_printer_info()
```

Both were extracted together into:

* `ui_printer_info_cards.c`

Public APIs:

```c
ui_printer_info_cards_make_sized(...)
ui_printer_info_cards_make(...)
```

The fixed-width builder now delegates internally to the sized builder.

Several extraction scripts required refinement because the original assumptions no longer matched the evolving source tree:

* helper became wrapper + implementation pair
* repeated legacy comment markers caused incorrect extraction boundaries
* extraction updated to search forward from the located function

Final extraction completed successfully.

Status:

* Build verified
* OTA verified
* Printer page verified
* Network page verified

---

## main.c reduction

Approximate progression during this session:

```
5249
 ↓
5173
```

Additional presentation ownership removed from `main.c` without behavior changes.

---

## Architecture progress

Ownership continues moving toward:

```
main.c
    ↓
application coordinator

ui_dashboard_status_v32
    dashboard presentation

ui_printer_info_cards
    printer card construction
    printer card icon policy
```

`update_live_cards()` is now primarily orchestration instead of formatting.

---

## Lessons learned

Future extraction scripts should avoid relying on legacy comment markers.

Preferred strategy:

* locate functions by signature
* use brace matching
* search forward from the located function
* avoid fixed line numbers

This proved significantly more robust as wrappers and helper functions continue to evolve.

---

## Current status

Known-good checkpoint.

All changes completed today have been:

* built
* OTA installed
* runtime verified

No functional regressions observed.

Ready for the next refactor session.


## Session — Printer popup ownership completed

### Printer popup extraction completed

Completed the remaining printer temperature/fan popup ownership transition.

The popup implementation that previously remained in `main.c` was removed after verifying that equivalent functionality already existed in `ui_printer_popups`.

Removed legacy implementations:

```c
temp_motion_close_popup_event_cb()
gcode_button_event_cb()
popup_button()
show_temp_popup()
```

The printer card callbacks were reduced to thin event dispatchers:

```c
part_fan_card_event_cb()
nozzle_card_event_cb()
bed_card_event_cb()
```

Each callback now delegates directly to:

```c
ui_printer_popups_show_part_fan()
ui_printer_popups_show_nozzle()
ui_printer_popups_show_bed()
```

All popup presentation responsibilities now reside inside:

```
ui_printer_popups.c
```

including:

* popup construction
* styling
* status formatting
* preset button creation
* G-code dispatch
* popup close handling

`main.c` no longer contains popup presentation code.

---

### Verification

Completed successfully.

* Build successful
* OTA successful
* Nozzle popup verified
* Bed popup verified
* Part Fan popup verified
* Close button verified
* Motion popup verified
* No regressions observed

---

## main.c reduction

```
5173
 ↓
5027
```

**146 additional lines removed.**

`main.c` continues moving toward the intended role of application coordinator rather than UI implementation.

---

## Architecture progress

Ownership now follows:

```
main.c
    ↓
event routing

ui_printer_popups
    popup presentation
    popup layout
    popup styling
    popup status formatting
    popup command generation

ui_printer_motion
    motion popup
```

The printer page now delegates nearly all popup presentation to dedicated UI modules.

---

## Current status

Known-good checkpoint.

All work completed during this session has been:

* built
* OTA installed
* runtime verified

No functional regressions observed.

The next recommended extraction target is one of the remaining standalone UI builders or Moonraker/network presentation helpers still residing in `main.c`.


---

## Session — Printer popups and Moonraker discovery modularization

### Printer popup ownership completed

Removed the remaining legacy nozzle, bed, and part-fan popup implementation from `main.c`.

`ui_printer_popups` now owns:

- popup construction
- popup styling
- status formatting
- preset button creation
- G-code dispatch
- close handling

`main.c` retains only thin card event callbacks.

Verification:

- Build successful
- OTA successful
- Nozzle popup verified
- Bed popup verified
- Part Fan popup verified
- Motion popup verified
- No regressions observed

### Dashboard printer-status popup

Moved printer-status body formatting into:

```text
ui_printer_popups.c


---

## Pre-Stage-2B freeze — 20260710_234153

Known-good checkpoint before moving the Moonraker discovery scan task.

- Build verified
- OTA verified
- Discovery verified
- `main.c`: 4926 lines
- Freeze: `/home/khelix/P4/PrinterHMI_v3_2_FREEZE_PRE_STAGE2B_20260710_234153.tar.gz`



---

## Session — Printer popup, Moonraker discovery, and OTA ownership

### Printer popup ownership completed

Removed the remaining legacy nozzle, bed, and part-fan popup implementation from `main.c`.

`ui_printer_popups` now owns:

* popup construction
* popup styling
* status formatting
* preset button creation
* G-code dispatch
* popup close handling

`main.c` retains only thin event-routing callbacks.

Verification:

* Build successful
* OTA successful
* Nozzle popup verified
* Bed popup verified
* Part Fan popup verified
* Motion popup verified
* No regressions observed

---

### Dashboard printer-status popup

Moved printer-status popup body construction into:

```text
ui_printer_popups.c
```

`main.c` now formats elapsed, remaining, and progress values and delegates presentation through:

```c
ui_printer_popups_show_printer_status()
```

Verification:

* Build successful
* OTA successful
* Dashboard printer-status popup verified
* No regressions observed

---

## Moonraker discovery modularization

### Stage 1 — Discovery UI ownership

Created:

```text
moonraker_discovery.c
moonraker_discovery.h
```

The module initially took ownership of:

* discovery popup lifetime
* popup rendering
* status label
* status updates
* candidate buttons
* candidate-selection routing
* close handling
* cancellation state

`main.c` temporarily retained the network probe, discovery algorithm, and RTOS task.

Verification:

* Build successful
* OTA successful
* Discovery popup verified
* Candidate buttons verified
* Host selection and save verified
* Scan cancellation verified
* Popup reopening verified
* Test Moonraker verified

---

### Stage 2A — Moonraker HTTP probe

Created:

```text
moonraker_probe.c
moonraker_probe.h
```

Moved Moonraker `/server/info` probing out of `main.c`.

The new probe module owns:

* probe-specific HTTP response capture
* request timeout
* HTTP response validation
* Moonraker response identification

The discovery algorithm now calls:

```c
moonraker_probe_host(host, 7125);
```

Verification:

* Build successful
* OTA successful
* Host detection verified
* Host selection verified
* Cancellation verified
* No regressions observed

---

### Stage 2B — Discovery algorithm and RTOS task ownership

Moved from `main.c` into `moonraker_discovery.c`:

```c
moon_host_already_seen()
moon_scan_task()
```

The discovery module now also owns:

* scan task handle
* copied scan IP address
* candidate generation
* duplicate-host filtering
* network scan algorithm
* Moonraker probe calls
* task creation
* task lifetime
* completion status
* cancellation handling

New module APIs:

```c
moonraker_discovery_start()
moonraker_discovery_is_running()
```

`main.c` now retains only the high-level scan launcher and host-selection persistence bridge.

A known-good pre-Stage-2B freeze was created:

```text
/home/khelix/P4/PrinterHMI_v3_2_FREEZE_PRE_STAGE2B_20260710_234153.tar.gz
```

Verification:

* Build successful
* OTA successful
* Scan start verified
* Live status updates verified
* Host discovery verified
* Candidate selection and save verified
* Scan cancellation verified
* Popup reopening verified
* Test Moonraker verified
* No regressions observed

---

## OTA modularization

### Stage 3A — OTA progress UI ownership

Moved the OTA progress popup presentation into:

```text
ui_ota_popup.c
```

The module now owns:

* progress popup object
* progress bar
* status label
* percentage label
* byte-count label
* popup construction
* progress rendering
* MB formatting

New APIs:

```c
ui_ota_progress_show()
ui_ota_progress_pump()
```

`main.c` no longer owns OTA progress widgets or progress-popup rendering.

Verification:

* Build successful
* OTA successful
* Progress popup verified
* Status updates verified
* Percentage updates verified
* Progress bar verified
* Downloaded-byte display verified
* Reboot after OTA verified

---

### Stage 3B — OTA manager

Created:

```text
ota_manager.c
ota_manager.h
```

Moved out of `main.c`:

* OTA HTTP event handler
* OTA update task
* OTA task URL
* running state
* progress state
* byte counters
* content-length tracking
* OTA task creation
* success reboot handling
* failure handling

Public APIs:

```c
ota_manager_start()
ota_manager_is_running()
ota_manager_pump_ui()
```

Current OTA ownership:

```text
ui_ota_popup
    OTA server-entry popup
    OTA progress popup
    progress presentation

ota_manager
    OTA task
    HTTP event handling
    progress state
    byte counters
    task lifetime
    reboot on success

main.c
    saved URL persistence
    OTA popup bridges
    high-level Settings integration
```

Verification:

* Build successful
* OTA successful
* Progress popup verified
* Download progress verified
* Completion verified
* Device reboot verified
* Application startup verified
* No regressions observed

---

## main.c reduction

Progress during this session:

```text
5173
 ↓
5027   printer popup ownership
 ↓
5023   dashboard printer-status popup
 ↓
4952   Moonraker discovery UI ownership
 ↓
4926   Moonraker HTTP probe extraction
 ↓
4779   discovery algorithm and RTOS task ownership
 ↓
4676   OTA progress UI ownership
 ↓
4574   OTA manager extraction
```

Total reduction:

```text
599 lines
```

---

## Current architecture

```text
main.c
    application coordination
    feature integration
    persistence bridges
    page-level routing

ui_printer_popups
    printer popup presentation
    preset command UI
    dashboard printer-status popup

moonraker_discovery
    discovery popup
    candidate UI
    scan algorithm
    RTOS task
    cancellation
    task lifetime

moonraker_probe
    HTTP probing
    response capture
    Moonraker identification

ui_ota_popup
    OTA server-entry UI
    OTA progress UI

ota_manager
    OTA transport
    HTTP event handling
    progress state
    OTA task
    reboot handling
```

---

## Current status

Known-good checkpoint.

All completed changes were:

* built successfully
* OTA installed
* runtime verified

Current `main.c` line count:

```text
4574
```

No functional regressions observed.

The next recommended large extraction target is the remaining thumbnail rendering and refresh orchestration still residing in `main.c`.



-------------------------------------------------------------------------------

## 2026-07-11 – Moonraker Test Transport and Network Page Extraction

### Starting point

Moonraker discovery had already been modularized and runtime verified.

The Network page still retained substantial implementation and LVGL object
ownership inside `main.c`.

### Moonraker connection-test transport extraction

Added:

    bool moonraker_test_connection(
        const char *host,
        int port,
        int *http_code,
        esp_err_t *err_out);

Ownership moved into `moonraker.c`:

- `/server/info` URL construction
- HTTP client initialization
- HTTP request execution
- HTTP status capture
- client cleanup
- transport success/failure result

`test_moonraker_now()` remains temporarily in `main.c` as a presentation
bridge responsible for formatting and showing the Network test popup.

Verified:

- Build successful
- OTA successful
- TEST MOONRAKER runtime behavior successful

### Network destroy-policy extraction

Added transitional API:

    ui_network_v32_destroy_objects(...)

Moved into `ui_network_v32.c`:

- Network panel deletion
- Network label-reference clearing
- WiFi scan popup deletion
- WiFi scan popup reference clearing

`main.c` retains a thin `ui_network_v32_destroy()` bridge while Network
object storage remains transitional.

Also removed repeated pointer-clear assignments from the old destroy block.

Verified:

- Build successful
- OTA successful
- Network page create/destroy cycles successful
- WiFi popup open/close behavior successful

### Network page-construction extraction

Added transitional page-builder API:

    ui_network_v32_create_objects(...)

Moved into `ui_network_v32.c`:

- Network panel construction
- Network title and subtitle
- status banner
- WIFI card
- IP ADDRESS card
- MOONRAKER card
- HOST card
- PORT card
- LAST HTTP card
- Network instruction box
- WIFI/HOST/PORT card callback attachment

`main.c` now retains only a thin `ui_network_v32_create()` bridge supplying:

- existing Network object references
- current Moonraker port
- shared info-card builder
- WiFi scan callback
- Moonraker discovery callback
- port editor callback

### Extraction correction

The initial automated extraction matched an earlier declaration/bridge
location instead of the later full implementation.

This temporarily produced:

- two `ui_network_v32_create()` definitions
- removal of the adjacent `hide_graphs_tab()` implementation
- missing visibility of `make_printer_info()` at the bridge location

Corrected by:

- deleting only the second legacy Network create implementation
- restoring `hide_graphs_tab()`
- adding the required forward declaration for `make_printer_info()`

Final result was rebuilt and OTA verified.

### Runtime verification

Confirmed working after OTA:

- Network page open
- Network page leave/re-enter
- WiFi scan
- Moonraker host discovery
- Moonraker port editor
- TEST MOONRAKER
- Network page visual layout

### Current ownership

`ui_network_v32.c` now owns:

- Network page construction policy
- Network page cleanup policy
- public show/hide wrapper boundary

`main.c` still temporarily owns:

- Network page LVGL object storage
- Network refresh data routing
- WiFi/password popup state
- callback implementations
- live connection state

Current `main/main.c` line count:

    4483 lines

### Next

Network Ownership Stage B:

1. Consolidate duplicated Network refresh behavior.
2. Move label-update policy into `ui_network_v32`.
3. Replace direct LVGL writes in `main.c` with a Network refresh API.
4. Move page object storage into `ui_network_v32.c`.
5. Leave WiFi/password popup ownership for a separate verified stage.

Status:

- Build verified
- OTA verified
- Runtime verified
- Safe checkpoint established


wc -l main/main.c
4391 main/main.c



2026-07-11 (Continued) — Network UI Completed / Settings Page Ownership
Session summary

This session completed the remaining Network UI ownership work and began the final phase of the UI architecture cleanup by extracting the Settings page construction.

All changes were build verified, OTA verified, and runtime verified after each milestone.

Network UI completion
WiFi popup ownership

Completed migration of WiFi popup object ownership from main.c into ui_network_tools.c.

The module now owns:

WiFi scan popup
WiFi scan status label
WiFi scan list
Selected WiFi button
Password popup
Password textarea

Added owned workflow APIs:

ui_network_tools_wifi_scan_show_owned()
ui_network_tools_wifi_scan_close_owned()

ui_network_tools_wifi_password_show_owned()
ui_network_tools_wifi_password_close_owned()

ui_network_tools_wifi_password_copy_owned()

ui_network_tools_wifi_select_owned()

ui_network_tools_wifi_scan_render_owned()

ui_network_tools_wifi_scan_set_status()

ui_network_tools_wifi_popup_destroy_all()

main.c now interacts only through the owned workflow API.

Direct LVGL popup object ownership was completely removed from main.c.

Network lifecycle

Network module now owns:

Page creation
Page destruction
Page refresh
Page object ownership
WiFi popup ownership
Password popup ownership
Popup lifecycle

Remaining application ownership intentionally stays in main.c:

Credential buffers
WiFi connection workflow
ESP-IDF WiFi operations
NVS save/load
Application state

This is the intended architecture boundary.

Runtime verification

Verified after OTA:

Network page
WiFi scan
SSID selection
Password popup
CONNECT workflow
BACK workflow
Scan CLOSE
Leave/re-enter Network
Moonraker discovery
Port editing
TEST MOONRAKER

All passed.

Settings page ownership

The remaining legacy Settings page construction was extracted into ui_settings.c.

Created:

ui_settings_show_page(...)

Responsibilities moved:

Settings panel creation
Title
Subtitle
Banner

Sleep card
Brightness card
Firmware card
Build card
Compiled card
OTA Update card
SD Card card
Storage card

System Info button
V3.2 Dashboard button
Reboot button
Reset Settings button

Sleep card callback hookup
OTA callback hookup
Dashboard callback hookup
System Info callback hookup

main.c now contains only a thin bridge:

show_settings_tab()
    ↓
ui_settings_show_page(...)

The bridge simply prepares runtime strings and forwards callbacks.

Runtime verification

Verified after OTA:

Settings page
Sleep card
OTA popup
System Info popup
V3.2 Dashboard button
Reset Settings dialog
Leave/re-enter Settings repeatedly

All passed.

Current UI ownership status
Completed
Splash
Shell
Dashboard
Dashboard Status
Active Print
Printer
Printer Popups
Printer Motion
Printer Actions
Printer Banner
Printer Live Status
Printer Info Cards

Files

Thumbnail Manager

Moonraker Transport
Moonraker Discovery
Moonraker Probe

Network Page
Network Refresh
Network Creation
Network Destruction
WiFi Popup Ownership

Settings Page
Settings Popups
OTA Popup
Current architecture

main.c responsibilities are now primarily:

Application state

ESP-IDF WiFi
ESP-IDF networking
NVS

Moonraker polling

Application startup

Application coordination

Navigation routing

Most LVGL page construction has now been removed.

Line count

Project beginning:

~6981

Beginning of today's session:

4574

After Network ownership:

4391

After Settings extraction:

4264

Overall reduction:

6981 → 4264

2717 lines removed

≈39% reduction
Next milestone

Primary remaining large subsystem:

Graphs

After Graphs, main.c should primarily function as the application coordinator, with nearly all UI construction and ownership residing in dedicated modules.


-------------------------------------------------------------------------------

## 2026-07-11 – Graph Experiment Removed / Stable Baseline Restored

### Graph UI extraction experiment

A dedicated Graph UI module was temporarily created:

    ui_graphs_v32.c
    ui_graphs_v32.h

The experiment moved the following out of `main.c`:

- Graph page construction
- Graph page destruction
- LVGL chart ownership
- LVGL series ownership
- history canvas allocation and ownership
- graph-window buttons
- live chart updates
- live label updates

The Graph UI extraction initially built, OTA updated, and passed runtime
testing.

### Graph controller extraction experiment

A second module was then created:

    graph_controller.c
    graph_controller.h

The controller temporarily owned:

- RAM humidity history
- RAM temperature history
- circular-buffer write index
- sample count
- live sample recording

After this split, the new OTA image reproducibly failed during early FreeRTOS
startup before `app_main()`.

Observed assertion:

    assert failed: vApplicationGetTimerTaskMemory
    port_common.c:97
    pxStackBufferTemp != NULL

The OTA rollback mechanism worked correctly and booted the previous valid
partition.

Investigation confirmed:

- `CONFIG_FREERTOS_SUPPORT_STATIC_ALLOCATION=y`
- `CONFIG_FREERTOS_TIMER_TASK_STACK_DEPTH=2048`
- ESP-IDF's `vApplicationGetTimerTaskMemory()` was present
- the hook was linked into `libfreertos.a`
- `esp_hosted` uses `xTaskCreateStatic()`
- static allocation support must remain enabled
- the failure occurred before application code executed

A targeted rebuild reproduced the same startup assertion.

### Decision

The complete Graph subsystem was intentionally removed rather than continuing
to debug or preserve the experimental implementation.

Removed from the active project:

    main/ui_graphs_v32.c
    main/ui_graphs_v32.h
    main/graph_controller.c
    main/graph_controller.h

Removed from `main.c`:

- Graph includes
- Graph page show/hide bridges
- Graph navigation cleanup calls
- Graph redraw handling
- Graph RAM history
- Graph sample recording
- Graph timer refresh calls
- Graph live chart updates
- all Graph LVGL object references

Removed from `main/CMakeLists.txt`:

    ui_graphs_v32.c
    graph_controller.c

Timestamped `.pre_remove_graphs_*` reference copies were retained but are not
compiled.

### Verification

After complete Graph removal:

- Build successful
- OTA successful
- New image booted normally
- FreeRTOS timer-task startup assertion did not recur
- OTA rollback was no longer triggered
- stable project baseline restored

### Current policy

Graphs are postponed.

Any future Graph implementation should be designed again from scratch with
strict separation between:

1. history/sample storage
2. rendering
3. page ownership
4. refresh scheduling

The removed implementation should be treated only as historical reference,
not as the basis for an incremental restoration.

### Current main.c line count

    3790 lines

### Next target

Moonraker polling and live-state ownership.

Goal:

- separate HTTP polling from UI coordination
- move polling cadence and transport-result processing out of `main.c`
- keep UI modules consuming formatted application state
- preserve the current build → OTA → verify workflow

Status:

- Graph subsystem intentionally removed
- Build verified
- OTA verified
- Stable baseline restored

July 11, 2026 — Files Page Stability / Memory Lessons
Files page stability fixes
Investigated repeated Files-page crashes that occurred immediately after opening the Files page.
Initial panic appeared inside snprintf(), but investigation showed the failure was actually a taskLVGL stack protection fault, with snprintf() being the first function to execute after stack corruption.
Root cause was a 4 KB automatic (char body[4096]) response buffer allocated on the LVGL task stack inside app_files_reload().
Removed the large automatic buffer from the LVGL stack.
Refactored Files response buffering to use dynamic allocation, preferring PSRAM first with an internal RAM fallback.
Confirmed Files page now opens repeatedly without stack faults.
Files page ownership cleanup
Removed UI ownership from app_files_reload().
app_files_reload() is now responsible only for loading and parsing file data.
Page visibility is now owned entirely by the navigation/refresh logic.
Fixed Refresh behavior after this ownership change:
Refresh now performs:
Hide Files page
Recreate Files page
Reload file list
Eliminated unintended return to Dashboard during refresh.
Architectural lesson learned

Large temporary buffers should not be allocated on:

LVGL task stacks
FreeRTOS task stacks
Internal static RAM (.bss) unless absolutely necessary

Preferred order for large temporary working buffers:

PSRAM (preferred)
Internal heap (fallback)
Stack only for small temporary objects

This reduces:

LVGL stack usage
Internal RAM pressure
FreeRTOS startup allocation failures
Risk of stack-protection faults
Project guideline added

When refactoring existing code, actively look for:

Large local arrays (≥1 KB)
Large static internal buffers
JSON receive buffers
HTTP response buffers
Image decode scratch buffers
Thumbnail working buffers

Evaluate each for migration to PSRAM-backed dynamic allocation where appropriate.

This should become part of the refactoring checklist for remaining modules.


Refactor Log Update — July 11, 2026 (Evening)
Freeze

Created new verified freeze after Files stability work and Stage 2 dashboard migration progress.

Current baseline is considered stable.

Files page investigation

After beginning Stage 2 migrations, opening the Files page began producing intermittent crashes.

Initial investigation suggested:

snprintf()
NULL format pointer
LVGL task crash

The actual root cause was LVGL task stack exhaustion, not snprintf() itself.

Root cause

app_files_reload() allocated a large temporary response buffer on the LVGL task stack.

This consumed several kilobytes of stack during:

HTTP download
JSON parsing
UI construction

The corrupted stack later caused failures inside snprintf(), masking the real problem.

Memory architecture improvement

Large temporary response buffers should no longer live on:

task stacks
large internal static allocations

The Files response buffer was redesigned to:

allocate dynamically
prefer PSRAM
fall back to internal heap if PSRAM allocation fails
free the buffer immediately after use

Benefits:

eliminates LVGL stack overflow
preserves internal RAM
reduces FreeRTOS startup pressure
Files ownership cleanup

app_files_reload() now owns only data loading.

Page visibility is now owned entirely by UI navigation.

Refresh sequence changed from:

Hide
Reload

to

Hide
Show
Reload

which fixed Refresh returning to the Dashboard.

Project memory guideline (NEW)

Going forward, every refactor should inspect for:

HTTP response buffers
JSON receive buffers
image decode scratch buffers
thumbnail work buffers
temporary parser buffers
any local arrays larger than approximately 1 KB

Preferred allocation order:

PSRAM
Internal heap
Task stack (small objects only)

Avoid placing large temporary buffers on task stacks unless absolutely required.

This guideline will be applied throughout the remainder of the project.

Stage 2 Progress
Completed
Stage 2A
Moonraker state synchronization
Stage 2B.1
dash_print_state_text()
Stage 2B.2
ui_dashboard_v32_push_live_banner_data()
Stage 2B.3
ui_dashboard_v32_push_live_machine_data()
Dashboard portion of update_live_cards()
Validation

Verified after OTA:

Dashboard
Active print banner
Temperatures
Progress
ETA
Thumbnail updates
Files page
Files refresh
Printer page
OTA validity
No rollback
No LVGL stack faults
Refactoring methodology update

Today's work confirmed that the safest and fastest long-term approach is:

Leaf/helper functions
        ↓
Build
        ↓
OTA
        ↓
Dashboard regression
        ↓
Files regression
        ↓
Next helper

Rather than migrating large shared functions in a single pass.

Small validated batches have proven substantially more reliable while still allowing meaningful progress each session.

Remaining Stage 2 work
Stage 2C
Printer page read-only migration
Stage 2D
Drybox read-only migration
Stage 2E
Remove remaining legacy state globals
Final cleanup
Freeze

---

## 2026-07-12 11:02:55 — Moonraker HTTP Transport Layer Complete

### Completed

Moved the final direct Moonraker HTTP operation out of `main.c`.

The G-code POST request for:

```text
/printer/gcode/script
```

is now owned by `moonraker.c` through:

```c
moonraker_send_gcode_script()
```

### main.c responsibility

`main.c` now retains only the thin application-level wrapper:

- validates WiFi and command input
- calls `moonraker_send_gcode_script()`
- updates the user-facing Moonraker status text
- contains no ESP HTTP client setup, perform, or cleanup calls

### Transport ownership now centralized

`moonraker.c` now owns Moonraker HTTP transport for:

- connection testing
- live-object polling transport
- file-list fetches
- file-metadata fetches
- thumbnail downloads
- print-start requests
- G-code script POST requests

### Validation

Verified:

- source validation passed
- no `esp_http_client_init()` remains in `main.c`
- no `esp_http_client_perform()` remains in `main.c`
- no `esp_http_client_cleanup()` remains in `main.c`
- build successful
- OTA successful
- runtime successful

### Current line counts

```text
main/main.c       3949 lines
main/moonraker.c  689 lines
main/moonraker.h  134 lines
```

### Architectural milestone

The application now follows this ownership boundary:

```text
UI
    -> application coordination
        -> Moonraker transport
            -> ESP HTTP client
```

`main.c` no longer directly owns Moonraker HTTP transport.

### Next stage

Stage 2C — Printer page read-only migration.

Continue using small leaf/helper migrations:

1. identify one printer display-update helper
2. move formatting and widget updates into the printer UI module
3. keep command/action behavior unchanged
4. build
5. OTA
6. verify Dashboard, Files, and Printer pages

---

## 2026-07-12 14:49:37 — Stage 2C.3 — Printer Banner Read-Only Ownership

### Stage 2C.1 — Live-status ownership

Moved active-print file visibility and active-job detection from
`main.c` into `ui_printer_live_status.c`.

The live-status module now owns:

- active-file label text
- active-file card visibility
- active-job state evaluation
- speed display
- flow display
- layer display

Verified:

- build successful
- OTA successful
- Printer page verified
- Dashboard verified
- Files page verified

### Stage 2C.2 — Printer info-card timing ownership

Moved printer info-card remaining-time formatting from `main.c`
into `ui_printer_info_cards.c`.

Added:

```c
ui_printer_info_cards_refresh_live()
```

The module now creates its own small remaining-time buffer and calls
the existing info-card refresh implementation.

A build dependency was discovered because the top-bar ETA still needed
a remaining-time string. That dependency was preserved with a separate
small local buffer inside the top-bar ETA block.

Verified:

- build successful after dependency correction
- OTA successful
- runtime verified
- info-card behavior unchanged

### Stage 2C.3 — Printer banner read-only ownership

Moved printer state-label text and state color from `main.c` into
`ui_printer_banner.c`.

The banner module now owns:

- printer banner text
- printer state text
- printer state color

A scripted identifier replacement initially corrupted
`lv_label_set_text()` into `lv_label_set_banner_text()`.
The replacement was repaired and validation was added to reject the
corrupted identifiers.

Verified:

- build successful
- OTA successful
- runtime verified
- no UI regressions

### Current printer-page ownership

```text
ui_printer_banner
    banner text
    printer state text
    printer state color

ui_printer_live_status
    active-file label
    active-file visibility
    active-job evaluation
    speed
    flow
    layer

ui_printer_info_cards
    progress
    nozzle
    bed
    part fan
    elapsed
    remaining
    ETA
    remaining-time formatting
```

### Current line counts

```text
main/main.c                       3938 lines
main/ui_printer_banner.c          49 lines
main/ui_printer_live_status.c     138 lines
main/ui_printer_info_cards.c      245 lines
```

### Refactoring direction

The printer section of `ui_refresh_timer_cb()` is now primarily an
orchestrator calling focused UI modules.

Do not create a large cross-module printer refresh wrapper unless a
clear ownership boundary emerges.

The next phase should prioritize larger self-contained ownership
islands rather than additional very small cross-module edits.

Candidate next areas:

- thumbnail coordination
- remaining legacy page construction
- dashboard/status popup helpers
- larger standalone event-handler groups
- remaining legacy state globals

### Validation state

Known-good checkpoint:

- build verified
- OTA verified
- printer page verified
- dashboard verified
- files page verified
- Moonraker transport centralized
- Stage 2C.1 through Stage 2C.3 verified


---

## Stage 3 Thumbnail Architecture Complete

Verified:
- Download worker moved into thumbnail_manager_v32.
- Thumbnail result-state ownership moved into thumbnail_manager_v32.
- Shared thumbnail renderer introduced as thumbnail_render_v32.
- Printer-page rendering migrated to shared renderer.
- Dashboard render worker migrated to shared renderer.
- Dashboard synchronous renderer migrated to shared renderer.
- Duplicate RGB565 conversion and pixel helpers removed from main.c.
- LVGL decoder ownership removed from main.c.

Validation:
- Multiple successful builds.
- Multiple successful OTA updates.
- Dashboard live thumbnail verified.
- Printer page thumbnail verified.
- Files popup thumbnail verified.

Architecture:

thumbnail_manager_v32
    download
    cache
    task
    state

thumbnail_render_v32
    decode
    RGB565 conversion
    aspect-fit scaling
    rendering

main.c
    routing
    timers
    orchestration

Notes:
- Thumbnail rendering now exists in exactly one implementation.
- PSRAM-first allocation policy retained.
- Refactor completed without functional regressions.


---

## Stage 4 Dashboard Live-Preview Coordination Complete

Stage 4 reorganized the dashboard live-card and live-thumbnail
coordination path without changing operator-visible behavior.

### Stage 4A — Split `update_live_cards()`

The former monolithic `update_live_cards()` function was divided into
focused helpers:

```text
update_dashboard_status_cards()
    dashboard banner
    nozzle temperature
    bed temperature
    print progress and timing
    active print filename
    dashboard state and banner color

update_dashboard_live_preview()
    live-print preview coordination entry point

update_dashboard_print_complete_cleanup()
    printing/paused to ready/connected cleanup
    dashboard thumbnail placeholder restoration
    selected thumbnail cleanup

update_environment_cards()
    chamber temperature
    humidity
    heater target and state
    drybox fan
    Moonraker live-data link state

update_live_cards()
    thin sequencing wrapper
```

This was a structural split only. No ownership moved during Stage 4A.

Validation:

- build verified
- OTA verified
- dashboard live values verified
- environmental cards verified
- live thumbnail verified
- print-complete cleanup verified

### Stage 4B — Extract live-preview coordinator

The live-preview decision and invalidation logic moved out of
`main.c`.

Initial extraction name:

```text
dashboard_live_preview.c
dashboard_live_preview.h
```

The module was then renamed to reflect its broader responsibility:

```text
thumbnail_preview_coordinator_v32.c
thumbnail_preview_coordinator_v32.h
```

The coordinator now owns:

```text
live-print preview identity comparison
host:port plus filename preview-key generation
live-preview change detection
thumbnail force-refresh decisions
dashboard render-object invalidation
printer-page render-object invalidation
metadata refresh triggering
delayed thumbnail-load triggering
dashboard/printer preview synchronization
```

The coordinator deliberately remains separate from:

```text
thumbnail_manager_v32
    download
    cache
    worker task
    thumbnail data and result state

thumbnail_render_v32
    PNG decode
    RGB565 conversion
    aspect-fit scaling
    rendering
```

This prevents network/cache ownership, rendering ownership, and
presentation coordination from collapsing into one large module.

Validation:

- build verified
- OTA verified
- dashboard live thumbnail verified
- printer-page thumbnail verified
- file-confirm preview verified
- print completion and cancel cleanup verified
- host/port change refresh verified

### Stage 4C.1 — Move live-preview identity state

The live-preview identity buffer moved from `main.c`:

```c
dash_live_preview_key
```

to private coordinator state:

```c
static char s_live_preview_key[240];
```

The coordinator now owns both the live-preview comparison algorithm and
the state used by that algorithm.

A public invalidation API was added:

```c
thumbnail_preview_coordinator_v32_reset();
```

Existing invalidation paths now call that API:

```text
Moonraker host/port change
printer-page live-file synchronization
```

The key buffer is no longer exposed through the public coordinator
context.

Validation:

- build verified
- OTA verified
- live preview verified
- host-change invalidation verified
- printer-page synchronization verified

### Current thumbnail architecture

```text
thumbnail_manager_v32
    download
    cache
    task
    thumbnail data and result state

thumbnail_render_v32
    decode
    RGB565 conversion
    aspect-fit scaling
    rendering

thumbnail_preview_coordinator_v32
    live-preview identity
    live-preview routing
    dashboard/printer synchronization
    preview invalidation
    presentation lifecycle coordination

main.c
    application-level routing
    timer sequencing
    remaining shared thumbnail bridges
```

### State deliberately left in `main.c`

The following buffers were not moved during Stage 4:

```text
selected_print_file
selected_thumbnail_path
```

They are still shared by multiple workflows, including file selection,
printer-page preview, metadata generation, thumbnail requests, and
file-confirm preview handling.

They must not be moved into the live-preview coordinator without a
complete usage and ownership audit.

### Current line counts

```text
main/main.c                                      3842 lines
main/thumbnail_manager_v32.c                    659 lines
main/thumbnail_render_v32.c                     180 lines
main/thumbnail_preview_coordinator_v32.c         148 lines
main/thumbnail_preview_coordinator_v32.h          46 lines
```

### Stage 4 result

Stage 4 is complete and verified.

The dashboard live-preview logic is no longer implemented in
`main.c`, and the coordinator now privately owns its preview identity
state.

The next stage should be selected from a fresh ownership audit rather
than continuing to move shared thumbnail buffers without evidence.


---

## Printer Page Preview Ownership Moved to ui_printer_v32

The printer-page thumbnail preview implementation moved out of
`main.c` and into `ui_printer_v32`.

### Ownership moved

```text
printer preview box
printer preview placeholder label
printer preview image object
printer preview canvas object
printer preview RGB565 canvas buffer
printer preview rendered-file identity
printer preview widget creation
printer preview rendering
printer preview reset
printer preview reference cleanup
```

### New ui_printer_v32 preview API

```c
ui_printer_v32_preview_create()
ui_printer_v32_preview_show()
ui_printer_v32_preview_reset()
ui_printer_v32_preview_destroy_refs()

ui_printer_v32_preview_canvas_file()
ui_printer_v32_preview_canvas_file_size()

ui_printer_v32_preview_canvas_ref()
ui_printer_v32_preview_image_ref()
```

### Architecture

```text
ui_printer_v32
    printer-page preview widget ownership
    placeholder ownership
    canvas/image ownership
    PSRAM-first RGB565 buffer ownership
    preview rendering
    rendered-file identity

thumbnail_render_v32
    shared PNG decode and RGB565 conversion

thumbnail_manager_v32
    PNG data and download state

thumbnail_session_v32
    selected file
    selected thumbnail path
    metadata and persistence

main.c
    supplies current printer state
    requests preview refresh
    coordinates cross-page invalidation
```

### Validation

- build verified
- OTA verified
- printer page verified
- printer preview verified
- dashboard preview verified
- shared thumbnail rendering retained
- PSRAM-first preview buffer allocation retained

### Current line counts

```text
main/main.c                         3595 lines
main/ui_printer_v32.c               279 lines
main/ui_printer_v32.h               27 lines
main/thumbnail_session_v32.c        305 lines
```

### Next stage

Finish printer-page ownership by moving the remaining page construction
and destruction code from `main.c` into `ui_printer_v32`.

The next change must preserve:

```text
application-owned printer state
Moonraker and command callbacks
printer controller state decisions
existing action-button callbacks
existing popup callbacks
page show/hide behavior
```


---

## Printer Behavior and UI Controller Extraction Complete

Printer behavior and action routing moved out of `main.c` into:

```text
printer_ui_controller.c
printer_ui_controller.h
```

### Controller ownership

The new controller owns:

```text
printer command-button event routing
cancel-action routing
motion-action routing
printer action-button enable/disable state
controller callback registration
```

### Public controller API

```c
printer_ui_controller_init()

printer_ui_controller_command_event_cb()
printer_ui_controller_motion_event_cb()

printer_ui_controller_update_action_buttons()
```

### Application bridges retained

One cancel bridge remains in `main.c`:

```c
printer_ui_controller_show_cancel_bridge()
```

This bridge remains necessary because the cancel popup requires a
G-code callback argument.

The redundant G-code and motion bridges were removed.

The controller now receives these functions directly:

```c
moonraker_send_gcode
show_motion_popup
```

### Current printer architecture

```text
printer_controller
    state interpretation
    action availability decisions
    status formatting

printer_ui_controller
    user-action routing
    cancel routing
    motion routing
    action-button updates

ui_printer_v32
    printer page
    preview widget
    preview rendering
    page UI ownership

ui_printer_actions
    action widgets

ui_printer_banner
    banner UI

ui_printer_info_cards
    printer information cards

ui_printer_live_status
    active-file and live-value UI

ui_printer_motion
    motion popup UI

ui_printer_popups
    temperature, cancel, and status popups

main.c
    application state
    Moonraker communication
    callback adaptation
    refresh sequencing
```

### Validation

- build verified
- OTA verified
- printer page verified
- HOME action verified
- PAUSE action verified
- RESUME action verified
- CANCEL popup verified
- motion popup verified
- normal G-code command routing verified
- action-button state updates verified

### Current line counts

```text
main/main.c                         3594 lines
main/printer_ui_controller.c        77 lines
main/printer_ui_controller.h        25 lines
main/ui_printer_v32.c               279 lines
main/ui_printer_v32.h               27 lines
```

### Milestone result

Printer widgets, preview rendering, state decisions, and UI behavior now
have distinct ownership boundaries.

The printer subsystem no longer has a large cohesive ownership island
inside `main.c`.

The next step is a final architectural audit. Further extraction should
only occur where a clear ownership violation remains.


---

# Final Architecture Audit – Monolith Refactor Complete

## Summary

A complete architectural audit of `main.c` was performed after the
printer controller extraction.

Current line counts:

```text
main/main.c                         3594
moonraker.c                          689
thumbnail_manager_v32.c              659
ui_network_tools.c                   579
ui_printer_v32.c                     279
printer_controller.c                 225
printer_ui_controller.c               77
```

## Final ownership review

### Printer

Completed.

Ownership is now divided into:

```text
printer_controller
    printer state interpretation

printer_ui_controller
    command routing
    motion routing
    cancel routing
    action button state

ui_printer_v32
    page ownership
    preview ownership

ui_printer_actions
ui_printer_banner
ui_printer_info_cards
ui_printer_live_status
ui_printer_motion
ui_printer_popups
```

No additional printer extraction is recommended.

---

### Dashboard

Completed.

Ownership now resides in:

```text
thumbnail_manager_v32
thumbnail_render_v32
thumbnail_session_v32
thumbnail_preview_coordinator_v32
ui_dashboard_v32
```

Remaining dashboard code inside main.c is application orchestration.

No additional extraction recommended.

---

### Network

Completed.

Ownership now resides in:

```text
ui_network_v32
ui_network_tools
network_wifi_scan
```

Remaining code inside main.c is application behavior.

No additional extraction recommended.

---

### Files

Completed.

Remaining app_files_reload() implementation is application logic and
correctly remains in main.c.

---

### Moonraker

Completed.

moonraker_get_live_objects()

moonraker_live_poll_tasklet()

moonraker_send_gcode()

remain application orchestration and should remain in main.c.

---

## Remaining legacy subsystem

The only remaining subsystem still marked LEGACY is the Drybox UI.

Legacy ownership still consists of:

```text
legacy_make_drybox_info()
legacy_make_drybox_button()
legacy_refresh_drybox_tab()
legacy_cleanup_drybox_tab()
legacy_create_drybox_tab()

build_drybox_dashboard()
```

This becomes the next modernization target.

---

## Architectural conclusion

The original refactor objective has been achieved.

main.c is no longer a monolithic UI implementation.

Ownership is now divided into:

- UI modules
- Controller modules
- Manager modules
- Moonraker transport
- Thumbnail pipeline
- Printer subsystem
- Network subsystem
- Files subsystem

main.c now primarily performs:

- application startup
- lifecycle management
- polling
- refresh sequencing
- subsystem coordination

Further extraction should occur only when it improves ownership, not
simply to reduce line count.

The next development stage begins with Drybox modernization rather than
continued decomposition of main.c.


---

## Popup Framework Completion / UI Cards Foundation

### Popup framework completed

Verified reusable popup framework:

- ui_popup_create()
- ui_popup_add_title()
- ui_popup_add_message()
- ui_popup_add_list()
- ui_popup_add_textarea()
- ui_popup_add_keyboard()
- ui_popup_add_button_at()
- ui_popup_add_button_aligned()

Verified consumers:

- Printer dialogs
- WiFi scan
- WiFi password
- Moonraker port editor
- Moonraker connection test

OTA verified after migration.

### UI Cards

Created:

- ui_cards.c
- ui_cards.h

Added:

- ui_info_card_create()

Printer page now builds all seven information cards through the shared
ui_cards module.

Printer-specific responsibilities remain in ui_printer_info_cards.c:

- vivid icons
- title positioning
- click callbacks
- live formatting
- value colors

### Compatibility

Removed:

- make_printer_info_w()
- ui_printer_info_make_w_cb_t

The fixed-size make_printer_info() helper remains temporarily as a thin
compatibility bridge for Network, Settings, and Drybox until those pages
migrate directly to ui_cards.

### Current architecture

ui_theme
→ visual system

ui_popup
→ dialogs and forms

ui_cards
→ reusable information cards

Page modules
→ behavior and business logic

Build verified before freeze.


---

## Telemetry modularization verified

Date: 2026-07-17 23:56:50

The original monolithic telemetry page was separated into four focused
modules:

- `telemetry_history.c/.h`
  - telemetry sampling
  - circular ten-minute history
  - PSRAM-first history allocation

- `ui_telemetry_components.c/.h`
  - shared telemetry labels
  - metric cards
  - chart legend items

- `ui_telemetry_charts.c/.h`
  - independent nozzle, bed, chamber, and humidity charts
  - chart state and series ownership
  - adaptive Y-axis scaling
  - axis hysteresis using consecutive edge samples
  - target reference lines
  - newest-sample markers
  - range statistics and history replay

- `ui_telemetry_v32.c/.h`
  - page lifecycle and composition
  - metric value formatting
  - public Moonraker refresh entry point

Validation:

- telemetry component extraction built successfully
- telemetry chart-engine extraction built successfully
- OTA verification succeeded after extraction
- final ownership cleanup built successfully
- no chart implementation remains in the page owner

Status: verified known-good architectural checkpoint.
