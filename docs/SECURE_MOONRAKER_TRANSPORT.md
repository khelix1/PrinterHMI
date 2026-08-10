# Secure Moonraker Transport

## Status

Released in PrinterHMI v6.1.0. Secure transport is optional and configured
independently for each printer profile. Standard HTTP/WebSocket remains the
default and keeps API-key behavior unchanged.

## Operator model

Each profile has a security selector beneath its Moonraker port:

- **Standard HTTP** uses `http://` and `ws://` on the configured port.
- **Secure HTTPS/WSS** shows PEM files in the SD-card root. Selecting a CA
  stores it only for that profile, changes its port to 443, and uses verified
  `https://` and `wss://` connections.

TLS and Moonraker API-key authentication are independent. A secure profile
never falls back to HTTP when certificate validation, the TLS proxy, or its CA
fails.

## Ownership and storage

- `moonraker_config_controller` owns the persisted per-profile security mode.
- `moonraker_tls_trust_store` owns per-profile CA PEM persistence in NVS and
  lazily loads the active trust material PSRAM-first (with internal-RAM
  fallback).
- `moonraker_transport_security_controller` owns scheme resolution, matching
  the endpoint to its profile trust material, and downgrade refusal.
- `moonraker`, live HTTP/WebSocket, probe, discovery and inactive-profile
  preview work consume that resolved policy; none creates a separate TLS rule.
- `ui_printer_profiles` owns the operator picker. It filters the SD-card root
  to `.pem` files and does not display or store private keys.

`main.c` only initializes and coordinates these modules.

## Server boundary

PrinterHMI does not modify a printer host. The host must have a TLS endpoint
that proxies Moonraker. The public installer at
`tools/install_moonraker_tls_nginx.sh` configures and verifies a local Nginx
endpoint, or an operator can use an equivalent independently managed proxy.
See [Secure Moonraker Setup](SECURE_MOONRAKER_SETUP.md).

## Validation

For every secure profile, verify HTTPS API, WSS subscription, G-code dispatch,
file transfer, printer switching, certificate rejection, no HTTP downgrade,
restart, OTA and power-cycle behavior. Also verify Standard mode with and
without an API key remains unchanged.
