# Building PrinterHMI

## Known-good environment

| Item | Version or setting |
| --- | --- |
| ESP-IDF | 5.4.4 |
| Target | `esp32p4` |
| Compiler architecture | RISC-V |
| Flash size | 16 MiB |
| LVGL | 9.2.2 |
| Color depth | RGB565 / 16-bit |
| FreeRTOS tick | 1000 Hz |
| Main task stack | 8192 bytes |
| LCD framebuffers | 2 |
| LVGL display mode | Direct mode with avoid-tearing enabled |

`dependencies.lock` is committed and is the authoritative resolved dependency
set. `main/idf_component.yml` declares direct component constraints.

## Prerequisites

- Linux development host
- ESP-IDF 5.4.4 installed through the supported Espressif installer
- USB access to the target's serial/download port
- Git
- Python environment supplied by ESP-IDF

## Clean checkout build

```bash
source "$IDF_PATH/export.sh"
cd PrinterHMI_v3_2
idf.py set-target esp32p4
idf.py build
```

Expected application output:

```text
build/PrinterHMI.bin
```

The first build resolves managed components. Do not commit
`managed_components/` or `build/`.

## Incremental build

```bash
source "$IDF_PATH/export.sh"
cd PrinterHMI_v3_2
idf.py build
```

## Fully clean rebuild

Use ESP-IDF's supported clean action:

```bash
idf.py fullclean
idf.py set-target esp32p4
idf.py build
```

`fullclean` deletes generated build output. It does not modify tracked source.

## Configuration policy

- `sdkconfig.defaults` contains settings that must survive a fresh target
  configuration.
- `sdkconfig` records the known-good resolved configuration and should change
  only when an intentional configuration change is reviewed.
- `partitions.csv` is the only supported partition layout for v4.1.0.
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
and bootloader-size regressions as release blockers.

## Non-portable lockfile note

The current lockfile records the local board component source path. A future
cleanup should make that component reference repository-relative before builds
are expected to work from an arbitrary checkout path.
