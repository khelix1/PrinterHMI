# Troubleshooting

## Collecting evidence

Start the serial monitor from the configured ESP-IDF environment:

```bash
idf.py -p /dev/ttyUSB0 monitor
```

Capture the reset reason, application version, OTA slot/state, PSRAM result,
display initialization, Wi-Fi address, SNTP result, SD mount and Moonraker
connection. Redact credentials, tokens and private addresses before sharing.

## Boot asserts before `app_main()`

If the log reports `vApplicationGetTimerTaskMemory` with
`pxTCBBufferTemp != NULL`, the image exhausted or fragmented internal startup
RAM before the FreeRTOS timer task was created. This is not a PSRAM-capacity
failure because it occurs before `app_main()` can allocate runtime contexts.

Compare the new and known-good linker maps and `heap_init` ranges. Move
long-lived non-DMA UI/controller state into bounded permanent PSRAM
allocations performed after scheduler startup. Do not disable FreeRTOS static
allocation; ESP-Hosted depends on static task creation.

## Device remains on splash

1. Wait long enough to distinguish slow network startup from a frozen LVGL
   task.
2. Check for watchdog output and the last startup-stage log.
3. Confirm all cross-task LVGL operations use the BSP display lock.
4. Test a warm reboot and a full power cycle separately.
5. If the image is an OTA first boot, inspect its OTA state.
6. Recover by USB if the UI never becomes operational.

## White or flashing display during startup

- Confirm the image uses the JD9165 1024 x 600 board path.
- Confirm ESP-Hosted initialization logs before BSP display startup.
- Confirm the backlight remains low while the C6 transport resets.
- Confirm two framebuffers, direct mode and avoid-tearing are enabled.
- Look for display reinitialization or LVGL access outside the display lock.
- Distinguish the accepted initial panel appearance from repeated flashing.

Repeated Wi-Fi-stage flashing means C6 initialization has moved behind visible
display startup or another path is disturbing the display/backlight.

## Wi-Fi does not connect

- Verify the hosted ESP32-C6 path initializes.
- Re-enter Wi-Fi credentials from the Network page.
- Confirm the access point supports the configured band and authentication.
- Check for DHCP timeout and retry messages.
- Factory reset only when losing all NVS settings is acceptable.

## Moonraker is offline

- Verify the active printer profile name, host and port.
- For Secure mode, verify port 443, the certificate IP/DNS name and the CA PEM selected for that profile; see `SECURE_MOONRAKER_SETUP.md`.
- Test `/server/info` from another device on the same network.
- Confirm Moonraker is listening on the expected interface and port.
- Check local firewall and network isolation.
- Switch away and back to the profile to force endpoint rebinding.
- The interface should remain responsive while an endpoint is unavailable;
  waiting for an unreachable endpoint is not expected to freeze touch input
  or navigation.

## Camera is blank, stale, or discovery fails

- Confirm the active printer profile is the intended one, then use
  **Camera → Configure → FIND CAMERAS**.
- Verify the printer's Moonraker `/server/webcams/list` response includes an
  enabled webcam with a usable `stream_url`.
- For Secure profiles, verify the profile's CA trust and HTTPS port first;
  webcam discovery follows that same policy and never downgrades it.
- A blank camera view for an unconfigured profile is expected. A previous
  printer's frame must not remain visible after switching profiles.
- Capture the `camera_find` task log if discovery resets or fails.

## A new print shows old progress or layer values

A new job should begin at 0% and show unknown layer values until fresh
Moonraker status arrives. If it instead inherits a prior job's percentage or
layer number, capture the serial log and Moonraker `virtual_sdcard`,
`print_stats` and `gcode_move` status for diagnosis.

## Clock is wrong

- Confirm Wi-Fi obtained an address and SNTP reported synchronized time.
- Select the correct timezone in Settings.
- Reopen the page or allow the shell clock timer to refresh.
- If UTC is correct but local time is not, inspect the persisted `zone_id` and
  POSIX timezone rule.

## SD or preview failure

- Confirm SDMMC mount succeeds on the documented pins.
- Check card format and write access.
- Verify `/sdcard/cache` and `/sdcard/hmi/profile_previews` can be created.
- Treat a missing thumbnail as non-fatal; file controls should remain usable.
- Remove only the affected cache entry when diagnosing stale preview data.

## OTA failure

- Verify the URL is reachable from the HMI network.
- Confirm the object is a current `PrinterHMI.bin`, not a project archive.
- Check content length, timeout and free OTA partition space.
- Use USB flashing after repeated OTA failure or uncertain partition state.

## Build fails after dependency resolution

- Confirm `~/esp/esp-idf-v6.0.2` is the exact v6.0.2 Git checkout.
- Run `./tools/build_idf6_hosted3.sh`; do not substitute a plain
  `idf.py build`.
- Confirm `sdkconfig.idf6` and `build-idf6-hosted3` are selected.
- Confirm both guarded compatibility scripts report `PASS`.
- Review `dependencies.lock` and the local board-component paths.
- Do not delete tracked configuration files to solve an unexplained mismatch.

## Factory reset consequences

Factory reset erases NVS and reboots. It does not erase removable SD content.
Use it only after preserving configuration values that must be restored.
