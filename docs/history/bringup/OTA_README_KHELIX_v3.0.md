# Drybox HMI v3.0 ESP32-P4 Dashboard Baseline

This package is intentionally **network-disabled** for the first stable v3.0 bring-up.

Why: ESP32-P4 has no native WiFi radio. The JC1060P470 board uses an ESP32-C6 coprocessor path for networking, so normal `esp_wifi` station-mode code does not apply directly on the P4.

What works here:

- Vendor MIPI-DSI JD9165 display baseline
- GT911 touch baseline through the vendor BSP
- LVGL 9 dashboard skeleton
- OTA-capable partition table with `ota_0` and `ota_1` slots

Use USB flashing for this baseline:

```bash
source /home/khelix/.espressif/v5.4.4/esp-idf/export.sh
cd ~/P4/JC1060P470_DryboxHMI_v3_0_Dashboard_Fixed3
rm -rf build
idf.py set-target esp32p4
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

Next stage: bring up the ESP32-C6 network bridge, then enable pull OTA through that network path.
