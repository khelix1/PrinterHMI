# PrinterHMI v3.2 Refactor Log
## Date
2026-07-05 (Part 2)

---

# Goal

Continue reducing main.c while preserving OTA stability after every change.

---

# Verified

## Files UI

- Moved Files page into ui_files_v32.c
- Files page creates/destroys correctly
- Refresh button works
- Files populate automatically
- File selection callback works
- Detail popup now owned by ui_files_v32
- Thumbnail placeholder still functions
- Start/Cancel callbacks bridged back into main.c

Verified by OTA.

---

## Moonraker service extraction

Added:

- moonraker_fetch_file_list()
- moonraker_fetch_file_metadata()

Both now live in moonraker.c.

main.c no longer contains:

- /server/files/list
- /server/files/metadata

Those HTTP endpoints are now owned by the Moonraker service.

Verified by OTA.

---

## Architecture improvements

Current ownership:

main.c
- application flow
- high level orchestration
- thumbnail manager
- metadata formatting
- printer workflow

ui_files_v32.c
- Files page
- refresh button
- file list widgets
- detail popup
- callback bridge

moonraker.c
- HTTP transport
- JSON helpers
- Moonraker state
- file list API
- metadata API

---

## Current size

main.c

5862 lines

Down from roughly 7000 lines.

---

## Stability

Every extraction today:

- compiled
- OTA updated
- booted
- verified on hardware

No rollback required.

---

## Next target

Extract:

moonraker_start_print_file()

into

moonraker.c

This completes another Moonraker API endpoint and further separates UI from transport.

