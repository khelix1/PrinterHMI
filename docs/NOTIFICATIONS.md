# Notification policy

A toast is optional confirmation of a completed, noncritical action. Missing it must not change what the operator needs to do. It must never be the only evidence that a command failed, that the printer needs attention, or that a running operation completed.

| Situation | Presentation | Examples |
| --- | --- | --- |
| Small action completed and its result is otherwise easy to miss | Optional short toast | Copied, exported, saved a preference |
| Result is already visible | Update the control; no toast | Theme changed, favorite toggled, filter applied, target edited |
| Input is invalid or an action is unavailable | Inline feedback near the field/action; retain input | Empty command, invalid macro parameter, home axes first |
| Transport rejects a command | Persistent inline result or page notice, with retry context | Print not started, command not sent, calibration rejected |
| Device is offline or operation is ongoing | Persistent page status | Printer offline, reconnecting, PID running, OTA progress |
| Operator decision is required | Explicit dialog before acting | Start print, save configuration/restart, destructive action |
| Safety or recovery needs attention | Persistent prominent fault/recovery dialog | Runout, stop not confirmed, restart failed, heater fault |

## Completion language

Sending a request and completing its effect are distinct events. WebSocket dispatch or HTTP acceptance may say **Request sent**; it cannot establish **Calibration complete**, **Printer stopped**, **Temperature reached**, or **Print started**. Those states require printer telemetry. Console keeps the command attempt and reported response/error in its bounded session history; it does not add a success toast for every command.

## Appliance-wide preference and current implementation

Settings → Display → Notifications controls **optional confirmations ON/OFF** for every printer, page and theme. OFF is the default. The value is persisted in NVS and included in configuration backups. Backup validation now accepts all five built-in themes, including Studio; its older upper bound stopped at Glass. Older backups without the field restore the quiet default; a malformed value is rejected.

The shared notification component enforces this policy for existing callers. Successful/neutral confirmations appear briefly only when enabled. Warnings, errors and essential information appear in a scrollable, persistent notice with a compact Dismiss action. They have no dismissal timer and cannot be disabled by the preference. Dismiss acknowledges the message; it does not clear a printer fault. Each notice captures the selected printer name at creation, so changing printers cannot relabel an old failure.

Pending notices are bounded to four, exact duplicates are coalesced, danger notices take priority, and overflow important notices are recorded in the existing bounded Event History ring. Optional success confirmations never displace an important notice. A queue-overflow message directs the operator to Event History. Event History is bounded to its existing 40 entries and its existing message length; this is not a durable fault archive.

Console owns empty-input, offline/stale-printer and send-failure feedback inside its editor. Rejected input remains editable. Its HTTP fallback suppresses the generic transport notice to avoid duplicate feedback. Calibration save, gantry and abort acknowledgements now stay in the workflow and describe dispatched requests. Stale-printer errors remain visible beside their blocked controls.

## Workflow migration status

Macro validation/send errors, calibration prerequisites and request results, print confirmation failures, OTA startup failures and emergency-stop/restart outcomes now use owned local feedback. General transport failures retain persistent notices with explicit Open Console navigation. Existing live result/state controllers continue to report execution outcomes after dispatch. The appliance-wide preference governs optional transient confirmations; it does not hide local validation or faults.

Physical printing, configuration-save/reconnection and recovery tests remain pending.

## Validation

`notifications_test.py` uses real LVGL to check global preference behavior, NVS persistence/failure, notice persistence, profile identity, priority, deduplication, bounded queues and Event History forwarding, layout/scroll/footer bounds and lifecycle in all five themes, three densities, normal/large text and wide/narrow/short displays. The production notification unit compiles separately with warnings as errors. `notification_backup_test.py` checks the actual preference parser against real libcJSON, including older-backup defaults and malformed values. Firmware/panel tests remain pending.

## Feedback at the action and explicit notice navigation

| Action | Feedback location |
| --- | --- |
| Macro validation, stale printer, failed dispatch | Parameter/review dialog; entered values retained for correction or retry |
| Macro sent/favorite change | Macro page status and favorite marker |
| Calibration readiness, command failure and leveling request | Current calibration dialog; a dismissible calibration status dialog when no workflow is open |
| Configuration save | Save dialog stays open with request/reconnect instructions; dispatch failure retains results and retry controls |
| Print readiness, printer change, HTTP start failure | Print confirmation remains open with inline explanation and Cancel |
| Emergency stop/restart | Recovery dialog displays dispatch outcome; M112 dispatch still happens immediately before rendering |
| Empty custom update URL | Editor remains open with a URL-required prompt |
| Update startup failure | Dismissible Update Not Started dialog; running update progress remains owned by the updater |
| Generic command failure | Persistent notice with explicit Open Console and Dismiss |

Tapping the notification body does not navigate. Open Console is explicit, never sends a printer command, and only opens when the captured configuration generation still owns the active printer. A stale notice remains visible with an explanation instead of navigating into another printer's context. Dismiss only acknowledges the notice. Optional confirmations stay non-interactive and follow the appliance-wide preference.

Local feedback is owned by its dialog and does not create a second toast. Existing calibration result/session updates, OTA download progress, live recovery state and command resolution remain responsible for actual execution outcomes; accepted dispatch is described as a request, not completed motion or a verified restart.

Host validation covers all five themes, densities, large text and narrow viewports; macro field retention/retry, stale command rejection, immediate M112 failure, restart feedback, nested calibration deletion, SAVE_CONFIG controls, print confirmation failures, OTA startup/active-update cases, notification navigation guards and actual WS/HTTP routing. Firmware build, panel and physical printing/recovery tests remain pending.
