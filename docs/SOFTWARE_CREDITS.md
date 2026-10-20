# PrinterHMI software credits

## Project authorship and AI-assisted development

PrinterHMI is created and directed by **khelix**. The concept, product direction,
visual design, feature decisions, final approval and hardware verification
remain with khelix. PrinterHMI's own source is distributed under the
[Apache License 2.0](../LICENSE); see [NOTICE](../NOTICE).

OpenAI **ChatGPT/Codex** has been used as a development tool for implementation,
debugging, code review, test development, documentation and exploring
implementation options. AI assistance does not replace khelix's decisions,
hardware testing or release approval. It also does not imply that third-party
software was written by khelix or by AI. OpenAI's services are development tools,
not bundled open-source firmware dependencies. This credit does not claim an
endorsement by OpenAI or any upstream project.

## Firmware and declared dependencies

Thank you to the authors, maintainers and contributors of the following projects.
The inventory below comes from `dependencies.lock`, `main/idf_component.yml`,
local component manifests, `main/CMakeLists.txt` and `sdkconfig` in this branch.
A locked dependency can be optional or build-time-only; this is a declared-stack
inventory, not a claim that every component is linked into every firmware image.
Versions below describe the reviewed lock file; the installed firmware identity
and the lock file used for a release take precedence.

| Component / project | Locked version | Contribution |
| --- | --- | --- |
| `idf` / [ESP-IDF](https://github.com/espressif/esp-idf) | 6.0.2 | Espressif framework, drivers, system services, OTA, SD/MMC and security integration |
| `lvgl/lvgl` / [LVGL](https://github.com/lvgl/lvgl) | 9.5.0 | Interface, widgets, rendering and built-in fonts |
| `espressif/cjson` / [cJSON](https://github.com/DaveGamble/cJSON) | 1.7.19~2 | JSON parsing; cJSON authors and contributors, including Dave Gamble |
| `espressif/esp_hosted` / [ESP-Hosted](https://github.com/espressif/esp-hosted-mcu) | 3.0.5 | Hosted connectivity to the ESP32-C6 |
| `espressif/esp_wifi_remote` | 1.5.3 | Remote Wi-Fi APIs |
| `espressif/esp_websocket_client` | 1.8.0 | Moonraker WebSocket transport |
| `espressif/esp32_p4_function_ev_board` / [ESP-BSP](https://github.com/espressif/esp-bsp) | 4.1.1 | Board-support foundation, locally adapted for the panel |
| `espressif/esp_lvgl_port` | 2.8.0~1 | LVGL/display integration, locally modified |
| `espressif/esp_lcd_jd9165` | 1.0.2 | JD9165 panel driver, locally adapted |
| `espressif/esp_lcd_touch` | 1.2.1 | Touch abstraction |
| `espressif/esp_lcd_touch_gt911` | 1.2.0~3 | GT911 touch controller |
| `espressif/esp_lcd_ek79007` | 1.0.4 | Additional LCD driver declared by the BSP |
| `espressif/esp_lcd_ili9881c` | 1.1.0 | Additional LCD driver declared by the BSP |
| `espressif/esp_codec_dev` | 1.6.2 | Codec services declared by the BSP |
| `espressif/usb` | 1.5.0 | USB support declared by the component stack |
| `espressif/cmake_utilities` | 0.5.3 | Component build utilities |
| `espressif/eppp_link` | 1.1.5 | Declared PPP transport support |
| `espressif/esp_serial_slave_link` | 1.1.2 | Declared serial slave transport support |
| `espressif/wifi_remote_over_eppp` | 0.3.3 | Optional remote Wi-Fi transport |

Espressif components are distributed through the
[ESP Component Registry](https://components.espressif.com/). Their source headers,
component license files and upstream notices remain authoritative. The local
BSP extension in `common_components/bsp_extra` retains its Espressif copyright
and Apache-2.0 notice. The JD9165, ESP-BSP and LVGL-port license files are retained
in their existing component directories. Do not attribute Espressif's underlying
work or other upstream code solely to PrinterHMI.

## SDK libraries and PNG decoding

ESP-IDF supplies third-party libraries as well as Espressif code. Core services
used by this project include **FreeRTOS** (Amazon and contributors), **lwIP**
(the Swedish Institute of Computer Science and contributors), **Mbed TLS**,
**FatFs** (ChaN), and the **Picolibc/Newlib** C runtime and its many contributors.
The reviewed configuration selects Picolibc with Newlib compatibility; the SoC
ROM also includes Newlib facilities. Wi-Fi/networking support also benefits from
projects such as **wpa_supplicant** and **http-parser** as described by ESP-IDF.

The [ESP-IDF 6.0.2 copyright inventory](https://github.com/espressif/esp-idf/blob/v6.0.2/docs/en/COPYRIGHT.rst)
is preserved in [docs/licenses/ESP-IDF-COPYRIGHT.rst](licenses/ESP-IDF-COPYRIGHT.rst).
It credits additional SDK and ROM work, including optional components, and points
to their individual notices. Its component-source references resolve in ESP-IDF;
source headers and exact component notices take precedence over summaries.

**LodePNG**, by **Lode Vandevenne**, supplies PNG decoding through LVGL and is
enabled for previews in the reviewed configuration. The cJSON MIT notice is
preserved in [licenses/cJSON.txt](licenses/cJSON.txt). LodePNG's original notice is
preserved in [licenses/LodePNG.txt](licenses/LodePNG.txt). LVGL's MIT notice is
preserved in [licenses/LVGL.txt](licenses/LVGL.txt).

This reference does not replace the individual licenses or claim a complete
linked-binary license audit. A firmware release should retain the notices for
its exact SDK, toolchain and selected components, including coprocessor firmware.

## Fonts, symbols and logo lettering

| Project / credited authors | Use | Preserved notice |
| --- | --- | --- |
| [Inter](https://github.com/rsms/inter), The Inter Project Authors | Studio typography; converted/subsetted by the project's font generator | [Inter-OFL.txt](licenses/Inter-OFL.txt), also `main/assets/fonts/studio/OFL.txt` |
| [Montserrat](https://github.com/JulietaUla/Montserrat), The Montserrat Project Authors | LVGL built-in text fonts used by the interface and compatibility themes | [Montserrat-OFL.txt](licenses/Montserrat-OFL.txt) |
| [Font Awesome Free](https://fontawesome.com/), Fonticons, Inc. | Symbol glyphs in LVGL's generated Montserrat fonts | [Font-Awesome-Free.txt](licenses/Font-Awesome-Free.txt) |
| [DejaVu Sans](https://dejavu-fonts.github.io/), DejaVu contributors, Bitstream Vera and credited Arev contributors | Lettering in the editable logo SVGs and their exports | [DejaVu.txt](licenses/DejaVu.txt) |

Inter, Montserrat and Font Awesome's font software are supplied under the
SIL Open Font License 1.1. Font Awesome's upstream notice separately describes
licenses for SVG/JS icons and code; that does not make font glyphs CC-BY icons.
The Font Awesome and Montserrat notices above are copied from the LVGL 9.5.0
font-license directory used for the generated built-in fonts. DejaVu's own
Bitstream/Arev notices apply to its lettering. No third-party font authorship
is claimed for PrinterHMI. The interface's custom Studio line icons are drawn
by project code; they are not described as Font Awesome artwork.

## Printer ecosystem: separate software

- [Klipper](https://github.com/Klipper3d/klipper), Kevin O'Connor and contributors:
  printer firmware, commands and status. Klipper runs on the printer/host;
  it is not bundled into this panel firmware.
- [Moonraker](https://github.com/Arksine/moonraker), Eric Callahan and contributors:
  printer API, files, metadata and WebSocket status. It runs on the printer host.
- [PrusaSlicer](https://github.com/prusa3d/PrusaSlicer), Prusa Research,
  Slic3r authors and contributors: G-code and thumbnail/metadata generation
  used in this project's printing workflow; not bundled in the panel.

These acknowledgments describe interoperability and development use, not
ownership of those projects, inclusion of their source, or endorsement.

## Development, testing, artwork and hosting tools

Thanks also to the maintainers and contributors of the **Espressif toolchains**,
**GCC/GNU Binutils/GDB**, **CMake**, **Ninja**, **Python**, **Git**, **Bash** and
other host utilities used by the build and audit scripts. **LVGL** supplies the
host rendering tested by the project's C fixtures. **lv_font_conv** and
**fontTools** generate the Studio font assets. **Inkscape** exports the project
logo artwork. **GitHub** provides source hosting and release/automation services.
These tools and services are credited for their development role; mentioning
them does not imply that they are compiled into PrinterHMI or share its license.

## Keeping credits current

Run `python3 tools/audit/software_credits_test.py` after dependency changes. It
checks that each locked component has a credit and that referenced local notices
exist. Update this document and About when a new runtime library, font, copied
asset or substantial source contribution is introduced. Keep upstream copyright
headers, notices and license files intact, and identify local modifications.
