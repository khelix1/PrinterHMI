# Modernization closeout: 7.0.0-dev

Branch: `feature/profile-setup-dialogs`. Base GitHub checkpoint: `e38a6bd`; later delivered patches remain uncommitted here. GitHub is read-only for this workspace. No version bump, merge or release is part of this closeout.

The planned remaining code batches are complete: Dashboard camera consumer ownership/frame validation and Active Print/Dashboard canvas/render caller lifetimes. The last caller review also retired all preview-coordinator slots before deletion callbacks. These changes preserve the existing operator layout and commands. Visual polish has not started.

## Apply in order

1. `apply_dashboard_camera_safety.py`, after the previously accepted Camera lifecycle handoff patch.
2. `apply_active_print_preview_safety.py`, after accepting the Dashboard camera patch.

For each patch: apply from Downloads, build with `./tools/build_idf6_hosted3.sh`, OTA/flash and verify the affected panel flow before applying the next. Installers check branch/origin/version and exact prerequisite/file hashes, refuse an unexpected baseline without edits, verify applied hashes, and recognize a repeated run. They do not commit or push.

## Host verification

The combined closeout passed:

- `camera_pipeline_test.py`: Camera controls/frame polling/show-hide/setup handoffs and standalone owner compilation.
- `dashboard_camera_safety_test.py`: actual Dashboard camera owner with a compact LVGL hierarchy, frame/control/timer retirement and standalone owner compilation.
- `active_preview_safety_test.py`: actual Active Print/coordinator ownership, native taps, interrupted updates and bounded retained-buffer accounting.
- `aspect_preview_pipeline_test.py`: actual main.c render/apply/restore/worker/timer functions, real Active Print construction, stale staged renders and allocation failure preserving displayed pixels.
- `preview_cache_ownership_test.py`: profile/Files buffers, fullscreen ownership and high-resolution worker boundaries.
- `thumbnail_component_safety_test.py` and `chooser_refresh_test.py`: component/observer/field/action lifetime and unchanged refresh behavior.
- `responsive_layout_test.py`, `studio_pages_test.py`, `live_appearance_test.py`: established geometry, theme/accessibility/density, navigation and appearance regressions.
- `offline_navigation_test.py`: nonblocking worker/connection retirement.

LVGL sources: 9.5.0. Each suite retains its documented theme/density/text/viewport matrix; matrices differ by fixture. SDK, network, persistence and scheduler boundaries are mocked. PNG/JPEG downloads and ESP-IDF firmware compilation were not run in this workspace. Callback injection after real LVGL/render calls tests owner boundaries without deleting a parent inside LVGL's own child-deletion traversal.

## Device acceptance before visual polish

- Dashboard Camera/Thumbnail toggle; live camera fullscreen and exit; leave/reopen both pages.
- Print thumbnail fit on Dashboard/Printer, Files inspector thumbnail/fullscreen and switching to another file while work is pending.
- Intended printer/camera switches; old frame, file or feedback must not replace the new session.
- Camera Configure/Cancel, View/picker Close, unavailable Retry and setup/OTA suspension/resumption when available.
- Repeat navigation with live streaming; observe responsiveness, frame behavior and heap stability. Real transport/decoder/PSRAM and long-duration behavior remain device checks.
- Retain earlier documented printing-dependent recovery/cancellation and connection regressions as hardware acceptance items; host coverage does not establish those physical outcomes.

After device acceptance, commit the accumulated delivered changes and run the existing branch checkpoint/push verification workflow locally. Only integrated `main` produces the nightly firmware; this feature-branch checkpoint must not be described as a release/nightly.

## Deferred optional work

Broader observer migration, measurement-driven profiling/update optimization and PPA/DMA2D acceleration remain optional follow-ups. They do not block visual polish after acceptance and checkpoint. No further mandatory code batch was identified in this scoped closeout review; this is not a guarantee that future testing cannot find defects.
