# Secure Moonraker Transport

## Status

Planned, opt-in per printer profile. This document defines the future HTTPS/WSS
capability; it does not change current HTTP, WebSocket, or API-key behavior.

## Operator model

Each printer profile will offer **Connection security**:

* **Standard (HTTP)** — current behavior, with or without a Moonraker API key.
* **Secure (HTTPS/WSS)** — encrypted, certificate-verified traffic, also with
  or without a Moonraker API key.

TLS and Moonraker API-key authentication are independent. A secure profile
must never silently fall back to HTTP after certificate validation fails.

## Boundaries

* `moonraker_config_controller` owns persisted per-profile endpoint, security
  mode, and trust-anchor reference only; it does not perform TLS I/O.
* `moonraker_transport_security_controller` owns security policy, endpoint
  scheme selection, certificate-validation outcomes, and downgrade refusal.
* `moonraker_tls_trust_store` owns PSRAM-first loaded CA material and NVS/SD
  trust-anchor persistence. It exposes validated PEM data to transports and
  never exposes private keys.
* `moonraker_live_transport`, `moonraker_live_websocket`, probe, discovery,
  preview and file services consume the resolved secure endpoint; they do not
  implement their own TLS policy.
* `ui_printer_profiles` owns operator selection of Standard or Secure mode.
  A focused secure-connection popup owns CA import, fingerprint presentation
  and explicit approval.

`main.c` remains only the coordinator for module initialization and narrow
adapters. Long-lived trust data must prefer PSRAM with internal-RAM fallback.

## Server provisioning

PrinterHMI cannot install a reverse proxy or generate certificates on a
printer host. Secure mode therefore requires a separately configured local
HTTPS/WSS endpoint, such as Nginx terminating TLS in front of Moonraker.
PrinterHMI must not accept an unverified certificate automatically.

## Validation

Validate each profile with and without an API key: HTTP regression behavior,
HTTPS API, WSS subscription, G-code dispatch, file transfer, printer switch,
certificate rejection, no HTTP downgrade, restart, OTA and power-cycle.
