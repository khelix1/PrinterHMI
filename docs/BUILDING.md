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

The first build resolves managed components. Do not commit
`managed_components/` or `build/`.

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
- `partitions.csv` is the only supported partition layout for v6.0.0.
- Do not place credentials in either configuration file.

## Dependency policy

When changing `main/idf_component.yml`:

1. Run a clean component resolution and build.
2. Review the `dependencies.lock` diff.
3. Confirm the board, display, touch and hosted-Wi-Fi components remain on
   compatible versions.
4. Perform the full target test matrix before committing the lockfile.

## Build diagnostics

Useful commands:

```bash
idf.py size
idf.py size-components
idf.py size-files
```

Treat new compiler warnings, partition overflows, internal-memory regressions
and bootloader-size regressions as release blockers. For v5 and later, compare
the startup `.bss`/internal-RAM map and confirm the image passes
`vApplicationGetTimerTaskMemory()` before accepting a build.

## Non-portable lockfile note

The current lockfile records the local board component source path. A future
cleanup should make that component reference repository-relative before builds
are expected to work from an arbitrary checkout path.
