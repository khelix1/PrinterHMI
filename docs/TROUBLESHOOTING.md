# Troubleshooting

## Collecting evidence

Start the serial monitor from the configured ESP-IDF environment:

```bash
idf.py -p /dev/ttyUSB0 monitor
```

Capture the reset reason, application version, OTA slot/state, PSRAM result,
display initialization, Wi-Fi address, SNTP result, SD mount and Moonraker
connection. Redact credentials, tokens and private addresses before sharing.

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
- Confirm the backlight stays disabled until the first splash frame is ready.
- Look for display reinitialization or page creation outside the LVGL lock.
- Test without SD and with the network unavailable to isolate startup overlap.
- Record whether the flash occurs before display-ready, during Wi-Fi startup or
  when the splash is removed.

Intermittent brief splash-frame flashing remains under investigation.

## Wi-Fi does not connect

- Verify the hosted ESP32-C6 path initializes.
- Re-enter Wi-Fi credentials from the Network page.
- Confirm the access point supports the configured band and authentication.
- Check for DHCP timeout and retry messages.
- Factory reset only when losing all NVS settings is acceptable.

## Moonraker is offline

- Verify the active printer profile name, host and port.
- Test `/server/info` from another device on the same network.
- Confirm Moonraker is listening on the expected interface and port.
- Check local firewall and network isolation.
- Switch away and back to the profile to force endpoint rebinding.

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

- Confirm ESP-IDF 5.4.4 is active.
- Review `dependencies.lock` and the local board-component path.
- Run `idf.py fullclean`, then set the `esp32p4` target and rebuild.
- Do not delete tracked configuration files to solve an unexplained mismatch.

## Factory reset consequences

Factory reset erases NVS and reboots. It does not erase removable SD content.
Use it only after preserving configuration values that must be restored.
