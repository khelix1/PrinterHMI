# Hardware

## Supported target

PrinterHMI v6.0.1 targets the JC1060P470C-I/W operator panel built around the
ESP32-P4.

| Function | Hardware |
| --- | --- |
| Application processor | ESP32-P4, dual-core RISC-V |
| Network coprocessor | ESP32-C6 through Espressif hosted Wi-Fi |
| Display | 7-inch 1024 x 600 JD9165 MIPI-DSI panel |
| Touch | GT911 capacitive touch controller |
| Flash | 16 MiB |
| External RAM | Hex PSRAM at 200 MHz |
| Removable storage | 4-bit SDMMC |

## Display and touch

- Resolution: 1024 x 600
- MIPI-DSI lanes: 2
- Lane bitrate: 550 Mbit/s per configured lane
- Pixel format: RGB565
- Display framebuffers: 2
- LVGL mode: direct mode with avoid-tearing enabled
- Backlight: GPIO 23
- LCD reset: GPIO 27
- Touch I2C SDA: GPIO 7
- Touch I2C SCL: GPIO 8
- GT911 coordinate range: 1024 x 600

The display and touch path is supplied by the local
`espressif__esp32_p4_function_ev_board` component plus `bsp_extra`. The board
component contains alternate panel definitions; PrinterHMI's active build is
the 1024 x 600 JD9165 path.

ESP-Hosted resets and initializes the C6 while the backlight is off. Display
startup follows, preventing repeated C6-stage flashes. One brief initial panel
appearance may remain when the splash first becomes visible.

## SDMMC pin map

| Signal | GPIO |
| --- | ---: |
| CLK | 43 |
| CMD | 44 |
| D0 | 39 |
| D1 | 40 |
| D2 | 41 |
| D3 | 42 |

The card is mounted at `/sdcard`. Thumbnail cache files are stored beneath
`/sdcard/cache`, and per-printer preview files beneath
`/sdcard/hmi/profile_previews`.

## Memory policy

PSRAM is enabled for instruction/rodata placement and general allocation.
PrinterHMI explicitly prefers PSRAM for large Moonraker and WebSocket buffers,
thumbnail data and rendered previews. Console history, macro catalogs, the
Macros page context and persistent shell pointer tables are also PSRAM-first.
Their allocations occur after scheduler startup. Display DMA buffers remain
internal and DMA-capable as required by the BSP.

## Flash partitions

| Name | Type | Offset | Size | Purpose |
| --- | --- | ---: | ---: | --- |
| `nvs` | data/nvs | `0x9000` | 24 KiB | Persistent settings |
| `otadata` | data/ota | `0xf000` | 8 KiB | OTA selection state |
| `phy_init` | data/phy | `0x11000` | 4 KiB | PHY initialization |
| `ota_0` | app/ota_0 | `0x20000` | 7 MiB | Application slot A |
| `ota_1` | app/ota_1 | automatic | 7 MiB | Application slot B |
| `storage` | data/spiffs | automatic | 1920 KiB | Internal data volume |

The partition table is defined by `partitions.csv`. Never flash an image built
for a different partition layout without a full USB recovery plan.

## Power and recovery

Use a stable supply capable of powering the display, ESP32-P4, ESP32-C6 and SD
card simultaneously. During first boot after OTA, keep power applied until the
new image has initialized and marked itself valid.
