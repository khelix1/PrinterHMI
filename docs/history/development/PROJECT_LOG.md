
## Printer Live Status extraction

- Froze main.c before extraction:
  - main/main.c.freeze_before_printer_live_status_extract
- Added:
  - main/ui_printer_live_status.c
  - main/ui_printer_live_status.h
- Moved the Printer page LIVE TUNING card creation out of main.c.
- Moved live refresh logic for:
  - active file label
  - live velocity
  - live flow
  - layer estimate/current layer
- main.c now delegates this card to ui_printer_live_status_create() and ui_printer_live_status_refresh().
- Kept old label variable names for safety:
  - printer_fan_label currently displays live speed
  - printer_speed_label currently displays live flow
  - printer_flow_label currently displays layer
- Build passed.
- OTA update passed.
- Runtime behavior verified after OTA.

Next planned extraction:
- Printer thumbnail / preview panel.

## Printer thumbnail extraction deferred

- Reviewed printer thumbnail/preview references before extraction.
- Decided not to extract printer thumbnail yet because it is shared across:
  - printer page preview
  - dashboard preview
  - file popup preview
  - thumbnail cache/render/decode path
  - live vs selected preview policy
- Safer next extraction selected:
  - Printer info cards: nozzle, bed, progress, elapsed, remaining, ETA.

## Session Summary - Printer Page Modularization continued

Completed:
- Added ui_printer_live_status.c/.h.
- Moved LIVE TUNING card creation out of main.c.
- Moved live status refresh logic:
  - active file
  - live speed
  - live flow
  - current/estimated layer
- Build passed.
- OTA update passed.
- Runtime verified.

Completed:
- Added ui_printer_info_cards.c/.h.
- Moved Printer info card creation:
  - Progress
  - Nozzle
  - Bed
  - Part Fan
  - Elapsed
  - Remaining
  - ETA / Moonraker status
- Moved refresh logic into ui_printer_info_cards_refresh().
- Left make_printer_info* helpers in main.c because they are shared by Printer, Network, and Settings.
- Build passed.
- OTA update passed.
- Runtime verified.

Architecture decision:
- Deferred thumbnail extraction.
- Thumbnail/preview is treated as a shared subsystem because it touches:
  - Dashboard preview
  - Printer preview
  - File popup preview
  - thumbnail cache
  - PNG/canvas render path
  - live vs selected preview policy

Current extracted Printer modules:
- ui_printer_motion
- ui_printer_live_status
- ui_printer_info_cards

Next planned extraction:
- ui_printer_actions

## Printer Action Buttons extraction

- Froze main.c before extraction:
  - main/main.c.freeze_before_printer_actions_extract
- Added:
  - main/ui_printer_actions.c
  - main/ui_printer_actions.h
- Extracted Printer page action button creation:
  - MOTION
  - HOME
  - PAUSE
  - RESUME
  - CANCEL
- main.c still owns command behavior through callbacks:
  - printer_cmd_event_cb
  - motion_popup_event_cb
- main.c still owns button enable/disable policy.
- Build passed.
- OTA update passed.
- Runtime verified.

## Printer Banner extraction started

- Next extraction selected:
  - ui_printer_banner
- Goal:
  - Move Printer page banner UI creation and refresh out of main.c.
- Constraint:
  - Keep printer state ownership in main.c unless the banner text/color logic is isolated enough to move safely.

## Printer Controller extraction started

- Phase 2 started: behavior/controller extraction.
- First controller responsibility selected:
  - Printer action button enable/disable policy.
- Goal:
  - Move state-based action button policy out of main.c without changing UI or command behavior.

## Printer Controller action button policy extraction

- Added:
  - main/printer_controller.c
  - main/printer_controller.h
- Moved action button enable/disable policy into printer_controller_update_action_buttons().
- Moved low-level set_btn_enabled() helper into printer_controller.c.
- main.c now delegates printer action button guard updates to printer_controller.
- main.c still owns:
  - printer_state source data
  - refresh timing
  - button object handles
  - command callbacks
- Build passed.
- OTA passed.
- Runtime verified.

## Printer Controller time formatter extraction

- Froze main.c before moving shared time formatter:
  - main/main.c.freeze_before_time_formatter_move
- Moved format_hhmm() responsibility into printer_controller:
  - printer_controller_format_hhmm()
- Updated main.c call sites to use printer_controller_format_hhmm().
- Build passed.
- OTA passed.
- Runtime verified.

## Printer Controller state policy extraction started

- Next controller responsibility:
  - centralize the meaning of printer states.
- Goal:
  - move repeated printing/paused/ready/error/active-job decisions into printer_controller.
- Constraint:
  - replace only safe, obvious state checks first.

## Printer Controller state policy extraction

- Added printer state policy helpers:
  - printer_controller_state_is()
  - printer_controller_is_printing()
  - printer_controller_is_paused()
  - printer_controller_is_ready()
  - printer_controller_is_error()
  - printer_controller_has_active_job()
- Replaced active job policy in main.c with printer_controller_has_active_job().
- Replaced raw printer_is_live strcmp checks with controller state helpers.
- Build passed.
- OTA passed.
- Runtime verified.

## Remaining Printer state checks routed through controller

- Froze main.c before routing remaining state checks:
  - main/main.c.freeze_before_remaining_state_checks_routed
- Replaced remaining direct printer_state_is() checks in main.c with printer_controller state helpers.
- main.c no longer directly checks printer printing/paused/error state through printer_state_is().
- Printer state meaning now lives in printer_controller.
- Build passed.
- OTA passed.
- Runtime verified.

## ETA clock formatting moved into printer_controller

- Froze main.c before ETA clock policy move:
  - main/main.c.freeze_before_eta_clock_policy_move
- Added:
  - printer_controller_format_eta_clock()
- Moved wall-clock ETA formatting policy out of main.c.
- Preserved SNTP/year validity behavior using the safer existing check.
- main.c no longer contains raw remaining-time ETA math.
- Build passed.
- OTA passed.
- Runtime verified.

## Live printer state helper added

- Froze main.c before live-state helper:
  - main/main.c.freeze_before_live_state_policy_helper
- Added:
  - printer_controller_is_live_state()
- Replaced repeated printing-or-paused expressions in main.c.
- Build passed.
- OTA passed.
- Runtime verified.

## Printer status symbol policy moved into controller

- Froze main.c before status symbol policy move:
  - main/main.c.freeze_before_status_symbol_text_policy
- Added:
  - printer_controller_format_status_symbol_text()
- Moved PRINTING / PAUSED / ERROR / READY / OFFLINE symbol-text decision out of main.c.
- Removed remaining direct printer_controller_is_printing/is_paused/is_error status formatting block from main.c.
- Build passed.
- OTA passed.
- Runtime verified.

## Dead printer_state_is helper removed

- Froze main.c before removal:
  - main/main.c.freeze_before_dead_printer_state_is_removed
- Removed unused printer_state_is() helper from main.c.
- Printer state interpretation is now routed through printer_controller.
- Build passed.
- OTA passed.
- Runtime verified.

## Printer UI completion pass started

- Goal:
  - Finish the remaining Printer-tab UI boundary where safe.
- Constraint:
  - Do not move shared thumbnail/cache/file infrastructure tonight.
- First target:
  - Printer popup controllers.
