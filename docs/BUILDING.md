# Building PrinterHMI

## Known-good environment

| Item | Version or setting |
| --- | --- |
| ESP-IDF | 6.0.2 |
| Target | `esp32p4` |
| Compiler architecture | RISC-V |
| Flash size | 16 MiB |
| LVGL | 9.5.0 |
| Color depth | RGB565 / 16-bit |
| FreeRTOS tick | 1000 Hz |
| Main task stack | 8192 bytes |
| LCD framebuffers | 2 |
| LVGL display mode | Direct mode with avoid-tearing enabled |

`dependencies.lock` is committed and is the authoritative resolved dependency
set. `main/idf_component.yml` declares direct component constraints.

## Prerequisites

- Linux development host
- ESP-IDF 6.0.2 installed through the supported Espressif installer
- USB access to the target's serial/download port
- Git
- Python environment supplied by ESP-IDF

## Clean checkout build

```bash
cd PrinterHMI_v3_2
./tools/build_idf6_hosted3.sh
```

Expected application output:

```text
build-idf6-hosted3/PrinterHMI.bin
```

The first build resolves managed components and applies the two guarded
v6 compatibility patches. Do not commit `managed_components/` or generated
build directories.

## Incremental build

```bash
cd PrinterHMI_v3_2
./tools/build_idf6_hosted3.sh
```

## Fully clean rebuild

Use ESP-IDF's supported clean action:

```bash
source "$HOME/esp/esp-idf-v6.0.2/export.sh"
idf.py -B build-idf6-hosted3 fullclean
./tools/build_idf6_hosted3.sh
```

`fullclean` deletes generated build output. It does not modify tracked source.

## Version policy

`version.txt` is the single build-version source. CMake reads it into
`PROJECT_VER`, ESP-IDF stores that value in the application descriptor, and
runtime UI surfaces read the descriptor with `esp_app_get_description()`.
Current documentation is checked by `tools/audit/version_audit.sh`.

## Configuration policy

- `sdkconfig.defaults` contains settings that must survive a fresh target
  configuration.
- `sdkconfig` records the known-good resolved configuration and should change
  only when an intentional configuration change is reviewed.
- `partitions.csv` is the only supported partition layout for v7.2.4-dev.
- Do not place credentials in either configuration file.

## Dependency policy

When changing `main/idf_component.yml`:

1. Run a clean component resolution and build.
2. Review the `dependencies.lock` diff.
3. Confirm the board, display, touch and hosted-Wi-Fi components remain on
   compatible versions.
4. Perform the full target test matrix before committing the lockfile.

## Build diagnostics

After a successful canonical build, inspect that configured build directory:

```bash
source "$HOME/esp/esp-idf-v6.0.2/export.sh"
idf.py -B build-idf6-hosted3 size size-components size-files
```

Treat new compiler warnings, partition overflows, internal-memory regressions
and bootloader-size regressions as release blockers. For v5 and later, compare
the startup `.bss`/internal-RAM map and confirm the image passes
`vApplicationGetTimerTaskMemory()` before accepting a build.

## Portable dependency paths

The local board component uses repository-relative paths in the manifest and
lockfile. `tools/audit/dependencies_lock_audit.sh` rejects private absolute paths.
Builds and project-root archives must work from another checkout directory; do
not reintroduce a machine-specific board-component path.

## Development and release versions

`version.txt` owns the current build identity: `7.2.4-dev`. Local `X.Y.Z-dev` builds
are Development; unlabelled final-version local builds are Custom. CMake accepts `X.Y.Z`, `X.Y.Z-dev` and generated nightly identities.
Stable release tooling reads the version source and rejects development suffixes
before build or network publication. Nightly tooling restores the exact original
version after its temporary nightly build. Release preparation updates the version and release notes together. Use
`./tools/release_stable.sh` from a clean, integrated `main` checkout to build and
publish the Stable channel. Known device
validation limits are retained in the release notes.

### Publishing a stable release

Prepare a final `X.Y.Z` in `version.txt`, current release notes and matching
identity documentation. Build and panel-check with
`PRINTERHMI_BUILD_CHANNEL=stable ./tools/build_idf6_hosted3.sh`. Commit the
reviewed result on its feature/release branch. Integrate into up-to-date `main`
with `git merge --ff-only`, then run `./tools/release_stable.sh` from clean main.
The script builds and audits the complete P4+C6 stack, atomically pushes main
and the version tag, and publishes OTA/full-stack assets and checksums. Skip
nightly checkpointing for this publication. Keep the branch until publication
succeeds. Published v7.1.0 and v7.2.0 preparation steps are history, not commands
to rerun for the next release.

### OTA channel identity

Local `-dev` versions embed channel Development (3). Existing IDs for Custom (0),
Stable (1) and Nightly (2) are retained. A stable build still requires a final
`X.Y.Z` version and explicit release tooling. No NVS migration is needed.

OTA compares numeric versions only within Stable and dates only within Nightly.
Moving from Development or legacy Custom-labelled dev images to a stable release
is a channel switch, even when development patch numbers are numerically larger.
The operator sees installed and selected identities and release notes. This does
not bypass target bootloader anti-rollback checks or firmware image validation.
