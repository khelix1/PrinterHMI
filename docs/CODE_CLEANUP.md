# Independent stale-code cleanup

This branch ports both accepted cleanup batches from `chore/stale-code-cleanup`
onto main, together with independent Files, Dashboard and Bed Mesh fixes.
It does not merge the cleanup branch's experimental viewer ancestry.

## Retired code

- 59 completed one-time `tools/migration` source rewrite scripts. No live build,
  release or audit path calls them. Stored-data and backup migrations remain.
- Unbuilt `ui_preview.c/.h`, the `ui_printer_motion` forwarding pair (the application
  now directly opens the identical toolhead popup), and the unbound
  `ui_dashboard_status` pair. Current banner, Active Print, machine and camera
  owners remain, including the active Dashboard status popup.
- Unused Network callback adapters, Dashboard startup fallback and leaf APIs in
  widgets, Printer, Network, Files, machine status and popup factories.
- Unconsumed Phase-1 thumbnail state/file/path accessors and two thumbnail-session
  wrappers. Actual worker result flags, serialized download, PNG ownership,
  selected storage, fullscreen and per-profile previews remain.

## Preserved dependencies

Theme implementations use macro-generated dispatch and must not be classified by
literal call counts. All theme files are unchanged from main. ESP-Hosted and PPA
build helpers, runtime NVS/profile migration, backup compatibility, recovery and
historical records remain. The removed motion wrapper passes exactly the same
arguments to the existing toolhead popup. Files row/cache production files match
the accepted v7.1.31/v7.1.32 implementation byte for byte.

`source_membership_test.py` checks 157 firmware C sources with none outside the
build list. `non_viewer_boundary_test.py` checks absent experimental sources/tools
and routing hooks. Its negative fixtures catch both accidental source inclusion
and a viewer action reintroduced into the coordinator.

## Apply and verify

Start `chore/non-viewer-cleanup` from main bce8481, not from a viewer branch.
The updater checks the base revision, changed-file hashes and protected build,
transport, theme and touch inputs before writing; unrelated sdkconfig changes
are preserved. It supports `--check`, backups, guarded recovery and ordinary
write-failure rollback. It performs no commit, push, merge, build or flash.
Use `./tools/build_idf6_hosted3.sh` and verify on the panel before integration.

After merging this branch into main, the viewer branch will need a separate
reconciliation before it can be merged in future; its old commits include older
copies of these shared modules. Preserve the viewer branch as the work checkpoint.
