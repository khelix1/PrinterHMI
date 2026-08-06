# Flashing, OTA and recovery

## USB installation

Build and flash through ESP-IDF:

```bash
cd PrinterHMI_v3_2
./tools/build_idf6_hosted3.sh

source "$HOME/esp/esp-idf-v6.0.2/export.sh"
idf.py -B build-idf6-hosted3 \
    -D SDKCONFIG="$PWD/sdkconfig.idf6" \
    -p /dev/ttyUSB0 flash monitor
```

Replace `/dev/ttyUSB0` with the detected serial port. A full USB flash writes
the bootloader, partition table, OTA metadata and application image according
to ESP-IDF's generated flash arguments.

## Matching P4 and C6 firmware

PrinterHMI v6.0.2 requires ESP-Hosted 3.0.5 on both the ESP32-P4 host and
ESP32-C6 co-processor. The complete release archive contains normal P4 OTA
and full-flash images plus the matching C6 full-flash image.

For a normal update where the C6 already runs 3.0.5, install only the P4 OTA
image. For initial installation or C6 recovery, flash the C6 3.0.5 full image
directly and then flash the normal P4 image. Confirm the startup log reports
host 3.0.5, co-processor 3.0.5 and RPC v2.

Both processors are built with the exact ESP-IDF 6.0.2 tag.

## OTA update

1. Build `build-idf6-hosted3/PrinterHMI.bin` from a clean, reviewed commit.
2. Record its SHA-256 checksum.
3. Publish it on a server reachable by the HMI.
4. Open Settings and enter the exact firmware URL.
5. Start the update and leave power connected.
6. To stop it, select `CANCEL` and wait for acknowledgement. A cancelled partial
   image is not selected for boot.
7. Otherwise, wait for completion and reboot.
8. Confirm the firmware version, active OTA slot and core features.

The OTA URL is persisted in NVS. The current manager accepts HTTP or HTTPS
URLs, but server-certificate policy and image signing are not yet a complete
production trust chain. Use a controlled network and server.

## Rollback behavior

The project enables bootloader application rollback. A newly installed image
may boot as `ESP_OTA_IMG_PENDING_VERIFY`. Startup calls
`esp_ota_mark_app_valid_cancel_rollback()` after core initialization reaches
the application validation path.

Do not remove power during the first boot of a new image. Verify more than a
single warm reboot: perform one controlled reboot and one full power cycle.

## Recovery

Use USB flashing when:

- the splash never clears and the device does not reach the operator UI;
- the Wi-Fi or OTA URL configuration is unusable;
- both OTA selection and application state are uncertain;
- the partition table changed;
- repeated watchdog resets prevent normal operation.

Recovery procedure:

```bash
cd PrinterHMI_v3_2
./tools/build_idf6_hosted3.sh

source "$HOME/esp/esp-idf-v6.0.2/export.sh"
idf.py -B build-idf6-hosted3 \
    -D SDKCONFIG="$PWD/sdkconfig.idf6" \
    -p /dev/ttyUSB0 erase-flash

idf.py -B build-idf6-hosted3 \
    -D SDKCONFIG="$PWD/sdkconfig.idf6" \
    -p /dev/ttyUSB0 flash monitor
```

`erase-flash` destroys all flash-resident settings and OTA state. Use it only
when that loss is intended. It does not erase the removable SD card.

## Nightly firmware

A clean, tested `main` checkpoint publishes a GitHub prerelease tagged
`nightly-YYYY-MM-DD-<commit>`. It contains `PrinterHMI.bin` and its SHA-256
checksum. `tools/end_of_night_checkpoint.sh` builds, audits, pushes, tags,
publishes and verifies these assets.

Nightly builds are development checkpoints, not versioned releases.

## Release artifacts

For each release retain:

- Git commit and annotated tag
- `PrinterHMI-vX.Y.Z-ota.bin`
- SHA-256 checksum
- complete P4+C6 IDF6 stack archive and checksum
- build date and toolchain version
- partition table
- test record
- recovery instructions
