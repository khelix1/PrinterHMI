# Full-stack build

PrinterHMI v6 uses ESP-IDF 6.0.2 for both processors. The complete builder
creates the normal ESP32-P4 application and matching ESP32-C6 ESP-Hosted
firmware.

## Supported toolchain

| Firmware | SDK | Components |
|---|---|---|
| Normal P4 application | ESP-IDF 6.0.2 | ESP-Hosted 3.0.5, ESP Wi-Fi Remote 1.5.3 |
| ESP32-C6 firmware | ESP-IDF 6.0.2 | ESP-Hosted 3.0.5, RPC v2 |

Other ESP-IDF 6.x releases are not claimed compatible. Bootstrap mode installs
the exact tested v6.0.2 tag alongside other SDK installations.

## Fresh-clone build

Install Git, Python 3 and the standard ESP-IDF host prerequisites, then run:

```bash
git clone https://github.com/khelix1/PrinterHMI_v3_2.git
cd PrinterHMI_v3_2
./tools/build_v6_stack.sh --bootstrap
```

Later builds can run without `--bootstrap`:

```bash
./tools/build_v6_stack.sh
```

To select an existing exact SDK checkout:

```bash
PRINTERHMI_IDF6_PATH=/path/to/esp-idf-v6.0.2 \
./tools/build_v6_stack.sh
```

## Outputs

The generated `dist/` package contains:

- normal P4 OTA and full-flash images;
- direct-installation C6 application and full-flash images;
- component images, flashing instructions, a manifest and SHA-256 sums;
- one checksummed `.tar.gz` archive suitable for a GitHub release.
