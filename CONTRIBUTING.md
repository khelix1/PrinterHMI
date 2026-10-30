# Contributing to PrinterHMI

PrinterHMI uses short-lived branches and device-verified commits.

## Start a change

```bash
git switch main
git pull --ff-only
git switch -c <short-feature-name>
```

Keep a branch focused on one behavior or one mechanical cleanup. Do not combine
unrelated UI, transport and configuration changes.

## Before committing

1. Review `git status --short` and `git diff`.
2. Build with `./tools/build_idf6_hosted3.sh` using the pinned ESP-IDF 6.0.2
   environment; see [Building](docs/BUILDING.md). Do not substitute a plain
   `idf.py build` or reuse an IDF 5.x build cache.
3. Install the image by USB or OTA.
4. Exercise the affected UI and its failure path on the target hardware.
5. Confirm there are no new watchdog resets, heap failures or persistent-state
   regressions.
6. Update documentation when behavior, configuration, dependencies or module
   ownership changes.

## Commit and merge

```bash
git add <changed-paths>
git commit -m "Describe the verified outcome"
git switch main
git pull --ff-only origin main
git merge --ff-only <short-feature-name>
./tools/end_of_night_checkpoint.sh
git merge-base --is-ancestor <short-feature-name> main
git branch -d <short-feature-name>
```

The checkpoint publishes a nightly only from clean integrated `main`. For a
stable release, first prepare a final `X.Y.Z` version and release notes, then
use `./tools/release_stable.sh` instead of the nightly checkpoint. Keep the
feature branch until publication succeeds. See [Git workflow](docs/GIT_WORKFLOW.md).

Do not commit build output, local credentials, generated archives, mechanical
backup files or private network endpoints.

## Coding rules

- `main.c` coordinates startup and cross-module routing.
- UI page modules own creation, destruction, refresh and page-local callbacks.
- Controllers own policy; service modules own I/O and persistence.
- LVGL calls must occur in the appropriate task and under the display lock when
  called outside the LVGL event context.
- Prefer PSRAM for large non-DMA buffers; keep DMA-required data internal.
- Treat Moonraker responses and profile changes as generation-scoped data.
- Use shared themes, buttons, cards, popups, page geometry and typography.
- Keep warnings actionable; do not introduce additional warning suppressions.

## Historical records

Files under `docs/history/` are evidence of earlier decisions. Do not update
them to describe current behavior. Correct current documentation instead.
