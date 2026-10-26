# Source cleanup

The cleanup branch starts at the pushed v7.1.29-dev checkpoint `0dc6e8a1ba77`.
Work is grouped by ownership and reference evidence, with host checks after each
batch. Old names, low reference counts and exclusion from the firmware build are
candidate signals, not proof that code can be removed. Macro dispatch, callbacks,
weak platform hooks, host tests, recovery tools and stored-data compatibility all
need separate checks.

## Batch 1 — v7.1.30-dev

| Change | Evidence and result |
| --- | --- |
| Delete 59 `tools/migration/*.py` scripts | Completed, one-time source rewrites for earlier development phases. No current source, audit, build or release references. Their exact versions remain in Git history at the checkpoint. |
| Delete `main/ui_preview.c/.h` | Unbuilt prototype with no consumers. Current preview owners are `ui_thumbnail`, page modules, caches and `ui_preview_lightbox`. |
| Delete `main/ui_printer_motion.c/.h` | Thin forwarding compatibility module. The sole firmware show caller and the Printer host fixture now call `ui_printer_toolhead_show` directly with the same arguments. Toolhead initialization and command formatting already use the real implementation. |
| Remove ignored Network creation parameters | The information-card and port-editor callbacks were accepted but discarded. Their application adapters and obsolete typedef are removed; scan and profile-management callbacks remain. |
| Remove Dashboard startup fallback | `ui_dashboard_create_status` had no callers. Actual Dashboard construction continues through the existing theme-aware page. |
| Remove eight unused helper exports | `ui_create_card_title`, `ui_create_card_value`, `ui_operator_banner_set_status`, `ui_printer_refresh`, `ui_network_refresh`, `ui_printer_preview_canvas_file_size`, `ui_dashboard_thumb_forget_canvas_file`, `ui_files_add_file_button`. No code, audit or macro consumers. |
| Add source membership audit | Every firmware source must exist and be listed once. Additional `main/*.c` files require a classified host consumer; missing project UI headers are detected. Negative fixtures cover missing, duplicate and unclassified sources. |

This retires 63 files containing 15,689 lines, plus 12 unused bridge/leaf
functions and five motion-forwarding APIs. Current theme implementations and
all 3D/Files loading fixes are retained byte for byte. No runtime behavior change
or meaningful firmware speed/size gain is claimed: most removed code was already
unbuilt or eligible for link-time removal.

## Batch 2 — v7.1.32-dev

| Change | Evidence and result |
| --- | --- |
| Retire `ui_dashboard_status.c/.h` | Its constructor had no callers. `ui_dashboard_page` explicitly left its old status fields empty in every layout, and the separate legacy owner was never bound. Remove the empty page fields, unused copied handles and controller calls that formatted values for those empty handles. Current banner, Active Print, machine and camera owners remain. |
| Retire the Phase-1 thumbnail state/path prototype | `thumbnail_state_t`, the old file/status strings, `/sdcard/cache` path builder and their public accessors had no consumers. The real worker/result flags, PNG ownership, selected-cache I/O and download entry points remain. Two internal free/set wrappers are inlined into the live PNG clear/take operations. |
| Remove 14 leaf APIs | Ten unconsumed popup factories (title/message/status/progress/close/form grid/cell), the old fixed-layout machine-card constructor, unused Printer layout-reference clearing and two unused thumbnail-session install/store wrappers. No macro, firmware, platform or test consumer calls these APIs. Shared footer/keyboard/owner helpers still have host consumers and remain. |
| Adjust regression ownership | The telemetry fixture no longer constructs the private legacy status owner that firmware never used. Actual Dashboard banner/machine/offline/camera behavior is checked by production-owner native fixtures; repeated Printer updates, charts and telemetry history retain their existing tests. A new actual-thumbnail-manager test covers the ownership/worker paths changed here. |

This batch retires 35 public APIs, deletes two legacy source/header files and
removes approximately 870 production lines. The Files row/cache modules are
byte-for-byte the verified v31 implementation. Runtime data migration, theme
macro dispatch, network/security helpers and build patches are preserved.
No firmware performance or size improvement is asserted.

Native checks cover current Dashboard status/percentages across themes/text sizes,
stale/offline/reconnect behavior, camera ownership, popup footer/keyboard/modal
safety, telemetry/charts, full/aspect preview paths, Files cache reuse and the 3D
viewer. `thumbnail_manager_test.py` runs the actual manager under ASan/UBSan for
PNG replacement/freeing, raw-descriptor publication, result transitions, task
single-flight, retries, and allocation/task-start failure cleanup. Source membership
now classifies 164 firmware C sources plus the two retained host experiments.

## Explicitly retained

- Theme implementation wrappers: calls are generated by `DISPATCH_VOID` and
  `THEME_C_DELEGATE_VOID`. Full spelling searches alone cannot classify these as unused.
- `mesh_preview_loader` and `ui_mesh_preview`: excluded from firmware but still
  consumed by host loader/UI checks. The mesh renderer, converter and fixtures
  also support automatic G-code viewer regression tests.
- ESP-IDF/ESP-Hosted patches, PPA alignment/cache helpers and `prepare_ppa_trial`:
  the current `tools/build_idf6_hosted3.sh` invokes them. The old-looking PPA name
  is not a reason to delete the build dependency.
- Runtime NVS/profile migration, legacy backup import and security compatibility
  behavior. Source rewrite scripts are different from stored-data migrations.
- Historical release/architecture records, asset licenses, firmware recovery,
  release/checkpoint scripts and host benchmarks.
- The parked Theme Maker branch and its tooling are not part of these removals.

Remaining low-reference exports need a deeper callback/test/ABI review before a
later batch. The parked standalone mesh UI can be reorganized separately; its
presence here is classified rather than mistaken for firmware functionality.

## Validation and target limit

Source membership, real-LVGL theme persistence/chooser, all-theme responsive page
layouts, Printer toolhead entry/lifecycle and G-code viewer gesture/fallback tests
are run. PPA build-helper tests verify default rendering, software fallback and
build/flash/monitor configuration selection. The public-tree and version audits
also run. The full ESP-IDF firmware build is unavailable in this workspace;
`./tools/build_idf6_hosted3.sh` and a short panel smoke test remain required.

## Applying and recovering

The updater requires `chore/stale-code-cleanup` and checks every target's expected
v7.1.29-dev bytes before writing or deleting anything. It supports `--check`,
backs up replaced/deleted files outside the repository, preserves file modes,
rejects drift/symlinked target paths, and rolls back ordinary write failures.
Repeated application is harmless. Unrelated files and user configuration are
not rewritten. No commit, push, build or flash is performed by the updater.
The backup includes a guarded `restore_cleanup.py` recovery command.
