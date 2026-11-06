# Changelog

## [7.3.52-dev] - 2026-11-05

- Publish Setup Wi-Fi scan records and completion with one atomic result handoff, preventing a previous worker from clearing the next scan’s busy state.
- Distinguish scan/start/result-read failures from a successful scan with no networks; preserve retries and close/reopen behavior.

## [7.3.51-dev] - 2026-11-05

- Forward captured printer API keys through Files-row, fullscreen, selected-file and inactive-printer thumbnail downloads.
- Capture/scrub selected-file download credentials and reject launch if the active endpoint or generation changes during preparation.
- Clear quiet-download output pointers/lengths before network ownership acquisition fails; preserve explicit anonymous compatibility APIs.

## [7.3.50-dev] - 2026-11-05

- Use each inactive printer’s captured API key for its status/metadata requests instead of retaining the active startup key.
- Discard background results after inactive endpoint, key, transport or configured-state changes, without forcing an active-printer reconnect.
- Hide health cached for a different endpoint, reconcile inactive edits on each polling turn, and avoid carrying old-endpoint retry delays into unknown health.

## [7.3.49-dev] - 2026-11-05

- Treat unchanged profile connection fields and primary camera URLs as successful no-op saves without flash writes or generation changes.
- Rebind the active connection after profile editing only when configuration generation changes, including accepted partial saves.

## [7.3.48-dev] - 2026-11-05

- Reject file selections that exceed the thumbnail session path capacity before changing selected file, cached selection or preview state.
- Report the path-length failure on Files and safely support selecting the existing session buffer.

## [7.3.47-dev] - 2026-11-05

- Retire deleted print-time preference controls and disable Save when required fields are missing.
- Reject duplicate/disabled save actions and fence storage completion to its original dialog, preserving replacements and retry selections.

## [7.3.46-dev] - 2026-11-05

- Timestamp authoritative/demo printer-health snapshots and clear freshness on explicit offline or unknown results.
- Debounce the active-to-inactive notification baseline so brief printer-selection gaps do not fabricate Reconnected notices; retain confirmed loss/recovery behavior.

## [7.3.45-dev] - 2026-11-05

- Keep inactive-printer health checks running when the active printer is offline; defer thumbnail downloads during reconnection.
- Show Verifying in the chooser once inactive live state expires instead of retaining an old Printing/Paused label.

## [7.3.44-dev] - 2026-11-05

- Keep manual Moonraker verification and LAN discovery restartable after fast worker completion or task-creation failure.
- Reject oversized endpoint hosts/API keys before copying them; clear temporary verification API keys after completion or failed startup.
- Publish discovery running/cancellation state atomically and update older worker fixtures for task-handle-free startup.

## [7.3.43-dev] - 2026-11-05

- Keep the live camera stream restartable after early worker exit, queue allocation failure, or task-creation failure.
- Reject oversized camera stream URLs and reject failed headers/non-200 HTTP responses before reading video data.
- Release the shared transport if a camera stop arrives while acquiring it.

## [7.3.42-dev] - 2026-11-05

- Keep camera tests and discovery restartable when their workers finish before task creation returns; protect results while consumers copy them.
- Clear discovery API keys after probing and turn binary lighting outputs fully on regardless of remembered PWM brightness.

## [7.3.41-dev] - 2026-11-05

- Clear stale Wi-Fi scan rows on rescan and keep result status across Network refreshes.
- Make unnamed scan results non-selectable and keep inline rows out of popup selection state.

## [7.3.40-dev] - 2026-11-04

- Show “ETA paused” on Dashboard instead of a finish time that assumes immediate resume.

## [7.3.39-dev] - 2026-11-04

- Mark Dashboard ETAs after local midnight with a day offset, including across daylight-saving changes.

## [7.3.38-dev] - 2026-11-04

- Theme the Print progress & time fields and expanded option lists using the shared selector styles.
- Give the Cancel Object confirmation button room for its label at larger text sizes.

## [7.3.37-dev] - 2026-11-04

- Load active-job estimate metadata independently of thumbnail previews and retry temporarily unavailable metadata in the background.

## [7.3.36-dev] - 2026-11-04

- Add four print progress modes and independent time-remaining and ETA source selections in Settings.
- Read file position, slicer progress, and filament usage through WebSocket and HTTP fallback. Match file metadata to the current job before using it.

## [7.3.35-dev] - 2026-11-04

- Keep Dashboard and Printer progress consistent and hide stale Dashboard print details when the connection is offline.
- Show placeholders for invalid elapsed and remaining times instead of converting them to integers.

## [7.3.34-dev] - 2026-11-04

- Keep demo commands from changing simulated hardware while offline or after shutdown.
- Reject malformed demo heater, fan, motion, offset, and filament sensor commands.

## [7.3.33-dev] - 2026-11-04

- Honor OTA cancellation up to the start of firmware validation and keep overlapping starts out of the progress dialog.
- Reject short reads and trailing data if a plain or encrypted backup changes size during restore.

## [7.3.32-dev] - 2026-11-04

- Reject oversized Console commands without changing or sending the input, and preserve complete commands in history.

## [7.3.31-dev] - 2026-11-04

- Discard thumbnail downloads invalidated by printer or preview changes before the display adopts the image.
- Give Files detail priority over background live thumbnail refresh while the dialog is open.

## [7.3.30-dev] - 2026-11-04

- Key SD thumbnail PNGs by printer endpoint and both paths to keep unrelated previews separate.
- Reject oversized thumbnail requests before allocating a transfer buffer, and use the shared encoder for live metadata.

## [7.3.29-dev] - 2026-11-04

- Stop retired Files row and fullscreen preview jobs before downloading the image if metadata returns after a printer switch or view change.
- Preserve Files preview slots and cache revisions when a queue submission needs a retry.

## [7.3.28-dev] - 2026-11-04

- Skip invalid or oversized thumbnail paths before selecting a preview. Files rows still prefer smaller suitable images; other previews use the largest usable original.
- Share thumbnail selection and URL encoding across preview consumers. Reject a path that cannot be encoded in full.

## [7.3.27-dev] - 2026-11-04

- Discard Lighting slider gestures interrupted by reconnects or canceled touches, and remember brightness when reopening Lighting on the same printer.
- Clear pending Lighting controls when the printer no longer reports the light.
- Require matching live Speed/Flow readings to confirm a change and discard gestures interrupted by an observed disconnect.

## [7.3.26-dev] - 2026-11-04

- Retire Toolhead controls and hotend activation confirmations when the observed print job changes.
- Reject invalid or truncated jog, extrusion, and runtime Z offset commands.

## [7.3.25-dev] - 2026-11-03

- Drop speed and flow gestures and pending values when the observed print job changes.
- Require a fresh temperature or fan dialog after a job change, and bound displayed inputs before integer conversion.

## [7.3.24-dev] - 2026-11-03

- Share Pause dispatch and duplicate suppression between Dashboard and Printer, scoped to the observed print job.
- Require a fresh Filament Recovery dialog after a job or printer change.
- Advance the demo job revision on simulated job changes so those guards also work in demo mode.

## [7.3.23-dev] - 2026-11-03

- Suppress duplicate Start Print requests while an accepted request is waiting for live status.
- Require a fresh print review after the observed job changes.

## [7.3.22-dev] - 2026-11-03

- Start the exact G-code path shown in print review instead of reading the shared preview selection at dispatch time.

## [7.3.21-dev] - 2026-11-03

- Bind Stop Print and Cancel Object confirmations and duplicate-send guards to the observed print job.

## [7.3.20-dev] - 2026-11-03

- Use Cancel Object consistently in the object list, confirmation, and command feedback.

## [7.3.19-dev] - 2026-11-03

- Escape discovered object names when building Moonraker WebSocket subscriptions.
- Reject object exclusion names containing G-code delimiters or quoting characters.

## [7.3.18-dev] - 2026-11-03

- Reject out-of-range live measurements and invalid Drybox program codes before publishing them.
- Validate toolhead and object map coordinates and bound the map projection to the display area.

## [7.3.17-dev] - 2026-11-03

- Decode live HTTP telemetry and inactive printer status from their exact Moonraker objects.
- Validate metadata before using file details, layer estimates, or thumbnail paths.
- Reject truncated downloads and HTTP text with embedded NUL bytes; keep the status-only connection probe independent of response size.

## [7.3.16-dev] - 2026-11-03

- Require complete, bounded WebSocket JSON objects and exact numeric response IDs.
- Keep discovered controls and configuration intact when a discovery reply is malformed or reports an error.
- Parse WebSocket response routing once before handing the result to its owner.

## [7.3.15-dev] - 2026-11-03

- Retry saved CA certificate reads after temporary NVS failures instead of caching the failure until reboot.
- Reject incomplete or malformed stored CA strings before publishing trust material.
- Exercise actual CA storage, import preservation and HTTPS/WSS recovery under storage/allocation faults.

## [7.3.14-dev] - 2026-11-03

- Preserve the full supported macro command through WebSocket and HTTP JSON escaping.
- Reject oversized semantic actions instead of silently dispatching a shortened command.
- Share command capacity between the macro builder, action resolver and transport envelopes.

## [7.3.13-dev] - 2026-11-02

- Use the bounded firmware-identity version parser for OTA stable release tags and retire the duplicate sscanf parser.
- Require bounded, complete JSON release catalogs.
- Reject invalid, fractional or out-of-range asset sizes before integer conversion.

## [7.3.12-dev] - 2026-11-02

- Resolve metadata thumbnail paths against the G-code parent directory before encoding.
- Apply the same folder-aware resolution to rows, details, fullscreen and inactive-printer previews.
- Increase selected-preview encoding capacity for resolved paths.
- Add nested-folder metadata and real row-fetch regressions.

## [7.3.11-dev] - 2026-11-02

- Carry supported filenames through URL encoding, metadata HTTP requests and JSON print-start requests without truncation.
- Strictly parse bounded file lists, skip overlong paths and malformed entries, and report invalid responses locally.
- Remove the obsolete path-scraping fallback and its public interface.
- Bound numeric metadata before integer conversion and ignore non-finite/negative file statistics.

## [7.3.10-dev] - 2026-11-02

- Reject stale live device updates if the catalog is replaced during formatting.
- Ignore non-finite device readings and driver fault flags.
- Reject fractional or non-finite endstop request IDs before conversion.
- Share strict bounded JSON parsing between Console and Lighting.
- Keep malformed Lighting documents from acknowledging pending changes.

## [7.3.9-dev] - 2026-11-02

- Open macro run dialogs only on short taps, so releasing a Favorite long press does not open a dialog.
- Keep Favorite feedback inline after the row list refresh.
- Reject Favorite gestures behind an open macro dialog or from a stale printer page.
- Exercise real LVGL press/hold/release sequences for Favorite add and remove.

## [7.3.8-dev] - 2026-11-02

- Bound Console notification scanning to the supplied frame length and validate complete, bounded JSON.
- Validate stored macro favorites before string comparisons and retry failed loads.
- Keep Favorites unchanged on save failures and show local failure feedback instead of success.
- Replace the ambiguous favorite-toggle API with an explicit desired-state setter.
- Omit overlong macro names instead of turning them into truncated commands.
- Keep catalog generations nonzero on wrap and remove duplicate legacy favorite entries together.

## [7.3.7-dev] - 2026-11-03

- Reject camera names and URLs that cannot round-trip through catalog fields before saving.
- Clear camera lookup outputs on invalid input and distinguish missing slots from URL read errors.
- Strictly validate SD preview metadata, including ports, line lengths and trailing data.
- Reject ambiguous metadata filenames before preview persistence.
- Lock preview-cache reads, retain newer live previews during SD restore and reject stale profile loads.
- Retry transient display-lock failures instead of permanently skipping restoration.

## [7.3.6-dev] - 2026-11-03

- Stop profile persistence immediately on a failed configured-flag write instead of erasing its API key.
- Preserve stored profiles when schema reads fail, schemas are unsupported, or no valid saved profiles load.
- Require TLS for unreadable or malformed saved TLS fields; retain HTTP compatibility for missing legacy fields.
- Preserve camera URLs during basic profile edits and clarify persistence guarantees.

## [7.3.5-dev] - 2026-11-03

- Reject failed Wi-Fi password reads and clear both credential outputs on load failure.
- Preserve maximum-length SSIDs and passwords in all three station configuration paths.
- Share fixed-width station field construction; reject oversized credentials before saving.

## [7.3.4-dev] - 2026-11-03

- Preserve the only recovery backup after interrupted rotation or failed rollback, for plain and encrypted exports.
- Share backup promotion logic within the backup module.
- Report failed settings rollback and partial backup deletion accurately.

## [7.3.3-dev] - 2026-11-02

- Reject undersized JPEG frame headers in camera connection tests.
- Reject oversized camera discovery hosts and API keys rather than silently truncating them.

## [7.3.2-dev] - 2026-11-02

- Preserve supported API keys and long encoded filenames in file-detail loads.
- Reject invalid ports and oversized job fields before launching the worker.

## [7.3.1-dev] - 2026-11-02

- Bind layer metadata to its file and printer generation; remove duplicated metadata storage from main.
- Preserve full URL encoding for long filenames in metadata requests.

## [7.3.0] - 2026-11-01

- Add a tablet offline demo and refresh Theme Maker tablet simulation.
- Move offline demo entry to Settings and improve externally started print layer estimates.

## [7.2.23-dev] - 2026-11-01

- Estimate current layer when an external print progresses but the printer keeps reporting layer zero.

## [7.2.22-dev] - 2026-11-01

- Move the offline demo entry from the printer chooser to Settings.

## [7.2.21-dev] - 2026-11-01

- Update the offline demo introduction to describe the available sample workflows and maintenance limits.

## [7.2.20-dev] - 2026-11-01

- Keep simulated print pause, resume and cancel commands consistent with the current printer state.

## [7.2.19-dev] - 2026-11-01

- Add a continuous sample print journey check in both themes and document demo verification.

## [7.2.18-dev] - 2026-10-31

- Keep sample printer discovery retry available in offline demo.

## [7.2.17-dev] - 2026-10-31

- Enable the Network page in demo mode with local sample Wi-Fi and printer discovery.
- Keep sample passwords out of saved credentials and real Wi-Fi operations.

## [7.2.16-dev] - 2026-10-31

- Enable Setup Center in the offline demo with a sample network and printer discovery.
- Keep sample network credentials and discovery separate from real Wi-Fi.

## [7.2.15-dev] - 2026-10-31

- Add an offline OTA catalog and simulated transfer to tablet demo.
- Block real OTA workers in demo; allow cancellation and completion without reboot.

## [7.2.14-dev] - 2026-10-31

- Enable real Calibration dialogs in tablet demo using bounded local commands and results.
- Simulate probe accuracy, guided probe/Z, screws, gantry, shaper, resonance, PID and pressure-advance workflows.
- Keep demo saves local and reset calibration state on printer changes or disconnects.

## [7.2.13-dev] - 2026-10-31

- Keep the Dashboard Camera/Thumbnail toggle above incoming frames and thumbnail updates.
- Return from Dashboard camera fullscreen to the Dashboard live view.
- Mark the sample feed as DEMO CAMERA and remove the desktop-only layering workaround.

## [7.2.12-dev] - 2026-10-31

- Enable the actual Camera page in tablet demo with local sample views.
- Share sample artwork with the desktop, label it clearly, and isolate all camera transport.
- Keep selection, rotation, mirroring, fullscreen and catalog settings in demo RAM.

## [7.2.11-dev] - 2026-10-31

- Enable Devices, Macros and Drybox in tablet demo with shared offline catalogs.
- Publish local live readings, lighting values, motion details and positional endstop replies.
- Add bounded numeric sample-macro execution and filament-presence feedback.
- Simulate Drybox heating, humidity, Hold/Resume/Stop and emergency/error shutdown.
- Preserve per-printer jobs/filament/Drybox state and retain transport/storage isolation.


## [7.2.10-dev] - 2026-10-31

- Select Dashboard when opening it programmatically after a demo print start.
- Return successful normal-firmware print starts to Dashboard; preserve Files on failure/cancel.
- Publish demo object-exclusion capability, object names/polygons and per-printer excluded state.
- Reset exclusions for fresh sample jobs and printing/paused/idle scenarios.


## [7.2.9-dev] - 2026-10-31

- Share Files browsing/search/folders/details/print confirmation between desktop and tablet demo.
- Add embedded previews and local full-resolution thumbnail/metadata routes.
- Start sample jobs with consistent heating targets, progress and layer state; reject unknown or busy starts.
- Restore demo Dashboard/Printer previews and active layer/time updates without network or SD workers.
- Retire local preview/selection owners on printer switches. Live Files behavior remains unchanged.


## [7.2.8-dev] - 2026-10-31

- Add selectable tablet offline demo with restart confirmation and RAM settings.
- Keep saved printer profiles/preferences separate and block live transports in demo.
- Reuse the desktop printer model for simulated controls, progress, scenarios, Lighting and Bed Mesh.
- Mark unsupported firmware demo workflows explicitly; retain broader desktop fixtures.


## [7.2.7-dev] - 2026-10-31

- Place Operator's detected Lighting action inside the Tools header instead of
  partly above it. Reserve title/subtitle space while the button is visible
  and restore that space when the active printer has no case light.
- Reapply the alignment when the page resizes; retain Studio's plain heading
  and the existing detection and command guards.


## [7.2.6-dev] - 2026-10-30

- Exercise all 15 native page routes and standalone control dialogs in Studio
  and Operator, including repeated appearance and Printer-page rebuilds.
- Reset Printer card references before creating either layout, fixing stale
  Studio-only temperature targets when switching to Operator.
- Route desktop Escape through the foreground dialog's actual dismissal action
  instead of deleting its objects; simulated OTA cancellation now retires its
  timer and permits reopening. Preserve nested confirmations and dropdown owners.
- Keep fullscreen Camera/preview dismissal and notice acknowledgement with their
  real owners. Add an optional sanitizer build for host regression checks.


## [7.2.5-dev] - 2026-10-30

- Refresh Theme Maker against current main, preserving its native firmware UI
  and theme export/background support without importing the parked 3D viewer.
- Add an explicit Tablet simulation view using native widgets and built-in
  appearance independently of the theme being edited.
- Compile the real Setup Center, Lighting and OTA review/progress dialogs.
  Service replies and installation remain local simulations.
- Share command effects, timed heating and print progress in a portable model;
  add idle, printing, paused, completed, disconnected and error scenarios.
- Preserve backend state across page navigation and stop automatic native
  retry loops after a helper failure. Firmware offline demo integration follows
  separately; this patch does not enable it on the tablet.


## [7.2.4-dev] - 2026-10-30

- Fix an oversized-camera-test JPEG heap overwrite; enforce buffer capacity on
  every byte, bound the frame-read phase and reject failed HTTP/header responses.
- Reject truncated camera-test URLs before creating the worker.
- Reject missing, overlapping, changed-length and stale-generation WebSocket
  fragments before JSON parsing; reset assembly on reconnect/retirement.
- Reject non-finite/unrepresentable Bed Mesh numbers before publication.
- Stop application OTA logs from recording full download URLs and tokens.
- Remove the unused synchronous printer-health poller and stale API comment.
- Repair About modified-source/channel coverage and Bed Mesh fixture linkage.
- Review all main-repository documentation; correct security/build/recovery/OTA
  instructions, configuration defaults and paths, and label historical records.
- Add sanitizer-backed camera, fragment and mesh input regressions. See the
  [project audit](docs/PROJECT_AUDIT_2026_10_30.md) for findings and remaining risks.

## [7.2.3-dev] - 2026-10-29

- Label local `-dev` firmware as Development in Settings, About and OTA identity.
- Treat moves between Development, Nightly, Custom and Stable as channel switches;
  development patch counters no longer produce false stable downgrade warnings.
- Retain numeric upgrade/downgrade ordering within Stable and date ordering
  within Nightly. Show an explicit Switch to Stable confirmation.
- Keep modified-source marking visible for Development builds.
- Add legacy `7.2.2-dev` → `7.2.0` regressions and real OTA dialog checks.

## [7.2.0] - 2026-10-29

### Stable case lighting release preparation

- Consolidate Tools case_led detection, on/off and PWM brightness, slider feedback
  and active-printer visibility through v7.2.2-dev.
- Fix sub-1% On/Off handling and tighten live confirmation matching.
- Move command formatting outside the RTOS lock and fence concurrent changes.
- Remove unused test scaffolding; retain active API and model/transport coverage.
- See [v7.2.0 release notes](docs/releases/v7.2.0.md) for validation limits.

## [7.2.2-dev] - 2026-10-29

### Lighting discovery visibility

- Show Tools Lighting only for a detected case_led on the active printer.
- Recheck ownership on click to reject stale actions between refresh ticks.
- Retain the existing four Tools tiles and current lighting controls.

## [7.2.1-dev] - 2026-10-29

### Lighting slider feedback

- Hold the requested position while waiting for live confirmation.
- Use shared themed slider styling with a slim track and thumb clearance.
- Retain timeout/failure rollback and single-command release behavior.

## [7.2.0-dev] - 2026-10-29

### Tools case lighting

- Add live case_led on/off and PWM brightness in a Tools header action.
- Honor output scale, serialize release-only commands and confirm live values.
- Fence commands to the active printer and disable unavailable/offline controls.
- Add model/transport and native UI ownership/layout tests.

## [7.1.0] - 2026-10-28

### Stable release preparation

- Consolidate accepted non-viewer development through v7.1.55-dev: Files caching,
  Dashboard filename layout, live Bed Mesh, connection/setup and lifecycle fixes.
- Retire stale code and preview/UI compatibility paths; preserve stored settings
  migration and Quiet connection-loss warnings.
- Keep experimental G-code viewing and Theme Maker outside this release.
- See [v7.1.0 release notes](docs/releases/v7.1.0.md) for scope and validation limits.

## [7.1.55-dev] - 2026-10-28

### Remove unreachable Printer status formatter

- Remove an unused status-summary API; retain shared status dialogs.
- Keep long-file layout and lifecycle coverage on the active popup implementation.

## [7.1.54-dev] - 2026-10-28

### Remove obsolete preview crop renderer

- Remove unused center-crop sampling and its public entry point.
- Drop the redundant renderer mode flag; retain active packing and full-image fit.
- Keep active pixel, padding, dimensions and buffer-bound coverage.

## [7.1.53-dev] - 2026-10-28

### Remove unused lightbox and Wi-Fi wrappers

- Remove three lightbox and two scan wrappers with no firmware callers.
- Keep file-pinned fullscreen loading and shared scan/password implementations.
- Retarget existing ownership, worker and dialog tests to retained paths.

## [7.1.52-dev] - 2026-10-28

### Remove unused component constructors

- Remove generic Active Print, unowned page-state and wrapped-thumbnail constructors.
- Retarget fixtures to active profile/owned/standard creation APIs.
- Retain active layouts, buffer ownership and lifecycle checks.

## [7.1.51-dev] - 2026-10-28

### Remove obsolete button constructors

- Remove an unused custom-color constructor and five wrappers.
- Retain active semantic buttons and their pressed-state/layout coverage.
- Remove obsolete API names from the button-label inventory scanner.

## [7.1.50-dev] - 2026-10-28

### Remove stale notification and preview interfaces

- Remove two boolean notification APIs; retain stored preference migration.
- Remove unused Active Print filename-size and placeholder-hide helpers.
- Exercise current notification levels directly in existing host fixtures.

## [7.1.49-dev] - 2026-10-28

### Remove unused preview helpers

- Remove legacy scale/fill helpers and two unused convenience wrappers.
- Retarget safety fixtures to active file-image, placeholder and containment APIs.
- Retain decoder, cache, lightbox and worker behavior.

## [7.1.48-dev] - 2026-10-28

### Remove obsolete combined memory labels

- Remove two unused label-binding APIs, their cached state and formatter.
- Retain active metric-cell formatting and retarget lifecycle tests to those APIs.

## [7.1.47-dev] - 2026-10-27

### Remove unused entry points and state

- Remove the unreachable standalone Moonraker port popup; port editing remains in
  the printer-profile editor, including range validation and save retry.
- Remove three unused backup compatibility wrappers, two Dashboard thumbnail
  forwarding helpers, the duplicate thumbnail-ready setter and the unused Files
  destroy wrapper. Keep the active progress, preview and teardown paths.
- Remove the permanently-null Files popup label and never-written HTTP-busy flag;
  pass its existing false value explicitly without changing poll scheduling.
- Retarget host fixtures to active APIs and retain NULL-progress callback coverage.

## [7.1.46-dev] - 2026-10-27

### Shared shell ownership and refresh cleanup

- Retire top-bar/navigation handles on direct deletion and stop the clock timer
  when its top bar or clock label is retired. Preserve replacement shells opened
  by DELETE callbacks during teardown.
- Guard early/late calls and bind navigation/printer-switch actions to current
  controls. Retain all eight routes, E-stop behavior and printer-switch callback.
- Avoid identical clock, printer-name and Wi-Fi indicator writes; preserve clock
  formatting, timezone updates and RSSI hysteresis.
- Remove the unbound shell ETA field and an identical-font conditional.
- Add real shell lifecycle/clock/status tests on the pushed v45 checkpoint.


## [7.1.45-dev] - 2026-10-27

### Network page ownership and refresh cleanup

- Retire Network root/field references on direct deletion and preserve replacement
  pages created by DELETE callbacks during normal teardown.
- Avoid unchanged Network text/color writes and stop a retired owner's refresh.
- Guard nested scan rendering, bound SSID display/copy to 32 bytes and disable a
  scan row if its callback data cannot be allocated. Retain row-owned cleanup.
- Remove three permanently-null application editor handles, their destroy-helper
  arguments and the unused Network hint field. Current popup editors remain owned
  by Network Tools and Setup Center.
- Extend native Network tests for teardown/reopen, retired fields, replacement
  callbacks, refresh silence, bounded SSIDs, allocation failure and balanced row data.


## [7.1.44-dev] - 2026-10-27

### Timed control refresh cleanup

- Avoid rewriting unchanged Toolhead position, homed/feedback and Z-offset labels
  on every 500 ms refresh; preserve live changes and feedback expiry.
- Reject callbacks from unrelated timers in Toolhead, Hotend and Filament Sensor
  owners; retain current timer cadence and explicit immediate refresh calls.
- Remove the write-only Toolhead tab variable and duplicate hotend target array.
  Keep actual visible/selected tab state and all six exact preset commands.
- Extend native control tests for redraw silence, live transitions, timer ownership,
  displayed tab state, preset values and existing dispatch/lifecycle behavior.


## [7.1.43-dev] - 2026-10-27

### Setup completion check marks

- Replace hard-coded incomplete Time zone and Display navigation states with
  saved completion flags. Mark successful preference saves or Continue accepting
  the current values, and retain progress across reopen and reboot.
- Mark Welcome after Continue and Review after successful Finish. Previously
  completed setups satisfy the informational/preference steps automatically.
- Keep Network, Printer and optional Camera checks tied to their current state;
  skipping an unconfigured camera never marks it complete.
- Keep failed/canceled saves unchecked; completion-marker failures remain
  retryable without discarding a successful appearance save or its UI rebuild.


## [7.1.42-dev] - 2026-10-27

### Setup deferred callback cleanup

- Cancel queued theme/text changes when Setup Center or its owned fields are
  deleted, releasing the busy latch so reopened Setup remains navigable.
- Match deferred callbacks to their pending epoch; a stale callback cannot
  consume or unlock a replacement owner's pending appearance change.
- Retire the permanently-null step-window handle, its unreachable cleanup helper
  and nine old patch-marker comments. Retain current stage and poll cleanup.
- Extend the real Setup audit with cancellation/reopen, no-save-on-cancel,
  bounded timers, duplicate clicks and callback-driven owner replacement cases.


## [7.1.41-dev] - 2026-10-27

### Calibration timer and callback ownership

- Retire the Calibration refresh timer and detached workflow dialogs when LVGL
  deletes the page directly, as well as through normal navigation.
- Clear Probe/Z and Axis Twist confirmation handles on direct deletion. Prevent
  rebuilding the page inside teardown and reject callbacks from retired timers.
- Guard calibration starts against duplicate transport reentry and stop old UI
  transitions when the command callback closes the page or switches printers.
  Recheck capabilities, idle state and reviewed homing before dispatch.
- Extend the real LVGL Calibration audit with teardown/reopen, printer-switch,
  command-reentry and rejected-start cases. Keep Quiet connection-loss warnings.


## [7.1.40-dev] - 2026-10-26

### Unbound telemetry and label plumbing

- Retire eight permanently-null legacy Dashboard metric slots and their no-op
  runtime branches; current live Dashboard writers retain metric ownership.
- Remove unused Printer/top-bar label aliases and context fields, two ETA wrappers
  reached only through the null top-bar slot, and a write-only HTTP success timestamp.
- Remove ignored Printer banner/Wi-Fi row/webcam-normalization arguments and the
  obsolete Wi-Fi absolute-row-offset accumulator. Keep current rendering and URL policy.
- Add actual Dashboard runtime pipeline coverage for timing/layers, offline/reconnect,
  preview routing and completion cleanup. Retain recent connection/notification fixes.


## [7.1.39-dev] - 2026-10-26

### Retire compatibility plumbing

- Remove the no-op Dashboard filename setter and its no-op completion call;
  the shared status banner retains filename ownership.
- Retire empty Printer filename handles and controller fields, plus ignored
  metadata/filename/state/progress arguments from the live-status interface.
- Remove the ignored Camera setup callbacks and Drybox button fill parameter,
  updating all current production and host-fixture callers.
- Delete the unused application polling-interval macro; actual polling and
  WebSocket freshness scheduling remain unchanged.
- Preserve connection, notification, preview and Files fixes from v36-v38.


## [7.1.38-dev] - 2026-10-26

### Quiet-mode connection recovery

- Keep confirmed connection-loss warnings in Quiet. On recovery, clear the old
  offline warning without showing a reconnected/online card in Quiet.
- Record both loss and recovery in Event History. Standard/Detailed retain both
  cards; switching to Quiet removes queued recovery cards but keeps loss warnings.
- Classify connection notices explicitly, preserving printer faults, paused-print
  warnings, generic warnings and essential recovery instructions at every level.
- Update notification preference descriptions and real-owner regressions for
  active and inactive printers. Transport state/timing and backup settings remain unchanged.


## [7.1.37-dev] - 2026-10-26

### Connection-state publication race

- Publish verified WebSocket status time under the same lock as connection flags.
  Recheck that timestamp under the lock before the runtime expires stale status.
- Prevent an earlier stale probe from overwriting a newer online update during
  switching/subscription. Derive runtime flags from the synchronized snapshot,
  including the fresh fast path, so a real disconnect wins immediately.
- Retain the three-second freshness bound, profile reset, Klipper readiness,
  disconnect events and WebSocket-owned reconnect without competing HTTP polling.
- Add real JSON/status/runtime interleaving regressions; the v36 runtime fails
  the reproducer. v36's preview continuity remains in place.


## [7.1.36-dev] - 2026-10-26

### Printer-switch preview continuity

- Reset preview state once before restoring the destination profile's cached
  image, instead of clearing it a second time after restoration.
- Keep a matching RGB canvas visible when live printing/paused status starts
  fresh thumbnail verification; retire PNG-backed image objects before freeing
  their source and still clear mismatched or profile-invalidated canvases.
- Retain fresh metadata/thumbnail requests, failed-download retry bounds and
  profile/endpoint validation. Add actual switch-bridge and coordinator regressions.


## [7.1.35-dev] - 2026-10-26

### Internal and repository cleanup

- Retire five unused helper APIs, including the thumbnail wrappers left uncalled
  after the old global restore path was removed; remove their three private
  copy/icon lookup helpers as well.
- Remove the uncalled title-based Printer icon decorator. Current cards keep their
  existing icon and color construction; no visible UI change is intended.
- Delete the completed July root-archive script and seven empty tracked root files
  with no build/configuration consumers. Active recovery and build scripts remain.
- Correct outdated Phase-1 comments in WebSocket and shell interfaces to describe
  current snapshot publication and application routing ownership.


## [7.1.34-dev] - 2026-10-26

### Stale API cleanup from current main

- Retire 19 uncalled helper definitions and their declarations, plus an
  unimplemented Active Print filename declaration and the obsolete global
  thumbnail-restore result enum.
- Remove the unused direct Moonraker state accessor; active asynchronous consumers
  continue using snapshots. Retain active profile save, security, webcam list,
  per-profile preview, encrypted worker and variable-distance toolhead paths.
- Remove unused reset/getter/configuration shims and empty toolhead/Wi-Fi init
  stubs. Correct the Wi-Fi scan module's outdated ownership comment.
- Keep Files caching, Dashboard filename behavior, Bed Mesh viewing and the
  non-viewer boundary from main unchanged. No firmware speed or size gain is claimed.


## [7.1.33-dev] - 2026-10-26

### Independent fixes and cleanup from main

- Keep the experimental 3D G-code viewer on its existing branches; this branch
  starts at main's bce8481 and carries no viewer source, actions, touch-port changes
  or experimental tools.
- Reuse revision-matched Files row RGB565 thumbnails in RAM and in bounded SD
  buckets, choose smaller source images, and reject stale profile results.
- Extend Studio's Dashboard filename above the percentage and keep long names
  scrolling without overlapping large-text progress.
- Display available Bed Mesh snapshots while printing or paused; retain idle-only
  calibration and profile-changing controls.
- Retire the obsolete source rewrite scripts, unused preview/motion/status modules,
  thumbnail prototype and unconsumed UI factories. Preserve live data migrations,
  recovery, themes and build helpers.
- Add standalone source/boundary, cache corruption, thumbnail ownership/worker,
  actual-page and live Bed Mesh regression checks.


## [7.0.0] - 2026-10-20

### Added and improved

- Studio interface redesign and system-wide LVGL 9.5 modernization.
- Consistent responsive dialogs, keyboard layouts, cursor feedback, scrolling,
  large-text controls and theme previews.
- Printer setup, profiles, authentication/TLS/certificate selection and camera
  configuration workflows.
- Filament recovery, active-print temperature/fan controls, calibration and
  motion diagnostics, Files previews, Console, telemetry and notification levels.
- Plain/encrypted configuration backup and restore progress, results and
  passphrase feedback, including TLS and primary camera settings.
- Firmware channel/revision identity, OTA review, transfer metrics and cancellation.

### Fixed

- Stale dialog callbacks, popup teardown, duplicate dispatch, profile switching
  and retry handling across operator controls.
- Preview/camera ownership, file navigation, live refresh and status invalidations.
- CA import rollback, duplicate-endpoint TLS downgrades, URL validation,
  restored API-key validation, print filename JSON encoding and size bounds.
- Backup serializer allocation failures, malformed/deep SD JSON, lost camera
  URLs, truncated passphrases/firmware URLs and OTA catalog mutex failures.

### Release notes

- Stable publication builds ESP-IDF 6.0.2 P4 and ESP-Hosted 3.0.5 C6 assets.
- The previous intermittent backup-to-OTA panic/rollback remains unconfirmed;
  these changes are not claimed to resolve that event.

## [6.5.6] - 2026-10-04

### Added

- Repeated-sample Probe Accuracy checks with final statistics.
- Live endstop queries under Devices, refreshed every two seconds while idle.
- Public-macro search, detected parameter fields and final command review.
- Motion runtime limits, a hardware-geometry axis-distance calculator and TMC driver diagnostics.

### Fixed

- Recheck printer readiness and workflow requirements when Tools actions run.
- Fence calibration sessions and diagnostics to the active printer and handle timeouts.
- Save Bed Mesh profiles and SAVE_CONFIG as one confirmed, ordered script.
- Use an overlap-safe Motion driver-name copy for optimized firmware builds.

### Documentation

- Align current product/version references, architecture, source catalog, Tools guide and release checks with 6.5.6.

## [6.5.5] - 2026-10-03

### Fixed

- Refresh Dashboard thumbnails for browser-started prints without requiring page navigation; retry transient metadata/download failures with bounded, spaced attempts.
- Wait for Moonraker readiness before fetching the live print preview.
- Keep Dashboard and Printer layer estimates synchronized using live G-code Z.
- Preserve virtual-SD progress priority across partial WebSocket updates and use consistent integer progress displays.

## [6.5.4] - 2026-08-26

### Fixed

- Corrected Dashboard print ETA so it remains a completion time instead of tracking the current clock after a print completes.

## [6.5.3]

- Harden UI popup teardown and first-click file metadata/preview loading.
- Improve camera, OTA, printer-state, and Wi-Fi responsiveness.


## [6.5.2] - 2026-08-22

### Fixed

- Refresh Dashboard camera visibility immediately when switching printers.
- Keep the Camera/Thumbnail toggle interactive when it becomes visible after a printer change.
- Preserve the all-theme in-card camera/thumbnail placement correction.



## [6.5.1] - 2026-08-22

### Fixed

- Hardened ESP32-P4 JD9165 LCD reset timing for reliable startup and wake recovery.
- Explicitly enables the JD9165 panel after initialization before the backlight is restored.
- Preserved the stable camera, dashboard, and operator-shell behavior from v6.5.0.



## [6.5.0] - 2026-08-20

- Added the guided first-run Setup Center with in-card Wi-Fi, printer, and camera discovery.
- Improved offline setup behavior, camera/OTA lifecycle handling, and shared localization coverage.

This file records product-level changes. Detailed development history before
v4.0.0 is preserved under `docs/history/`.

## [6.4.1] - 2026-08-16

### Fixed

- Removed the default-camera NVS-key format-truncation warning that caused the
  v6.4.0 GitHub ESP-IDF build check to fail despite producing a valid firmware image.

## [6.4.0] - 2026-08-16

### Added

- Completed per-printer camera management for up to four Moonraker-discovered MJPEG streams.
- Added persistent friendly camera names, direct named slot selection, default-camera selection, and safe camera removal confirmation.
- Camera discovery now imports and reports every webcam returned by Moonraker, retaining each discovered name and URL.

### Changed

- USE CAMERA applies the selected camera immediately; the Camera page updates its selector and stream without requiring page navigation.
- The Camera page honors the saved default camera on initial open after boot and after profile changes.

## [6.3.0] - 2026-08-15

### Added

- Added Camera v2: a persistent per-printer catalog for up to four camera streams, secure Moonraker webcam discovery/import, identified active-camera selection, fullscreen live MJPEG viewing, PSRAM-first stream storage and hardware JPEG decode.

### Fixed

- Gave secure camera discovery a PSRAM-first TLS task stack, eliminating the camera-search stack-protection reset.
- Cleared the last decoded camera frame when the active printer has no configured camera.
- Kept the startup splash at a solid backlight level until handoff, preventing the hard-start flicker captured at reduced saved brightness.
- Rebuilt the persistent Camera page after a theme change and normalized its footer controls so they remain within the live-view card.

### Validation

- Verified on the ESP32-P4 panel: secure camera discovery, live stream, fullscreen, unconfigured-camera behavior, cold boot, OTA and Operator Shell navigation.

## [6.2.1] - 2026-08-15

### Fixed

- Stabilized the startup splash by keeping it on LVGL's top layer during page construction, reducing progress redraws, and delaying backlight restoration until the first MIPI frame is stable.
- Made the Files page transition immediate while its Moonraker library request starts after the first rendered frame.
- Added automatic retries for transient Moonraker file-list fetch failures.

### Changed

- Normalized the existing per-printer camera configuration declarations and NVS persistence path in preparation for Camera v2.

### Validation

- Verified startup splash behavior, Files navigation and refresh, Camera streaming, and the complete Operator Shell on the ESP32-P4 panel.

## [6.2.0] - 2026-08-15

### Added

- Added the Operator Shell theme: an eight-item print-cell navigation rail and a consistent operational layout across Dashboard, Printer, Files, Camera, Tools, Console, Drybox, and Settings.
- Added live MJPEG camera streaming for the active printer, including Moonraker camera discovery, per-printer stream configuration, PSRAM-first frame storage, continuous hardware JPEG decode on ESP32-P4, fullscreen viewing, and Dashboard preview.
- Added a four-card Tools hub for Calibration, Bed Mesh, Devices, and Macros.

### Changed

- Redesigned the printer-profile editor and camera setup workflow with shared popup controls and usable on-screen keyboards.
- Reworked the Printer page into an Active Print workspace with compact live controls, complete machine metrics, and a lower action rail.
- Made Files open immediately, autoload its library after the first frame, and retry transient Moonraker file-list failures automatically.
- Made the splash overlay render on LVGL's top layer, preventing startup page construction from flashing through it.

### Validation

- Verified on the ESP32-P4 panel: eight-item navigation, live Camera stream, Camera configuration, Files browsing and refresh, Tools routing, Console, Drybox, Settings, and Operator Shell layout fitting.

## [6.1.1] - 2026-08-11

### Added

- Added current/minimum internal-heap and PSRAM diagnostics plus the largest
  contiguous internal block to Settings → System Information.

## [6.1.0] - 2026-08-09

### Added

- Released opt-in per-profile HTTPS/WSS Moonraker transport with verified local CA trust, no HTTP downgrade, and unchanged optional API-key behavior.
- Added per-profile SD-card-root `.pem` selection, PSRAM-first certificate storage, and automatic HTTPS port 443 selection.
- Added a public generic Nginx installer that validates Moonraker first, creates an IP/DNS-SAN certificate, verifies the HTTPS endpoint, exports the public CA, and restores the prior site if verification fails.


- Added a non-publishing GitHub Actions CI gate for source-policy audits, portable dependency-lock validation, and a clean ESP-IDF 6.0.2 P4 build with a firmware-size budget.

### Changed

- Added a persistent active-printer E-STOP, named stop/recovery feedback, and confirmed Settings restart control.

- Added a clear attended-operation confirmation before a PID heater heat cycle starts.

- Extracted the shared calibration-results and confirmed Apply & Restart workflow into its own explicit-context owner.

- Extracted the PID calibration picker and guarded heater command dispatch from the Calibration page into its own explicit-context workflow owner.









- Extracted Custom Calibration macro selection and dispatch into `ui_calibration_custom` with explicit page callbacks.
- Extracted Screws Tilt and Z Tilt/QGL workflows into `ui_calibration_geometry` with explicit page-owned context.
- Extracted reusable Calibration cards, labels and capability summaries from `ui_calibration` into `ui_calibration_layout` without changing calibration workflows.
- Extracted OTA release-catalog and custom-URL popup dispatch from `main.c` into `ota_ui_controller` without changing OTA transport behavior.
- Extracted NVS-only Wi-Fi credential persistence from `main.c` into `wifi_credentials_store`; connection and UI behavior remain unchanged.
- Extracted post-splash OTA rollback confirmation from `main.c` into the focused `ota_boot_validation` service without changing update behavior.
- Clarified current module ownership boundaries and future extraction seams without changing runtime behavior.
- Completed the active-source legacy naming cleanup: `_v32` API and type suffixes are now removed without changing behavior.
- Removed the non-interactive in-app Theme Laboratory. Custom themes remain available as validated SD-card packages, with clear external-authoring guidance.
- Updated active documentation and release tooling for the `khelix1/PrinterHMI` repository rename, the supported ESP-IDF 6.0.2 platform, and the active CI gate.
- Removed legacy `_v32` filenames from active modules; behavior and public interfaces are unchanged.
- Removed obsolete dashboard helpers so fresh PrinterHMI builds remain warning-free.
- Made CI verify the tracked ESP-IDF SDIO compatibility correction by target version, file set, line count and source markers, avoiding container-specific Git diff metadata.
- Added a strict CI gate that rejects compiler warnings from PrinterHMI application sources without treating upstream ESP-IDF notes or third-party diagnostics as product warnings.

## [6.0.6] - 2026-08-08

### Added

- Optional AES-256-GCM encrypted configuration backups. Encrypted backups include Moonraker API keys only after passphrase protection; ordinary backups remain key-free.
- Responsive progress feedback while creating encrypted backups and verifying encrypted restores, plus a confirmed Clear All Backups operation that never changes live settings.
- Optional per-printer Moonraker API-key authentication. API keys are masked in the editor, used by HTTP and live WebSocket connections, excluded from ordinary SD-card configuration backups, and never written to event logs.

### Changed

- Moonraker discovery now completes a full local IPv4 subnet scan, testing ports 7125 through 7128 with the proven one-address TCP probe. Once an address responds, all four ports are HTTP-verified serially so separate instances on the same host are not skipped.
- Added a non-blocking Test action to the printer profile editor. It verifies a manual Moonraker endpoint and reports the detected printer identity and Klipper readiness before an operator explicitly saves.
- Clarified the About popup platform identity: ESP-IDF's `-dirty` suffix explicitly identifies the intentional ESP-Hosted SDIO compatibility patch.
- Configuration backup restore now validates before replacement, preserves recoverable previous files, and keeps destructive backup cleanup separate from live printer settings.

## [6.0.5] - 2026-08-07

### Changed

- Empty printer slots now open directly into the matching Add Printer form.
- Moonraker discovery identifies verified candidates by printer hostname when
  available, pre-fills fresh profile names, marks Klipper readiness, and uses
  bounded batched TCP port checks before HTTP verification.

## [6.0.4] - 2026-08-07

### Fixed

- Bound disconnected Moonraker WebSocket attempts to two seconds and
  paused inactive-printer HTTP work while the active endpoint reconnects.
- Added a 20-second cooldown after an inactive-printer failure so an
  unavailable profile cannot repeatedly contend for hosted Wi-Fi.
- Made a selected unavailable profile show `OFFLINE / RETRYING` immediately
  instead of the ambiguous `CHECKING...` chooser state.
- Preserved per-profile health across active-printer changes only when the
  profile endpoint is unchanged. The serialized inactive-profile worker now
  publishes each printer's current Moonraker print state to the chooser.
- Expired inactive positive state after five seconds. One missed inactive
  request now shows `VERIFYING...`; only a second confirmation failure
  changes an already-online printer to `OFFLINE`.

### Validation

- Built the ESP32-P4 firmware through `tools/build_idf6_hosted3.sh`.
- Verified active Moonraker reconnection remains responsive.
- Verified inactive healthy, starting, unreachable, shutdown and recovery
  states update automatically in the chooser without selecting a printer.

## [6.0.3] - 2026-08-05

### Added

- Added a professional Settings > About PrinterHMI popup with live image
  identity, software acknowledgements and creator credit for khelix.
- Added precise shared Manual Probe controls for Probe/Z and Axis Twist:
  symmetric TESTZ steps from 1.00 mm down to 0.005 mm.

### Fixed

- Stable and Nightly release selections now use their GitHub asset only for
  that installation. They no longer replace the saved Custom OTA URL.

### Validation

- Built the ESP32-P4 firmware through `tools/build_idf6_hosted3.sh`.
- Verified the About popup, Custom OTA URL persistence and fine TESTZ
  controls on the target panel.


## [6.0.2] - 2026-08-05

### Added

- Modernized Moonraker discovery with the shared modal popup, themed status,
  scrollable list, selectable rows and semantic footer actions.
- Added discovery of Moonraker endpoints on ports 7125 through 7128 and
  guaranteed every counted endpoint is published to the result list.
- Added a confirmed `REMOVE SELECTED` action to Manage Printers. It removes
  only the HMI's local profile, health state and cached preview; it never
  changes Klipper, Moonraker or the physical printer.

### Fixed

- Kept the operator UI responsive while an active Moonraker WebSocket is
  reconnecting by suppressing the synchronous HTTP fallback on the shell
  runtime task and making reconnect/discovery sends non-blocking.
- Moved inactive-printer preview and health HTTP work to its own
  low-priority worker so an unreachable saved profile cannot stall touch or
  page navigation.
- Made `virtual_sdcard.progress` authoritative on the WebSocket path and
  reset job-scoped progress, duration and layer state at each new print.
  A newly started job can no longer inherit the previous job's percentage
  or layer number while Moonraker status refreshes.

### Validation

- Built the ESP32-P4 firmware through `tools/build_idf6_hosted3.sh`.
- Verified disconnected Moonraker handling, printer-profile removal and
  fresh new-print progress/layer presentation on the target panel.

## [6.0.1] - 2026-08-02

### Added

- Added a reproducible full-stack builder for the ESP32-P4 application and
  matching ESP32-C6 ESP-Hosted 3.0.5 firmware.
- Added normal P4 and C6 full-flash images, component images, an internal
  manifest, flashing instructions and SHA-256 verification to one release
  archive.
- Added optional bootstrap installation of the exact ESP-IDF 6.0.2 tag.

### Changed

- Made ESP-IDF 6.0.2 the only active build toolchain for the current P4+C6
  firmware stack.
- Replaced the stable publisher's external transition-image dependency with
  the newly built and verified IDF6 stack archive.

### Validation

- Built both processors from the current repository workflow.
- Verified every packaged file and the outer archive against SHA-256 sums.
- Verified byte-for-byte placement of the P4 application at `0x20000` and
  the C6 application at `0x10000` in their full-flash images.


## [6.0.0] - 2026-08-01

### Changed

- Migrated the ESP32-P4 application to ESP-IDF 6.0.2.
- Upgraded the P4 host and ESP32-C6 co-processor to ESP-Hosted 3.0.5 with
  RPC v2.
- Upgraded ESP Wi-Fi Remote to 1.5.3.
- Established `tools/build_idf6_hosted3.sh` as the canonical build path.
- Restored the removable SD card on its native high-speed interface.

### Fixed

- Eliminated active-printer switching crashes and related OTA instability.
- Removed obsolete network settling and readiness delays while retaining
  WebSocket generation fencing and exclusive OTA ownership.
- Prevented release-catalog traffic from overlapping an active OTA download.
- Added the missing ESP-Hosted 3.0.5 host-side Wi-Fi RSSI RPC dispatch.
- Added accurate live Wi-Fi signal bars to the top status area.

### Build and migration

- Added guarded, tracked compatibility patches for ESP-IDF 6.0.2 SDIO and
  ESP-Hosted 3.0.5 RSSI dispatch.
- Added a verified C6 3.0.5 USB transition image for devices upgrading from
  the v5.1.2 C6 2.12.8 stack.
- Updated stable and nightly publishing to use the reproducible IDF6 build.

### Validation

- Verified matching ESP-Hosted 3.0.5 versions, RPC v2 and 40 MHz four-bit
  SDIO on the target panel.
- Verified repeated printer switching during prints, chooser state, OTA,
  SD-card mounting and live RSSI reporting.
- Verified reboot, OTA validation and normal Moonraker WebSocket operation.

## [5.1.2] - 2026-07-31

### Fixed

- Replaced the unstable ESP-Hosted 2.12.8 P4 host component with the verified
  2.9.3 host while retaining the installed ESP32-C6 2.12.8 firmware.
- Serialized short HTTP transactions and reserved exclusive network ownership
  for Wi-Fi scans and actual firmware downloads.
- Prevented the GitHub release catalog from blocking Custom URL OTA startup.
- Expanded the PSRAM-backed GitHub release response buffer from 128 KiB to
  256 KiB so the growing stable and nightly catalog is not discarded.
- Added visible release-catalog and OTA-start failure diagnostics.

### Validation

- Verified sustained WebSocket operation, inactive-printer health polling,
  chooser updates and repeated active-printer switching without an
  unrecoverable SDIO restart.
- Verified Devices, Stable OTA, Nightly OTA and Custom URL OTA workflows on
  the target panel.

## [5.1.1] - 2026-07-31

### Fixed

- Matched the ESP-Hosted P4 component and C6 firmware at 2.12.8.
- Stabilized Devices and OTA transitions by retaining one capability-aware
  WebSocket subscription and quiescing competing network activity before OTA.
- Preserved TX byte transfers with RX block transfers to avoid intermittent
  unrecoverable SDIO timeout resets.
- Added boot diagnostics for the ESP32-C6 co-processor firmware version.

### Build

- Added a guarded CMake hook that reapplies the tested ESP-Hosted transfer
  mode after managed-component regeneration.
- Kept the known-good ESP-IDF 5.4.4 and 40 MHz four-bit SDIO configuration.

### Validation

- Verified repeated Devices-to-OTA cycles, successful OTA completion and
  reboot with matching ESP-Hosted 2.12.8 host and co-processor versions.


## [5.1.0] - 2026-07-31

### Added

- Added a capability-aware Calibration workspace with guided PID, Input
  Shaper, Axis Twist, Z Tilt, Pressure Advance, Probe Z and custom workflows.
- Added shared manual-probe controls, calibration session tracking and
  confirmation before persistent SAVE_CONFIG operations.
- Added a capability-aware Devices catalog with category filters, pagination
  and synchronized live values.
- Added printer-action resolution and a repeatable v5 feature-architecture
  ownership audit.

### Changed

- Split Calibration into page composition, motion, Pressure Advance and
  manual-probe modules.
- Split Bed Mesh into page composition, gestures, rendering and profile
  controls.
- Split Devices into page composition, catalog view, live-value adapter and
  controller layers.
- Preserved the existing Console and Macros page/controller boundaries.

### Validation

- Verified incremental and final ESP-IDF 5.4.4 builds on the target firmware
  tree.
- Verified Calibration, Bed Mesh, Devices, Console and Macros on the target
  panel.
- Verified architecture, version, public-tree and firmware identity audits.

## [5.0.0] - 2026-07-28

### Added

- Added Bed Mesh as a dedicated sidebar page with a solid height-colored 3D
  surface, optional surface grid, rear X/Y/Z reference planes, origin markers
  and minimum, maximum and range statistics.
- Added drag rotation, responsive pinch zoom and two-finger graph panning.
- Added Bed Mesh calibration and detected profile management.
- Added a dedicated Console page with bounded command/response history,
  JSON-safe transport and response severity classification.
- Added automatic discovery of public Klipper macros with helper-macro
  filtering, execution confirmation and Console history integration.

### Changed

- Reordered the sidebar into Dashboard, Printer, Files, Bed Mesh, Macros,
  Console, Telemetry, Drybox, Network and Settings.
- Replaced the BED-card long-press entry point with a first-class Bed Mesh
  destination.
- Routed the new pages through shared themes, semantic buttons, modal popups,
  standard page geometry and existing Moonraker transport.
- Moved persistent Macros page and shell state into PSRAM-backed contexts
  allocated after scheduler startup.

### Fixed

- Corrected Bed Mesh pinch direction, sensitivity, redraw pacing and gesture
  competition.
- Corrected mesh origin placement, surface-line visibility and rear reference
  grid placement.
- Prevented overlapping profile-name copies during profile sorting.
- Protected FreeRTOS timer-task creation from the internal startup-RAM
  regression introduced by static Macros and sidebar state.

### Validation

- Added v5-specific build, startup, sidebar, Bed Mesh, Macros and Console
  release gates.

## [4.2.2] - 2026-07-26

### Changed

- Unified active-print filename presentation across the Dashboard and Printer
  page.
- Made `virtual_sdcard.progress` the primary live progress source, with
  `display_status.progress` retained as a compatibility fallback.
- Unified Remaining and ETA calculations under the shared printer controller.
- Made the top bar show the actual estimated completion clock instead of
  labeling the remaining-duration value as ETA.

### Fixed

- Corrected volumetric-flow presentation by treating retraction velocity as a
  magnitude and suppressing insignificant idle noise.
- Kept Dashboard, Printer page and top-bar timing values synchronized.
- Made completed jobs report `0:00` remaining while unknown timing remains
  `--:--`.


## [4.2.1] - 2026-07-25

### Added

- Added an on-device browser for stable releases and identity-aware nightly
  builds published by the official PrinterHMI GitHub repository.
- Added separate `STABLE` and `NIGHTLY` release channels with development-build
  warnings and exact installed-nightly detection.
- Added release status, publication date and time, firmware size and
  release-note details for each compatible OTA asset.
- Added explicit installation, downgrade and same-version reinstall
  confirmation paths.
- Added a secondary custom OTA URL path for local and development servers.

### Security

- Added trusted certificate-bundle verification to the GitHub Releases API
  request and HTTPS release-asset OTA download.
- Restricted stable entries to non-draft semantic-version releases containing
  the exact expected PrinterHMI OTA asset.
- Restricted nightly entries to prereleases with strict nightly tags, exact
  firmware assets and matching embedded-identity release metadata.

### Changed

- Made the official remote release browser the primary Settings OTA entry.
- Kept GitHub catalog parsing off the LVGL thread.
- Moved release-catalog buffers, task stack, mutex and selected-release state
  to PSRAM-backed runtime allocation to protect startup internal RAM.
- Increased OTA HTTP transmit buffering for GitHub signed-asset redirects.
- Updated nightly publishing so each firmware image embeds its exact nightly
  tag while restoring the stable version file after the build.

## [4.2.0] - 2026-07-25

### Added

- Added versioned custom-theme packages loaded from the SD card.
- Added custom-theme discovery, validation, preview, selection, persistence,
  configuration backup/restore and protected removal.
- Added per-page custom layout geometry, palette, radius and surface-opacity
  overrides with built-in theme fallbacks.

### Changed

- Upgraded LVGL from 9.2.2 to 9.5.0.
- Kept Foundry, Operator and Dark Glass compiled into firmware as protected
  fallback themes.

### Fixed

- Added missing standard I/O declarations found by the LVGL clean rebuild.

### Validation

- Verified a clean ESP-IDF 5.4.4 build with LVGL 9.5.0.
- Verified startup, navigation, built-in themes, popups, previews, telemetry,
  scrolling and display sleep/wake on target hardware.

## [4.1.0] - 2026-07-24

### Added

- Added capability-driven discovery and controls for as many as four hotends,
  including live temperatures, independent targets and active-tool selection.
- Added live Moonraker switch and motion filament-sensor status, with compact
  single-sensor state and detailed multi-sensor reporting.
- Added operator event history with chronological live updates and clear
  control.
- Added configuration backup and restore.
- Added Moonraker discovery directly to printer profile Add/Edit.
- Added an active-print layer fallback based on file metadata when the slicer
  does not publish `SET_PRINT_STATS_INFO`.
- Added nightly firmware and SHA-256 publication through the end-of-night
  checkpoint workflow.

### Changed

- Made printer controls reflect discovered Moonraker capabilities.
- Added auto-scaling telemetry ranges and removed unnecessary chart sliders.
- Hardened WebSocket profile rebinding, stale-client event handling and
  capability rediscovery.
- Expanded Dashboard status cards for filament state while preserving preview
  proportions.
- Enabled the two-framebuffer, avoid-tearing LVGL direct-mode display path.
- Separated the fast OTA UI timer from the general application refresh timer.
- Moved ESP-Hosted/C6 transport initialization ahead of visible display
  startup.
- Normalized OTA keyboard, editor and progress-popup transitions.

### Fixed

- Fixed delayed OTA keyboard and progress-popup closure.
- Fixed OTA cancellation responsiveness and final image validation handling.
- Fixed Add Printer host/IP and hostname text entry.
- Fixed active-printer connection failure and recovery states.
- Fixed live capability state after switching between Moonraker profiles.
- Reduced startup flashing to a single initial panel appearance.
- Removed obsolete UI compatibility symbols reported by the compiler.

### Validation

- Verified clean ESP-IDF 5.4.4 builds and target operation.
- Verified cold boot, warm reboot, power-cycle and display sleep/wake behavior.
- Verified multi-printer switching, Moonraker/Klipper restart recovery,
  previews, telemetry, layer reporting and printer controls.
- Verified configuration backup/restore and OTA update behavior.

## [4.0.0] - 2026-07-22

### Added

- Multi-printer profile management for up to four Moonraker endpoints.
- Runtime Classic, Operator and Dark Glass themes.
- Accent, density and accessibility appearance controls.
- Theme Lab and theme previews.
- Persistent timezone, brightness and display-sleep settings.
- Modal shared popups with normalized footers.
- Exclude-object visualization and selection.
- Files search, row thumbnails, long-press preview and metadata display.
- Printer-profile preview caching on SD storage.
- PrinterHMI logo assets and splash integration.

### Changed

- Normalized operator pages, status bars, page geometry and popup controls.
- Routed current UI styling through shared theme and component modules.
- Reduced routine serial output after startup stability testing.
- Moved suitable large buffers and preview data toward PSRAM-backed storage.

### Fixed

- Protected LVGL updates with the display lock during startup and profile
  identity changes.
- Corrected OTA first-boot validation and rollback cancellation.
- Corrected clock timezone handling after SNTP synchronization.
- Corrected several popup, file-preview and cancel-object build/runtime defects.

## Historical releases

The v3.0 through v3.3 transition was an iterative hardware bring-up and UI


## Development Update — Public Repository Hardening

### Public repository audit modularization

Extracted the embedded public-tree safety audit from `tools/end_of_night_checkpoint.sh` into the new reusable script:

* `tools/audit/public_tree_audit.sh`

`end_of_night_checkpoint.sh` now delegates repository validation to the standalone audit tool, making the audit reusable for future GitHub Actions workflows and manual validation while preserving existing checkpoint behavior.

### Compiler format-warning cleanup

Removed the global compiler suppression:

```cmake
add_compile_options(-Wno-format)
```

and replaced it with:

```cmake
add_compile_options(-Wformat=2)
```

Resolved every format warning originating from PrinterHMI-owned source code.

Changes included:

* Safe truncation handling for dashboard print filenames.
* Safer folder path construction in the Files controller.
* Increased buffer size for custom temperature popup initialization.
* Replaced unnecessary empty `snprintf()` usage with direct buffer clearing.

### Validation

Completed successfully:

* Clean project build.
* No remaining PrinterHMI-owned format warnings.
* Hardware smoke testing passed.
* Public repository safety audit continues to pass after modularization.

Remaining format warnings originate only from third-party components (ESP-IDF and LVGL) and are outside PrinterHMI source ownership.

### Project impact

This work improves overall code quality and prepares the project for future continuous integration by:

* Eliminating a project-wide compiler warning suppression.
* Making repository safety validation reusable.
* Reducing technical debt without changing runtime behavior.

ownership refactor. Its original plans, crash records and step-by-step logs are
retained verbatim under `docs/history/`.
