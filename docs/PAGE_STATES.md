# Loading, empty and error states

The page-state pass preserves the current themes and Drybox layout. Workflow feedback stays inline; routine loading/retry actions do not create global notifications.

## Files

A reload clears obsolete rows and marks the index unavailable until the latest request succeeds. Search, sort and folder actions cannot redraw a cached index over loading or an error. Configuration changes retire the previous folder/search context. Worker retries stop before another attempt when the request has been superseded. Existing request/configuration/page checks fence publication; actual confirmation/details modals continue deferring publication. Embedded previews retain their accepted refresh behavior.

Empty library, empty folder and no search results have distinct guidance. The existing Refresh, Up and Clear Search actions provide recovery. Network requests keep their existing finite transport timeout and retry limit; leaving the page prevents publication into its retired owner.

## Camera

Unavailable streams expose an inline Retry action. Retry requests worker shutdown without blocking navigation, clears the old frame, and lets the normal poll restart after shutdown. Automatic backoff remains available. No configured camera directs the user through the existing Configure action. Status text wraps rather than clipping; the first valid frame immediately reports live reception, and normal FPS updates remain intact.

Configuration changes retire camera popups and pixels before accepting another frame, select the new profile's default camera, and refresh the selector. Hide/destroy retain the existing nonblocking stop and pixel/cache cleanup.

## Discovery and Setup

Moonraker discovery uses a viewport-clamped native dialog with wrapped status/results and pinned Retry/Close actions. Retry uses the latest requested subnet after the preceding worker completes. Start failure has explicit feedback. Deleting its parent cancels result publication and clears widget references.

Camera discovery captures an immutable destination profile and configuration generation. The worker only returns webcam data; current-owner consumption imports it. Results from an old configuration are discarded. Profile editors explicitly identify the edited profile, including an inactive profile. Setup rebases its owner after its own camera import advances the active configuration. Camera test/discovery editors reject obsolete results and offer another test/search. Closing an editor removes its poll owner. The webcam lookup API returns a count without a transport error classification, so an empty result does not prove that the network or camera is absent.

Setup Wi-Fi verifies the selected SSID rather than treating an existing connection to another network as success. After 30 seconds without verification, it stops waiting, retains the password and offers Connect again. Scan again and Skip camera remain available when no camera is returned.

## Telemetry and firmware releases

Telemetry distinguishes no history while offline, waiting for Klipper, and waiting for the first live sample. Existing history-storage warnings, unavailable-channel labels, measured gaps, Hold/Resume and configuration resets remain authoritative. Connection recovery is handled by the existing transport; the UI does not fabricate history or issue a misleading retry request.

Firmware release loading disables duplicate refresh actions; errors show Retry. Idle and empty-channel states explain the available action. Refresh closes a pending release confirmation before replacing its catalog. The appliance-wide catalog worker remains cached and independent of printer selection; closing the browser removes its UI timer and widgets. Firmware download cancellation and explicit installation confirmation remain unchanged.

## Validation

Real-LVGL owner checks cover all five themes and normal/large text. Discovery, Setup, profiles, Files, OTA and Telemetry checks exercise densities and constrained displays. Camera tests include actual page creation, Retry/status bounds, first-frame reception, invalid frames, profile switches, hide/destroy and frame retirement. Typed worker fixtures exercise stale camera imports, inactive-profile imports, empty scans, task failure/retry, configuration changes during camera tests and cancellation through popup deletion. Storage/network/task execution is simulated; these are host checks, not physical network or printer tests.

Host suites: `files_load_worker_test.py`, `camera_pipeline_test.py`, `discovery_states_test.py`, `setup_center_layout_test.py`, `profiles_layout_test.py`, `calibration_dialog_layout_test.py`, `telemetry_modern_layout_test.py`, and `responsive_layout_test.py`. Discovery renders were inspected. Target firmware build, touch/Wi-Fi/camera/OTA behavior and previously documented printing/recovery tests remain pending. Stable version remains 6.5.6; this patch does not commit, merge or push.
