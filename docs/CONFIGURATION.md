# Configuration and persistent state

## Operator configuration

The Settings and Network pages provide runtime configuration for:

- Wi-Fi credentials
- Up to four named Moonraker printer profiles
- Active printer
- OTA firmware URL
- Display brightness and sleep timeout
- Timezone
- Theme, accent and density
- Large text, high contrast, reduced transparency and reduced motion

## Defaults

| Setting | Default |
| --- | --- |
| Moonraker profile count | One configured profile |
| Moonraker profile name | `Printer 1` |
| Moonraker port | `7125` |
| Theme | Operator |
| Accent | Theme default |
| Density | Comfortable |
| Accessibility options | Off |
| Brightness | 100% |
| Display sleep | Disabled |
| Timezone | Central Time (US/Canada) |

Tracked source does not contain Wi-Fi passwords or Moonraker API keys.
Credentials are provisioned on the device. See `SECURITY.md` for storage and
network assumptions.

## NVS schema

| Namespace | Keys | Owner |
| --- | --- | --- |
| `netcfg` | `ssid`, `pass` | Wi-Fi configuration |
| `netcfg` | `mp_schema`, `mp_active`, `pN_used`, `pN_name`, `pN_host`, `pN_port` | Multi-printer profiles |
| `display` | `brightness`, `sleep_min` | Display settings |
| `ui_theme` | `active`, `custom_id`, `accent`, `density`, `large_text`, `contrast`, `solid_glass`, `reduce_motion` | Appearance manager |

Custom `.phmitheme` packages are discovered from
`/sdcard/PrinterHMI/themes/`. See [Custom themes](CUSTOM_THEMES.md) for the
package contract, validation limits and fallback behavior.
| `time_cfg` | `zone_id` | Timezone configuration |
| `ota` | `url` | OTA manager |
| `hmi` | `last_file` | Thumbnail/session restore |

Legacy `netcfg/moon_host` and `netcfg/moon_port` values are migrated into
profile zero when the multi-printer schema is first loaded.

## Time synchronization

The timezone is applied before SNTP starts. After Wi-Fi obtains an address,
the device polls `pool.ntp.org` and `time.google.com`. The shell renders local
time using the selected POSIX timezone rule.

Supported timezone presets are UTC, Atlantic, Eastern, Central, Mountain,
Arizona, Pacific, Alaska, Hawaii, United Kingdom, Central Europe, India, China,
Japan, Australia Eastern and New Zealand.

## Moonraker connectivity

The active profile supplies host and port to HTTP and WebSocket transports.
The normal Moonraker port is 7125. Profile selection increments a generation
counter so queued results from the previous endpoint can be rejected.

Moonraker discovery is part of Manage Printers. Open Add or Edit, select
`DISCOVER`, then choose an endpoint to populate host and port. Discovery does
not save automatically; select `SAVE` after reviewing the profile.

Current API paths include server information, file listing, file metadata,
thumbnail download, object subscription, G-code script execution and print
start. Connections currently use local-network HTTP and `ws://`.

Use a DHCP reservation for the HMI. If Moonraker `trusted_clients` is required,
prefer the HMI's single address as a `/32` entry instead of trusting the entire
LAN. Keep the print cell behind a firewall and use a printer or IoT VLAN where
practical. See `SECURITY.md` for the complete network assumptions.

Active-print layers normally come from `print_stats.info`. When slicer layer
statistics are absent, PrinterHMI estimates layers from file `object_height`,
`layer_height` and print progress metadata.

## Bed Mesh profiles and detected macros

Bed Mesh profiles are owned by Klipper, not PrinterHMI NVS. The Bed Mesh page
reads the active profile list, can request calibration, and uses Klipper
`SAVE_CONFIG` semantics when a profile change must persist.

The Macros page is generated from Moonraker `printer.objects.list`. Public
`gcode_macro` objects are displayed alphabetically; names beginning with an
underscore are treated as internal helpers and hidden. Macro discovery and
history are runtime state and are not persisted in NVS.

## Factory reset

Factory reset erases NVS and reboots. This removes saved Wi-Fi, Moonraker,
appearance, display, timezone, OTA and last-file settings. It does not erase
SD-card caches or files.

## Changing persistent schemas

Any NVS schema change must include:

1. A schema version or compatible fallback.
2. Migration behavior for existing devices.
3. A fresh-device test.
4. An upgrade test from the preceding known-good release.
5. Updated factory-reset and recovery documentation.
