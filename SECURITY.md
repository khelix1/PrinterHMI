# Security Policy

PrinterHMI controls physical equipment. Treat firmware, network configuration
and update infrastructure as operationally sensitive.

## Supported version

Only the current `main` branch and the latest explicitly marked known-good tag
are supported during active development.

## Reporting

Report a suspected vulnerability through GitHub private vulnerability
reporting:

https://github.com/khelix1/PrinterHMI_v3_2/security/advisories/new

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

The current development transport uses local-network HTTP and WebSocket
connections to Moonraker. It does not provide transport encryption. Operate it
only on a trusted, segmented network until authenticated TLS is implemented.

The OTA manager accepts a configurable URL and currently permits HTTP. An
operator must control the update server and network path. Do not install an
image from an untrusted location.

## Release security gate

A release must not be published until:

- tracked source and reachable Git history are free of live credentials;
- affected credentials have been rotated;
- the OTA source and image provenance have been verified;
- dependencies and component licenses have been reviewed;
- factory-reset behavior and retained SD-card data are documented;
- the known-good tag points to the device-tested commit.
