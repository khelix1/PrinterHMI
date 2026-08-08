# Security Policy

PrinterHMI controls physical equipment. Treat firmware, network configuration
and update infrastructure as operationally sensitive.

## Supported version

Only the current `main` branch and the latest explicitly marked known-good tag
are supported during active development.

## Reporting

Report a suspected vulnerability through GitHub private vulnerability
reporting:

https://github.com/khelix1/PrinterHMI/security/advisories/new

Do not post passwords, API keys, private addresses, firmware URLs or complete
device logs in a public issue.

## Credential policy

- Never store Wi-Fi passwords or Moonraker API keys in tracked source.
- Never commit `.env`, credential headers, private certificates or signed URLs.
- Provision secrets at installation time and store them in NVS or another
  device-local store.
- Redact SSIDs, addresses, tokens and credentials before sharing logs.
- Rotate a credential immediately if it enters Git history; deleting it in a
  later commit does not remove it from earlier commits.

## Network assumptions

The current Moonraker transport uses local-network HTTP and `ws://`. It does
not provide transport encryption. Keep printers and the HMI behind the same
firewall; do not expose either service directly to the public internet.

Where practical, place the print cell on a printer or IoT VLAN. Give the HMI a
DHCP reservation and, if Moonraker `trusted_clients` is used, trust only that
single address with a `/32` entry instead of trusting the whole LAN. Network
segmentation limits who can observe commands or control a printer; it does not
encrypt Moonraker traffic.

Stable and nightly GitHub release downloads use HTTPS with server certificate
verification. The custom OTA editor also permits HTTP for a trusted local
development server and displays that limitation in the UI. Never install an
image from an untrusted location or across an untrusted network.

## Device-local credentials

Wi-Fi credentials are provisioned on the device and stored in ordinary NVS in
the current development configuration. Temporary plaintext copies used by the
Wi-Fi connection screen are cleared after the connection request. The Wi-Fi
driver and NVS retain the copies required for reconnecting.

Configuration backups intentionally exclude Wi-Fi passwords and Moonraker API
keys. Physical extraction resistance is not claimed while flash encryption is
disabled. Secure Boot, flash encryption and eFuse changes require a separate
production provisioning design and testing on spare hardware; they are not
part of routine firmware updates.

## Release security gate

A release must not be published until:

- tracked source and reachable Git history are free of live credentials;
- affected credentials have been rotated;
- the OTA source and image provenance have been verified;
- dependencies and component licenses have been reviewed;
- factory-reset behavior and retained SD-card data are documented;
- the known-good tag points to the device-tested commit.
