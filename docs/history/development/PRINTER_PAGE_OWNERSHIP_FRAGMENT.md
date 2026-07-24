
## Printer Page Ownership

main.c owns:
- printer state
- Moonraker data
- navigation orchestration
- shared helper functions still used across multiple pages
- popup/controller bridges for now

ui_printer_motion owns:
- motion popup UI
- jog controls
- extrusion controls
- motion step controls

ui_printer_live_status owns:
- live status card
- active file label
- live velocity
- live flow
- current/estimated layer display

ui_printer_info_cards owns:
- progress card
- nozzle card
- bed card
- part fan card
- elapsed card
- remaining card
- ETA/Moonraker hidden status card

Deferred:
- thumbnail/preview extraction is postponed until it can be treated as a shared preview subsystem.

Future modules:
- ui_printer_actions
- ui_printer_preview
- ui_printer_popups
